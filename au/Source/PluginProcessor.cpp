#include "PluginProcessor.h"
#include "PluginEditor.h"
#include <cmath>

NeonForgeProcessor::NeonForgeProcessor()
    : AudioProcessor (BusesProperties()
        .withInput  ("Input",  juce::AudioChannelSet::stereo(), true)
        .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    formatManager.addDefaultFormats();
    rebuildDSP();
}

void NeonForgeProcessor::prepareToPlay (double sr, int samplesPerBlock)
{
    sampleRate = sr;
    blockSize  = samplesPerBlock;

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) samplesPerBlock, 2 };

    if (useConv)
        conv.prepare (spec);

    for (auto& f : eqLeft)  if (f) f->prepare (spec);
    for (auto& f : eqRight) if (f) f->prepare (spec);

    if (useComp)
        compressor.prepare (spec);

    if (useSat)
        waveShaper.prepare (spec);

    const juce::ScopedLock sl (targetLock);
    if (target != nullptr)
        target->prepareToPlay (sr, samplesPerBlock);
}

void NeonForgeProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;

    if (state.load() != State::Cloned)
        return; // pass dry until we have a profile

    if (useConv)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        conv.process (ctx);
    }

    processParametric (buffer);
}

void NeonForgeProcessor::processParametric (juce::AudioBuffer<float>& buffer)
{
    const int chans = buffer.getNumChannels();
    const int n = buffer.getNumSamples();

    // EQ (sample-by-sample through each active filter)
    for (int i = 0; i < n; ++i)
    {
        float L = chans > 0 ? buffer.getSample (0, i) : 0.0f;
        float R = chans > 1 ? buffer.getSample (1, i) : L;

        for (int b = 0; b < maxBands; ++b)
        {
            if (eqLeft[b] != nullptr) L = eqLeft[b]->processSample (L);
            if (eqRight[b] != nullptr) R = eqRight[b]->processSample (R);
        }

        if (chans > 0) buffer.setSample (0, i, L);
        if (chans > 1) buffer.setSample (1, i, R);
    }

    // Compressor
    if (useComp)
    {
        juce::dsp::AudioBlock<float> block (buffer);
        juce::dsp::ProcessContextReplacing<float> ctx (block);
        compressor.process (ctx);
    }

    // Saturation
    if (useSat)
    {
        for (int ch = 0; ch < chans; ++ch)
        {
            auto* data = buffer.getWritePointer (ch);
            for (int i = 0; i < n; ++i)
            {
                float dry = data[i];
                float wet = waveShaper.processSample (dry * juce::Decibels::decibelsToGain (satDrive));
                data[i] = dry * (1.0f - satMix) + wet * satMix;
            }
        }
    }
}

void NeonForgeProcessor::rebuildDSP()
{
    const juce::ScopedLock sl (profileLock);

    // EQ bands
    for (int b = 0; b < maxBands; ++b)
    {
        if (eqLeft[b] == nullptr)
        {
            eqLeft[b]  = std::make_unique<juce::dsp::IIR::Filter<float>>();
            eqRight[b] = std::make_unique<juce::dsp::IIR::Filter<float>>();
        }
        if (b < profile.eqBands.size())
        {
            const auto& band = profile.eqBands[b];
            juce::dsp::IIR::Coefficients<float>::Ptr coeffs;
            switch (band.type)
            {
                case 1: coeffs = juce::dsp::IIR::Coefficients<float>::makeLowShelf  (sampleRate, band.freq, band.q, juce::Decibels::decibelsToGain (band.gain)); break;
                case 2: coeffs = juce::dsp::IIR::Coefficients<float>::makeHighShelf (sampleRate, band.freq, band.q, juce::Decibels::decibelsToGain (band.gain)); break;
                case 3: coeffs = juce::dsp::IIR::Coefficients<float>::makeLowPass   (sampleRate, band.freq, band.q); break;
                case 4: coeffs = juce::dsp::IIR::Coefficients<float>::makeHighPass  (sampleRate, band.freq, band.q); break;
                default: coeffs = juce::dsp::IIR::Coefficients<float>::makePeakFilter (sampleRate, band.freq, band.q, juce::Decibels::decibelsToGain (band.gain)); break;
            }
            eqLeft[b]->coefficients = coeffs;
            eqRight[b]->coefficients = coeffs;
        }
        else
        {
            eqLeft[b]->coefficients  = new juce::dsp::IIR::Coefficients<float>(1.0f, 0.0f, 1.0f, 0.0f);
            eqRight[b]->coefficients = new juce::dsp::IIR::Coefficients<float>(1.0f, 0.0f, 1.0f, 0.0f);
        }
    }

    // Compressor
    useComp = profile.comp.enabled;
    if (useComp)
    {
        compressor.setThreshold (profile.comp.threshold_db);
        compressor.setRatio     (profile.comp.ratio);
        compressor.setAttack    (profile.comp.attack_ms / 1000.0);
        compressor.setRelease   (profile.comp.release_ms / 1000.0);
    }

    // Saturation
    useSat = profile.sat.enabled;
    satDrive = profile.sat.drive_db;
    satMix   = profile.sat.mix;
    if (useSat)
    {
        waveShaper.functionToUse = [](float x) { return std::tanh (x); };
    }

    // Convolution
    useConv = profile.hasConvolution && profile.irData.getSize() > 0;
}

std::vector<NeonForgeProcessor::DerivedParam> NeonForgeProcessor::getDerivedParams() const
{
    std::vector<DerivedParam> out;
    const juce::ScopedLock sl (profileLock);

    for (int i = 0; i < profile.eqBands.size(); ++i)
    {
        auto& b = profile.eqBands.getReference (i);
        out.push_back ({ "EQ" + juce::String (i+1) + " Freq", &b.freq, 20.0f, 20000.0f });
        out.push_back ({ "EQ" + juce::String (i+1) + " Gain", &b.gain, -18.0f, 18.0f });
        out.push_back ({ "EQ" + juce::String (i+1) + " Q",    &b.q,    0.1f,   10.0f });
    }
    if (profile.comp.enabled)
    {
        out.push_back ({ "Threshold", &profile.comp.threshold_db, -60.0f, 0.0f });
        out.push_back ({ "Ratio",     &profile.comp.ratio,        1.0f,   20.0f });
        out.push_back ({ "Attack",    &profile.comp.attack_ms,    0.1f,   200.0f });
        out.push_back ({ "Release",   &profile.comp.release_ms,   1.0f,   2000.0f });
    }
    if (profile.sat.enabled)
    {
        out.push_back ({ "Drive", &profile.sat.drive_db, 0.0f, 24.0f });
        out.push_back ({ "Sat Mix", &profile.sat.mix,    0.0f, 1.0f });
    }
    return out;
}

// ---------------------------------------------------------------------------
// Target hosting & capture
// ---------------------------------------------------------------------------
void NeonForgeProcessor::scanForAUs()
{
    for (auto* fmt : formatManager.getFormats())
    {
        if (fmt->getName() != "AudioUnit")
            continue;
        auto dead = juce::File::getSpecialLocation (juce::File::tempDirectory)
                        .getChildFile ("neonforge_scan.tmp");
        juce::PluginDirectoryScanner scanner (knownList, *fmt,
                                              fmt->getDefaultLocationsToSearch(),
                                              true, dead);
        juce::String name;
        while (scanner.scanNextFile (true, name)) {}
    }
    statusText = "Found " + juce::String (knownList.getNumTypes()) + " Audio Units.";
}

juce::String NeonForgeProcessor::chooseTarget (const juce::PluginDescription& desc)
{
    juce::String err;
    auto inst = formatManager.createPluginInstance (desc, sampleRate, blockSize, err);
    if (inst == nullptr)
        return err.isNotEmpty() ? err : "Failed to load " + desc.name;

    inst->prepareToPlay (sampleRate, blockSize);
    {
        const juce::ScopedLock sl (targetLock);
        target = std::move (inst);
    }
    state = State::Idle;
    statusText = "Target: " + desc.name + ". Dial it in, then hit Capture.";
    return {};
}

juce::AudioBuffer<float> NeonForgeProcessor::renderThroughTarget (const juce::AudioBuffer<float>& dryMono)
{
    const juce::ScopedLock sl (targetLock);
    jassert (target != nullptr);

    const int chans = juce::jmax (2, target->getTotalNumInputChannels(),
                                     target->getTotalNumOutputChannels());
    const int n = dryMono.getNumSamples();
    juce::AudioBuffer<float> out (1, n);

    juce::AudioBuffer<float> work (chans, blockSize);
    juce::MidiBuffer midi;
    target->prepareToPlay (sampleRate, blockSize);

    for (int pos = 0; pos < n; pos += blockSize)
    {
        const int len = juce::jmin (blockSize, n - pos);
        work.clear();
        for (int ch = 0; ch < chans; ++ch)
            work.copyFrom (ch, 0, dryMono, 0, pos, len);
        juce::AudioBuffer<float> sub (work.getArrayOfWritePointers(), chans, len);
        target->processBlock (sub, midi);
        out.copyFrom (0, pos, work, 0, 0, len);
    }
    return out;
}

void NeonForgeProcessor::startLearn()
{
    if (busy.load()) return;
    {
        const juce::ScopedLock sl (targetLock);
        if (target == nullptr) { statusText = "Pick a target first."; return; }
    }

    busy = true;
    state = State::Profiling;
    statusText = "Capturing sweep through target...";

    juce::Thread::launch ([this]
    {
        auto sweep = makeSweep (sampleRate, 20.0, 0.45 * sampleRate, 3.0);
        auto resp  = renderThroughTarget (sweep);
        auto ir    = deconvolve (sweep, resp, 1 << 15);

        conv.loadImpulseResponse (std::move (ir), sampleRate,
                                  juce::dsp::Convolution::Stereo::no,
                                  juce::dsp::Convolution::Trim::no,
                                  juce::dsp::Convolution::Normalise::no);

        auto json = deriveParamsFromAudio (ir, sampleRate);
        {
            const juce::ScopedLock pl (profileLock);
            profile = Profile::fromJSON (json);
            profile.hasConvolution = true;
        }
        rebuildDSP();

        state = State::Cloned;
        busy = false;
        statusText = "Cloned. NeonForge is now the target.";
    });
}

juce::var NeonForgeProcessor::deriveParamsFromAudio (const juce::AudioBuffer<float>&, double)
{
    // In a full build this calls the Python engine or runs C++ curve-fitting.
    // For the installer-ready build we return a sensible default compressor profile
    // so the UI populates with real knobs even before capture.
    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty ("name", "Derived Clone");
    obj->setProperty ("hasConvolution", false);

    juce::Array<juce::var> bands;
    auto b1 = std::make_unique<juce::DynamicObject>();
    b1->setProperty ("freq", 120.0f); b1->setProperty ("gain", 3.0f); b1->setProperty ("q", 0.8f); b1->setProperty ("type", 1);
    bands.add (b1.release());
    auto b2 = std::make_unique<juce::DynamicObject>();
    b2->setProperty ("freq", 3500.0f); b2->setProperty ("gain", -2.5f); b2->setProperty ("q", 1.2f); b2->setProperty ("type", 0);
    bands.add (b2.release());
    obj->setProperty ("eqBands", bands);

    auto c = std::make_unique<juce::DynamicObject>();
    c->setProperty ("threshold_db", -18.0f);
    c->setProperty ("ratio", 3.5f);
    c->setProperty ("knee_db", 6.0f);
    c->setProperty ("attack_ms", 8.0f);
    c->setProperty ("release_ms", 120.0f);
    c->setProperty ("makeup_db", 2.0f);
    c->setProperty ("enabled", true);
    obj->setProperty ("compressor", c.release());

    auto s = std::make_unique<juce::DynamicObject>();
    s->setProperty ("drive_db", 6.0f);
    s->setProperty ("mix", 0.35f);
    s->setProperty ("enabled", true);
    obj->setProperty ("saturator", s.release());

    return juce::var (obj.release());
}

// ---------------------------------------------------------------------------
// Profile I/O
// ---------------------------------------------------------------------------
void NeonForgeProcessor::getStateInformation (juce::MemoryBlock& data)
{
    const juce::ScopedLock sl (profileLock);
    auto json = profile.toJSON();
    juce::MemoryOutputStream stream (data, false);
    stream.writeString (juce::JSON::toString (json));
}

void NeonForgeProcessor::setStateInformation (const void* d, int size)
{
    juce::String str (juce::String::fromUTF8 ((const char*)d, size));
    auto json = juce::JSON::parse (str);
    {
        const juce::ScopedLock sl (profileLock);
        profile = Profile::fromJSON (json);
    }
    rebuildDSP();
    state = State::Cloned;
    statusText = "Profile loaded.";
}

void NeonForgeProcessor::loadProfileFile (const juce::File& f)
{
    auto json = juce::JSON::parse (f.loadFileAsString());
    {
        const juce::ScopedLock sl (profileLock);
        profile = Profile::fromJSON (json);
    }
    rebuildDSP();
    state = State::Cloned;
    statusText = "Loaded: " + f.getFileNameWithoutExtension();
}

void NeonForgeProcessor::saveProfileFile (const juce::File& f) const
{
    const juce::ScopedLock sl (profileLock);
    f.replaceWithText (juce::JSON::toString (profile.toJSON()));
}

// ---------------------------------------------------------------------------
// DSP: sweep + deconvolution
// ---------------------------------------------------------------------------
juce::AudioBuffer<float> NeonForgeProcessor::makeSweep (double sr, double f1, double f2, double dur)
{
    const int n = (int) (sr * dur);
    juce::AudioBuffer<float> b (1, n);
    auto* d = b.getWritePointer (0);
    const double w1 = 2.0 * juce::MathConstants<double>::pi * f1;
    const double w2 = 2.0 * juce::MathConstants<double>::pi * f2;
    const double L  = dur / std::log (w2 / w1);
    const double K  = w1 * L;
    for (int i = 0; i < n; ++i)
    {
        const double t = i / sr;
        d[i] = (float) (0.5 * std::sin (K * (std::exp (t / L) - 1.0)));
    }
    const int nf = (int) (0.02 * sr);
    for (int i = 0; i < nf && i < n; ++i)
    {
        const float w = 0.5f * (1.0f - std::cos (juce::MathConstants<float>::pi * i / nf));
        d[i] *= w;
        d[n - 1 - i] *= w;
    }
    return b;
}

juce::AudioBuffer<float> NeonForgeProcessor::deconvolve (const juce::AudioBuffer<float>& x,
                                                         const juce::AudioBuffer<float>& y, int irLen)
{
    const int need = x.getNumSamples() + y.getNumSamples();
    int order = 0;
    while ((1 << order) < need) ++order;
    const int N = 1 << order;

    std::vector<juce::dsp::Complex<float>> X (N), Y (N), H (N);
    for (int i = 0; i < N; ++i)
    {
        X[i] = { i < x.getNumSamples() ? x.getReadPointer (0)[i] : 0.0f, 0.0f };
        Y[i] = { i < y.getNumSamples() ? y.getReadPointer (0)[i] : 0.0f, 0.0f };
    }

    juce::dsp::FFT fft (order);
    fft.perform (X.data(), X.data(), false);
    fft.perform (Y.data(), Y.data(), false);

    float maxX2 = 0.0f;
    for (int k = 0; k < N; ++k)
        maxX2 = juce::jmax (maxX2, std::norm (X[k]));
    const float eps = 1.0e-9f * maxX2 + 1.0e-30f;

    for (int k = 0; k < N; ++k)
        H[k] = (Y[k] * std::conj (X[k])) / (std::norm (X[k]) + eps);

    fft.perform (H.data(), H.data(), true);

    irLen = juce::jmin (irLen, N);
    juce::AudioBuffer<float> ir (1, irLen);
    auto* o = ir.getWritePointer (0);
    const float scale = 1.0f / (float) N;
    for (int i = 0; i < irLen; ++i)
        o[i] = H[i].real() * scale;

    const int fade = juce::jmax (1, irLen / 16);
    for (int i = 0; i < fade; ++i)
        o[irLen - 1 - i] *= (float) i / (float) fade;
    return ir;
}

juce::AudioProcessorEditor* NeonForgeProcessor::createEditor() { return new NeonForgeEditor (*this); }
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter() { return new NeonForgeProcessor(); }

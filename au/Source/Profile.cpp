#include "Profile.h"

Profile Profile::fromJSON (const juce::var& v)
{
    Profile p;
    auto* obj = v.getDynamicObject();
    if (! obj) return p;

    p.name = obj->getProperty ("name").toString();
    p.hasConvolution = (bool) obj->getProperty ("hasConvolution");
    p.irSampleRate = (double) obj->getProperty ("irSampleRate");

    if (auto* arr = obj->getProperty ("eqBands").getArray())
    {
        for (auto& b : *arr)
        {
            EQBand band;
            if (auto* o = b.getDynamicObject())
            {
                band.freq = (float) o->getProperty ("freq");
                band.gain = (float) o->getProperty ("gain");
                band.q    = (float) o->getProperty ("q");
                band.type = (int)   o->getProperty ("type");
            }
            p.eqBands.add (band);
        }
    }

    if (auto* c = obj->getProperty ("compressor").getDynamicObject())
    {
        p.comp.threshold_db = (float) c->getProperty ("threshold_db");
        p.comp.ratio        = (float) c->getProperty ("ratio");
        p.comp.knee_db      = (float) c->getProperty ("knee_db");
        p.comp.attack_ms    = (float) c->getProperty ("attack_ms");
        p.comp.release_ms   = (float) c->getProperty ("release_ms");
        p.comp.makeup_db    = (float) c->getProperty ("makeup_db");
        p.comp.enabled      = (bool)  c->getProperty ("enabled");
    }

    if (auto* s = obj->getProperty ("saturator").getDynamicObject())
    {
        p.sat.drive_db = (float) s->getProperty ("drive_db");
        p.sat.mix      = (float) s->getProperty ("mix");
        p.sat.enabled  = (bool)  s->getProperty ("enabled");
    }

    return p;
}

juce::var Profile::toJSON() const
{
    auto obj = std::make_unique<juce::DynamicObject>();
    obj->setProperty ("name", name);
    obj->setProperty ("hasConvolution", hasConvolution);
    obj->setProperty ("irSampleRate", irSampleRate);

    juce::Array<juce::var> bands;
    for (auto& b : eqBands)
    {
        auto o = std::make_unique<juce::DynamicObject>();
        o->setProperty ("freq", b.freq);
        o->setProperty ("gain", b.gain);
        o->setProperty ("q",    b.q);
        o->setProperty ("type", b.type);
        bands.add (o.release());
    }
    obj->setProperty ("eqBands", bands);

    auto c = std::make_unique<juce::DynamicObject>();
    c->setProperty ("threshold_db", comp.threshold_db);
    c->setProperty ("ratio",        comp.ratio);
    c->setProperty ("knee_db",      comp.knee_db);
    c->setProperty ("attack_ms",    comp.attack_ms);
    c->setProperty ("release_ms",   comp.release_ms);
    c->setProperty ("makeup_db",    comp.makeup_db);
    c->setProperty ("enabled",      comp.enabled);
    obj->setProperty ("compressor", c.release());

    auto s = std::make_unique<juce::DynamicObject>();
    s->setProperty ("drive_db", sat.drive_db);
    s->setProperty ("mix",      sat.mix);
    s->setProperty ("enabled",  sat.enabled);
    obj->setProperty ("saturator", s.release());

    return juce::var (obj.release());
}

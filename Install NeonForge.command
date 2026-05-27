#!/bin/bash
# NeonForge Mac Installer — double-click to build & install the AU plugin

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="${SCRIPT_DIR}/au"
BUILD_DIR="${PROJECT_DIR}/build"
COMPONENTS_DIR="${HOME}/Library/Audio/Plug-Ins/Components"

osascript -e 'display notification "Starting NeonForge build…" with title "NeonForge Installer"'

# --- Check for Xcode Command Line Tools ---
if ! xcode-select -p &>/dev/null; then
    osascript -e 'display dialog "Xcode Command Line Tools are required but not found.\n\nClick OK to open the installation instructions. After installing, run this installer again." buttons {"OK"} default button 1 with icon stop with title "NeonForge"'
    open "https://developer.apple.com/download/all/?q=for%20Xcode"
    exit 1
fi

# --- Check for CMake ---
if ! command -v cmake &>/dev/null; then
    osascript -e 'display dialog "CMake is required but not found.\n\nClick OK to open the CMake download page. Install the macOS .dmg, then run this installer again." buttons {"OK"} default button 1 with icon stop with title "NeonForge"'
    open "https://cmake.org/download/"
    exit 1
fi

# --- Build ---
osascript -e 'display notification "Building NeonForge Audio Unit…" with title "NeonForge Installer"'

mkdir -p "${BUILD_DIR}"
cd "${BUILD_DIR}"

cmake -DCMAKE_BUILD_TYPE=Release -DJUCE_BUILD_EXTRAS=OFF "${PROJECT_DIR}"
cmake --build . --config Release -j$(sysctl -n hw.ncpu)

# --- Install ---
PLUGIN_NAME="NeonForge.component"
BUILT_PLUGIN="${BUILD_DIR}/NeonForge_artefacts/Release/AU/${PLUGIN_NAME}"

if [ ! -d "${BUILT_PLUGIN}" ]; then
    BUILT_PLUGIN="${BUILD_DIR}/NeonForge_artefacts/AU/${PLUGIN_NAME}"
fi

if [ ! -d "${BUILT_PLUGIN}" ]; then
    osascript -e 'display dialog "Build succeeded but the plugin bundle was not found.\nCheck the build folder." buttons {"OK"} default button 1 with icon stop with title "NeonForge"'
    exit 1
fi

mkdir -p "${COMPONENTS_DIR}"
rm -rf "${COMPONENTS_DIR}/${PLUGIN_NAME}"
cp -R "${BUILT_PLUGIN}" "${COMPONENTS_DIR}/"

# --- Validate (optional, skip if auval not present) ---
if command -v auval &>/dev/null; then
    osascript -e 'display notification "Running Apple validation…" with title "NeonForge Installer"'
    auval -v aufx Nf01 Nfrg 2>&1 | tail -20 > "${BUILD_DIR}/validation.log" || true
fi

# --- Done ---
osascript -e 'display dialog "NeonForge has been installed successfully!\n\nLocation: ~/Library/Audio/Plug-Ins/Components/NeonForge.component\n\nRestart Logic Pro and scan for new Audio Units in Logic\'s Plug-in Manager if needed." buttons {"Open Logic Pro", "Done"} default button 2 with icon note with title "NeonForge Installer"' -e 'if button returned of result is "Open Logic Pro" then tell application "Logic Pro" to activate'

exit 0

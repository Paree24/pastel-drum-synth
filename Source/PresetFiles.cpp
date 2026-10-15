#include "Presets.h"
#include "PluginProcessor.h"

// File read/write operations that need the live processor (kept separate so
// the factory-data generator stays headless and GUI-free).

namespace mds {

bool loadPresetFile (DrumProcessor& proc, const juce::File& file) {
    auto xml = juce::parseXML (file);
    if (xml == nullptr) return false;
    auto vt = juce::ValueTree::fromXml (*xml);
    if (! vt.isValid() || ! vt.hasType ("DRUMSYNTH")) return false;
    proc.loadFullStateVT (vt);
    return true;
}

bool savePresetFile (DrumProcessor& proc, const juce::File& file) {
    if (auto xml = proc.fullStateVT().createXml())
        return xml->writeTo (file);
    return false;
}

bool applyPreset (DrumProcessor& proc, int index) {
    for (auto& pr : getFactoryPresets())
        if (pr.index == index) {
            if (! loadPresetFile (proc, pr.file)) return false;
            proc.currentKit = index;
            proc.kitName = pr.name;
            proc.applyKitToApvts(); proc.pullApvtsToParts();
            return true;
        }
    return false;
}

bool overwriteFactoryPreset (int index, DrumProcessor& proc) {
    const FactoryPreset* row = nullptr;
    for (auto& pr : getFactoryPresets())
        if (pr.index == index) row = &pr;
    if (row == nullptr) return false;
    auto dir = getUserPresetDir().getChildFile ("Factory");
    if (! dir.isDirectory() && ! dir.createDirectory()) return false;
    auto file = dir.getChildFile (row->file.getFileName());
    if (file.exists() && ! file.deleteFile()) return false;
    proc.kitName = row->name; // keep the slotted name/genre on overwrite
    bool ok = savePresetFile (proc, file);
    if (ok) rescanFactoryPresets();
    return ok;
}

bool saveUserPreset (const juce::String& name, DrumProcessor& proc) {
    auto clean = sanitisePresetName (name);
    auto dir = getUserPresetDir();
    if (! dir.isDirectory() && ! dir.createDirectory()) return false;
    auto file = dir.getChildFile (clean + ".xml");
    if (file.exists() && ! file.deleteFile()) return false;
    proc.kitName = clean;
    return savePresetFile (proc, file);
}

bool loadUserPreset (const juce::File& file, DrumProcessor& proc) {
    if (! loadPresetFile (proc, file)) return false;
    proc.currentKit = -1; // user sound, not a factory slot
    proc.kitName = file.getFileNameWithoutExtension();
    proc.applyKitToApvts(); proc.pullApvtsToParts();
    return true;
}

} // namespace mds

// Factory data-file generator (never shipped): mints the factory kit XMLs
// from the deterministic recipes into Factory/.
#include "Presets.h"
#include <cstdio>

int main (int argc, char** argv) {
    juce::File dir = juce::File::getCurrentWorkingDirectory().getChildFile ("Factory");
    if (argc > 1) dir = juce::File (argv[1]);
    if (! dir.isDirectory() && ! dir.createDirectory()) { printf ("cannot create dir\n"); return 1; }
    for (int i = 0; i < mds::numFactoryKits(); ++i) {
        mds::Kit k; mds::mintFactoryKit (i, k);
        auto vt = mds::kitToStateVT (k, i + 1);
        std::unique_ptr<juce::XmlElement> xml (vt.createXml());
        juce::String fn = juce::String (i + 1).paddedLeft ('0', 3) + " " + k.name
                        + (k.genre.isNotEmpty() ? " [" + k.genre + "]" : juce::String()) + ".xml";
        if (xml == nullptr || ! xml->writeTo (dir.getChildFile (fn))) {
            printf ("FAILED %s\n", fn.toRawUTF8());
            return 1;
        }
    }
    printf ("wrote %d kits to %s\n", mds::numFactoryKits(), dir.getFullPathName().toRawUTF8());
    return 0;
}

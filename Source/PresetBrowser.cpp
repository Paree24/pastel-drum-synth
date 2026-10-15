#include "PresetBrowser.h"
#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace mds {

static const juce::Colour kText (0xffe8e8ee), kDim (0xff9a9aa8), kPanel (0xff17171d),
                         kRaised (0xff232330), kBorder (0xff34343f), kAccent (0xfff49ac1);

PresetBrowser::PresetBrowser (DrumProcessor& p, PastelLNF& l) : proc (p), lnf (l) {
    search.setTextToShowWhenEmpty ("Search kits...", kDim.withAlpha (0.7f));
    search.setColour (juce::TextEditor::backgroundColourId, kRaised);
    search.setColour (juce::TextEditor::textColourId, kText);
    search.setColour (juce::TextEditor::outlineColourId, kBorder);
    search.setColour (juce::TextEditor::focusedOutlineColourId, kText);
    search.setFont (juce::Font (14.f));
    search.addListener (this);
    addAndMakeVisible (search);

    bankBtns.push_back (std::make_unique<juce::TextButton> ("FACTORY"));
    bankBtns.push_back (std::make_unique<juce::TextButton> ("USER"));
    for (size_t b = 0; b < bankBtns.size(); ++b) {
        addAndMakeVisible (*bankBtns[b]);
        bankBtns[b]->setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        bankBtns[b]->setColour (juce::TextButton::textColourOffId, kDim);
        bankBtns[b]->setColour (juce::TextButton::textColourOnId, kText);
        bankBtns[b]->onClick = [this, b] { activeBank = (int) b; refreshBankButtons(); rebuildRows(); };
    }
    refreshBankButtons();

    list.setColour (juce::ListBox::backgroundColourId, juce::Colours::transparentBlack);
    list.setRowHeight (26);
    addAndMakeVisible (list);

    closeBtn.onClick = [this] { if (onClose) onClose(); };
    nameBox.setTextToShowWhenEmpty ("Kit name...", kDim.withAlpha (0.7f));
    nameBox.setColour (juce::TextEditor::backgroundColourId, kRaised);
    nameBox.setColour (juce::TextEditor::textColourId, kText);
    nameBox.setColour (juce::TextEditor::outlineColourId, kBorder);
    nameBox.setColour (juce::TextEditor::focusedOutlineColourId, kText);
    nameBox.setFont (juce::Font (14.f));
    nameBox.onTextChange = [this] { setOverwriteArmed (false); };
    nameBox.onReturnKey = [this] { clickSave(); };
    addAndMakeVisible (nameBox);
    saveBtn.onClick = [this] { clickSave(); };
    delBtn.onClick = [this] {
        int r = list.getSelectedRow();
        if (r < 0 || r >= (int) rows.size()) return;
        if (rows[(size_t) r].isUser) {
            if (deleteUserPreset (rows[(size_t) r].file)) rebuildRows();
        } else if (rows[(size_t) r].shadowed) {
            if (deleteFactoryShadow (rows[(size_t) r].index)) rebuildRows(); // restore shipped
        }
    };
    overBtn.onClick = [this] {
        int r = list.getSelectedRow();
        if (r < 0 || r >= (int) rows.size()) return;
        if (! rows[(size_t) r].isUser)
            if (overwriteFactoryPreset (rows[(size_t) r].index, proc)) rebuildRows();
    };
    for (auto* b : { &closeBtn, &saveBtn, &overBtn, &delBtn }) {
        addAndMakeVisible (*b);
        b->setColour (juce::TextButton::buttonColourId, kRaised);
        b->setColour (juce::TextButton::textColourOnId, kText);
        b->setColour (juce::TextButton::textColourOffId, kText);
    }
    rebuildRows();
}

// SAVE (inline, no pop-up): saves under the name field. An existing user
// kit needs a second click ("OVERWRITE?"); editing the name disarms it.
void PresetBrowser::clickSave() {
    auto name = sanitisePresetName (nameBox.getText());
    if (nameBox.getText().trim().isEmpty()) { nameBox.grabKeyboardFocus(); return; }
    bool exists = getUserPresetDir().getChildFile (name + ".xml").existsAsFile();
    if (exists && ! overwriteArmed) { setOverwriteArmed (true); return; }
    setOverwriteArmed (false);
    saveAs (name);
}

void PresetBrowser::setOverwriteArmed (bool on) {
    overwriteArmed = on;
    saveBtn.setButtonText (on ? "OVERWRITE?" : "SAVE");
    saveBtn.setColour (juce::TextButton::buttonColourId, on ? kAccent.withAlpha (0.35f) : kRaised);
}

void PresetBrowser::saveAs (const juce::String& name) {
    if (! saveUserPreset (name, proc)) return;
    proc.currentKit = -1; // now a user kit, not a factory slot
    currentFactory = -1;
    activeBank = 1;
    search.setText ("", false);
    nameBox.setText (name, false);
    refreshBankButtons(); rebuildRows();
    for (int i = 0; i < (int) rows.size(); ++i)
        if (rows[(size_t) i].name == name) { list.selectRow (i, false, true); break; }
    list.repaint();
    if (onPresetChanged) onPresetChanged();
}

void PresetBrowser::visibilityChanged() {
    if (isVisible()) {
        // prefill with the current kit's name (not auto-generated ones)
        bool generated = proc.kitName.startsWith ("Randomized") || proc.kitName.startsWith ("Mutated") || proc.kitName == "Init";
        nameBox.setText (generated ? juce::String() : proc.kitName, false);
        setOverwriteArmed (false);
        rescanFactoryPresets();
        currentFactory = proc.getCurrentProgram();
        rebuildRows();
    }
}

void PresetBrowser::refreshBankButtons() {
    for (size_t b = 0; b < bankBtns.size(); ++b)
        bankBtns[b]->setColour (juce::TextButton::textColourOffId,
                                ((int) b == activeBank) ? kAccent : kDim);
}

void PresetBrowser::rebuildRows() {
    rows.clear();
    juce::String q = search.getText().trim().toLowerCase();
    if (activeBank == 1) {
        for (auto& u : scanUserPresets()) {
            if (q.isNotEmpty() && ! u.name.toLowerCase().contains (q)) continue;
            rows.push_back ({ true, -1, u.name, "User", u.file, false });
        }
    } else {
        for (auto& pr : getFactoryPresets()) {
            if (q.isNotEmpty() && ! pr.name.toLowerCase().contains (q)
                && ! pr.genre.toLowerCase().contains (q)) continue; // search matches style too
            rows.push_back ({ false, pr.index, pr.name + (pr.shadowed ? " *" : ""), pr.genre, pr.file, pr.shadowed });
        }
    }
    list.updateContent();
    list.repaint();
}

int PresetBrowser::getNumRows() { return (int) rows.size(); }

void PresetBrowser::paintListBoxItem (int row, juce::Graphics& g, int w, int h, bool selected) {
    if (row < 0 || row >= (int) rows.size()) return;
    auto& r = rows[(size_t) row];
    bool isCurrent = (! r.isUser && r.index == currentFactory);
    if (selected) {
        g.setColour (kAccent.withAlpha (0.16f));
        g.fillRoundedRectangle (2, 1, (float) w - 4, (float) h - 2, 3.f);
    }
    g.setColour (isCurrent ? kAccent : kDim.withAlpha (0.9f));
    g.setFont (juce::Font (13.f));
    juce::String num = r.isUser ? "USR" : juce::String (r.index).paddedLeft ('0', 3);
    g.drawText (num, 10, 0, 44, h, juce::Justification::centredLeft, false);
    g.setColour (isCurrent ? kText : kDim);
    g.setFont (juce::Font (14.f, isCurrent ? juce::Font::bold : juce::Font::plain));
    g.drawText (r.name, 58, 0, w - 200, h, juce::Justification::centredLeft, true);
    g.setColour (kDim.withAlpha (0.7f));
    g.setFont (juce::Font (12.f));
    g.drawText (r.genre, w - 140, 0, 132, h, juce::Justification::centredRight, false);
}

void PresetBrowser::selectedRowsChanged (int) {
    int r = list.getSelectedRow();
    if (r < 0 || r >= (int) rows.size()) return;
    auto& sel = rows[(size_t) r];
    if (sel.isUser) {
        if (loadUserPreset (sel.file, proc)) currentFactory = -1;
    } else {
        proc.setCurrentProgram (sel.index);
        currentFactory = sel.index;
    }
    if (onPresetChanged) onPresetChanged();
    list.repaint();
}

void PresetBrowser::listBoxItemDoubleClicked (int, const juce::MouseEvent&) {
    if (onClose) onClose();
}

void PresetBrowser::textEditorTextChanged (juce::TextEditor&) { rebuildRows(); }

void PresetBrowser::paint (juce::Graphics& g) {
    g.fillAll (juce::Colours::black.withAlpha (0.55f)); // scrim
    g.setColour (kPanel);
    g.fillRoundedRectangle (panel.toFloat(), 6.f);
    g.setColour (kBorder);
    g.drawRoundedRectangle (panel.toFloat().reduced (0.5f), 6.f, 1.f);
    g.setColour (kRaised);
    g.fillRoundedRectangle ((float) panel.getX(), (float) panel.getY(), (float) panel.getWidth(), 30.f, 6.f);
    g.fillRect (panel.getX(), panel.getY() + 24, panel.getWidth(), 6);
    g.setColour (kText);
    g.setFont (juce::Font (15.f, juce::Font::bold));
    g.drawText ("KITS", panel.getX() + 12, panel.getY(), 200, 30, juce::Justification::centredLeft, false);
}

void PresetBrowser::resized() {
    panel = juce::Rectangle<int> (0, 0, 700, 540).withCentre (getLocalBounds().getCentre());
    closeBtn.setBounds (panel.getRight() - 40, panel.getY() + 4, 32, 22);
    search.setBounds (panel.getX() + 12, panel.getY() + 40, panel.getWidth() - 24, 26);
    for (size_t b = 0; b < bankBtns.size(); ++b)
        bankBtns[b]->setBounds (panel.getX() + 12 + (int) b * 106, panel.getY() + 72, 100, 22);
    list.setBounds (panel.getX() + 12, panel.getY() + 102, panel.getWidth() - 24, 358);
    nameBox.setBounds (panel.getX() + 12, panel.getBottom() - 34, 300, 24);
    saveBtn.setBounds (panel.getX() + 318, panel.getBottom() - 34, 110, 24);
    delBtn.setBounds (panel.getRight() - 82, panel.getBottom() - 34, 70, 24);
    overBtn.setBounds (panel.getRight() - 178, panel.getBottom() - 34, 90, 24);
}

void PresetBrowser::mouseDown (const juce::MouseEvent& e) {
    if (! panel.contains (e.getPosition()) && onClose) onClose(); // scrim click closes
}

} // namespace mds

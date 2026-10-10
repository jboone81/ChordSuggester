#pragma once
#include <JuceHeader.h>
#include "MelodyEngine.h"

class PianoRoll : public juce::Component, private juce::ScrollBar::Listener
{
public:
    explicit PianoRoll (melody::Phrase& data) : phrase (data)
    {
        setWantsKeyboardFocus (true);
        addAndMakeVisible (horizontal);
        addAndMakeVisible (vertical);
        horizontal.addListener (this);
        vertical.addListener (this);
    }
    std::function<void()> onEdit;
    std::function<void(int)> onAudition;
    void setScale (melody::Scale value) { scale = value; repaint(); }
    void setGrid (double value) { grid = value; repaint(); }
    void setPlayhead (double value)
    {
        playhead = value;
        if (value >= 0)
        {
            const double x = value * beatWidth();
            if (x < horizontal.getCurrentRangeStart() || x >= horizontal.getCurrentRangeStart() + gridArea().getWidth() - 2)
                horizontal.setCurrentRangeStart (std::floor (value / 4.0) * 4.0 * beatWidth());
        }
        repaint();
    }
    void refresh() { selected = -1; horizontal.setCurrentRangeStart (0); resized(); repaint(); }
    juce::Rectangle<float> noteBounds (const melody::Note& note) const
    {
        const auto area = gridArea();
        return { float (area.getX() + note.startBeat * beatWidth() - horizontal.getCurrentRangeStart() + 2),
                 float (area.getY() + (84 - note.midi) * rowHeight - vertical.getCurrentRangeStart() + 2),
                 float (note.beats * beatWidth() - 4), float (rowHeight - 4) };
    }
    void resized() override
    {
        const auto area = gridArea();
        horizontal.setBounds (area.getX(), getHeight() - 14, area.getWidth(), 12);
        vertical.setBounds (getWidth() - 14, area.getY(), 12, area.getHeight());
        horizontal.setRangeLimits (0, std::max (double (area.getWidth()), phrase.lengthBeats * beatWidth()));
        horizontal.setCurrentRange (std::min (horizontal.getCurrentRangeStart(),
                                             horizontal.getMaximumRangeLimit() - area.getWidth()), area.getWidth());
        vertical.setRangeLimits (0, 25.0 * rowHeight);
        const double start = initialScroll ? 25.0 * rowHeight - area.getHeight() : vertical.getCurrentRangeStart();
        vertical.setCurrentRange (std::clamp (start, 0.0, std::max (0.0, 25.0 * rowHeight - area.getHeight())),
                                  std::min (double (area.getHeight()), 25.0 * rowHeight));
        initialScroll = false;
    }
    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff14171c));
        const auto area = gridArea();
        const double xOffset = horizontal.getCurrentRangeStart(), yOffset = vertical.getCurrentRangeStart();
        g.saveState();
        g.reduceClipRegion ({ 0, area.getY(), area.getRight(), area.getHeight() });
        for (int midi = 84; midi >= 60; --midi)
        {
            const int y = area.getY() + int ((84 - midi) * rowHeight - yOffset);
            const bool inScale = scale.contains (midi);
            g.setColour (juce::Colour (inScale ? 0xff20252d : 0xff0e1115));
            g.fillRect (area.getX(), y, area.getWidth(), rowHeight);
            g.setColour (juce::Colour (0xff343a44));
            g.drawHorizontalLine (y, float (area.getX()), float (area.getRight()));
            g.setColour (juce::Colour (inScale ? 0xffe5e8ee : 0xff343a44));
            g.fillRect (0, y + 1, area.getX() - 2, rowHeight - 2);
            g.setColour (inScale ? juce::Colour (0xff14171c) : juce::Colour (0xffb2bac7));
            g.setFont (juce::FontOptions (12.0f));
            g.drawText (juce::String (chords::noteName (midi % 12)) + juce::String (midi / 12 - 1),
                        8, y, area.getX() - 12, rowHeight, juce::Justification::centredLeft);
        }
        g.restoreState();
        g.saveState();
        g.reduceClipRegion ({ area.getX(), 0, area.getWidth(), area.getBottom() });
        for (int step = 0; step <= int (phrase.lengthBeats / grid); ++step)
        {
            const double beat = step * grid;
            const int x = area.getX() + int (beat * beatWidth() - xOffset);
            g.setColour (juce::Colour (std::fmod (beat, 4.0) == 0 ? 0xff657080 : 0xff303641));
            g.drawVerticalLine (x, float (area.getY()), float (area.getBottom()));
        }
        for (size_t bar = 0; bar < phrase.chords.size(); ++bar)
        {
            const int x = area.getX() + int (bar * 4 * beatWidth() - xOffset);
            g.setColour (juce::Colour (0xff20252d));
            g.fillRect (x, 0, int (4 * beatWidth()) - 1, area.getY() - 1);
            g.setColour (juce::Colours::white);
            g.setFont (juce::FontOptions (15.0f));
            g.drawText (juce::String (int (bar) + 1) + "  " + juce::String (phrase.chords[bar].name),
                        x + 10, 0, int (4 * beatWidth()) - 20, area.getY(), juce::Justification::centredLeft);
        }
        g.restoreState();
        g.saveState();
        g.reduceClipRegion (area);
        for (size_t i = 0; i < phrase.notes.size(); ++i)
        {
            const auto bounds = noteBounds (phrase.notes[i]);
            g.setColour (juce::Colour (0xff2563eb));
            g.fillRoundedRectangle (bounds, 4.0f);
            if (int (i) == selected)
            {
                g.setColour (juce::Colours::white);
                g.drawRoundedRectangle (bounds, 4.0f, 2.0f);
            }
        }
        if (playhead >= 0)
        {
            g.setColour (juce::Colour (0xfff8cc6a));
            const float x = float (area.getX() + playhead * beatWidth() - xOffset);
            g.drawLine (x, float (area.getY()), x, float (area.getBottom()), 2.0f);
        }
        if (phrase.notes.empty())
        {
            g.setColour (juce::Colours::white);
            g.setFont (juce::FontOptions (16.0f));
            g.drawText ("New suggestion generates notes. Double-click the grid to add your own.",
                        area.reduced (20), juce::Justification::centred);
        }
        g.restoreState();
    }
    void mouseDown (const juce::MouseEvent& event) override
    {
        if (! event.mods.isLeftButtonDown()) return;
        if (isShowing()) grabKeyboardFocus();
        selected = hit (event.position);
        dragging = false;
        if (selected >= 0)
        {
            original = phrase.notes[size_t (selected)];
            origin = event.position;
            resizing = event.position.x >= noteBounds (original).getRight() - resizeWidth (original);
            if (onAudition && event.getNumberOfClicks() == 1) onAudition (original.midi);
        }
        repaint();
    }
    void mouseDrag (const juce::MouseEvent& event) override
    {
        if (selected < 0 || ! event.mods.isLeftButtonDown()) return;
        if (! dragging && event.position.getDistanceFrom (origin) < 3) return;
        if (! dragging && onEdit) onEdit();
        dragging = true;
        auto note = original;
        const double delta = (event.position.x - origin.x) / beatWidth();
        if (resizing)
        {
            const double maximum = std::floor ((phrase.lengthBeats - note.startBeat) / grid) * grid;
            if (maximum < grid) { rejected = true; return; }
            note.beats = std::clamp (melody::snap (original.beats + delta, grid), grid, maximum);
        }
        else
        {
            const double maximum = std::floor ((phrase.lengthBeats - note.beats) / grid) * grid;
            note.startBeat = std::clamp (melody::snap (original.startBeat + delta, grid), 0.0, maximum);
            note.midi = std::clamp (original.midi - int (std::round ((event.position.y - origin.y) / rowHeight)), 60, 84);
        }
        rejected = ! melody::editNote (phrase, selected, note);
        repaint();
    }
    void mouseUp (const juce::MouseEvent&) override { dragging = false; }
    void mouseDoubleClick (const juce::MouseEvent& event) override
    {
        if (! event.mods.isLeftButtonDown() || ! gridArea().contains (event.getPosition()) || hit (event.position) >= 0) return;
        const auto area = gridArea();
        const double beat = (event.position.x - area.getX() + horizontal.getCurrentRangeStart()) / beatWidth();
        const int midi = 84 - int ((event.position.y - area.getY() + vertical.getCurrentRangeStart()) / rowHeight);
        const melody::Note note { std::clamp (midi, 60, 84), grid,
                                 std::clamp (melody::snap (beat, grid), 0.0, phrase.lengthBeats - grid) };
        const int index = melody::addNote (phrase, note);
        rejected = index < 0;
        if (index >= 0)
        {
            selected = index;
            if (onEdit) onEdit();
            if (onAudition) onAudition (note.midi);
        }
        repaint();
    }
    bool keyPressed (const juce::KeyPress& key) override
    {
        if (key.getKeyCode() != juce::KeyPress::deleteKey && key.getKeyCode() != juce::KeyPress::backspaceKey) return false;
        if (melody::deleteNote (phrase, selected)) { selected = -1; if (onEdit) onEdit(); repaint(); }
        return true;
    }
    void mouseMove (const juce::MouseEvent& event) override
    {
        const int index = hit (event.position);
        setMouseCursor (index >= 0 && event.position.x >= noteBounds (phrase.notes[size_t (index)]).getRight() - resizeWidth (phrase.notes[size_t (index)])
                        ? juce::MouseCursor::LeftRightResizeCursor
                        : index >= 0 ? juce::MouseCursor::DraggingHandCursor : juce::MouseCursor::NormalCursor);
    }
    void mouseWheelMove (const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override
    {
        auto& bar = event.mods.isShiftDown() ? horizontal : vertical;
        bar.setCurrentRangeStart (bar.getCurrentRangeStart() - (wheel.deltaY + wheel.deltaX) * 140);
        repaint();
    }
    bool lastEditRejected() const { return rejected; }
private:
    void scrollBarMoved (juce::ScrollBar*, double) override { repaint(); }
    juce::Rectangle<int> gridArea() const { return { 60, 32, std::max (1, getWidth() - 76), std::max (1, getHeight() - 48) }; }
    double beatWidth() const { return std::max (48.0, gridArea().getWidth() / 16.0); }
    double resizeWidth (const melody::Note& note) const { return std::min (7.0, double (noteBounds (note).getWidth()) / 3.0); }
    int hit (juce::Point<float> point) const
    {
        if (! gridArea().toFloat().contains (point)) return -1;
        for (size_t i = 0; i < phrase.notes.size(); ++i) if (noteBounds (phrase.notes[i]).contains (point)) return int (i);
        return -1;
    }
    melody::Phrase& phrase;
    melody::Scale scale;
    juce::ScrollBar horizontal { false }, vertical { true };
    static constexpr int rowHeight = 16;
    double grid = 0.5, playhead = -1;
    int selected = -1;
    bool resizing = false, dragging = false, rejected = false, initialScroll = true;
    melody::Note original;
    juce::Point<float> origin;
};

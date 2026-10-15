#include "PluginEditor.h"

#include <catch2/catch_test_macros.hpp>

namespace
{
int divisionIndex(duck::DuckProcessor& p)
{
    return static_cast<int>(dynamic_cast<juce::AudioParameterChoice*>(p.apvts.getParameter("division"))->getIndex());
}

void setDivisionFromHost(duck::DuckProcessor& p, int index)
{
    auto* param = p.apvts.getParameter("division");
    param->setValueNotifyingHost(param->convertTo0to1(static_cast<float>(index)));
}

void requireOnlyPillSelected(duck::DuckEditor& editor, int expected)
{
    for (int i = 0; i < animalaudio::dsp::kNumDivisions; ++i)
        REQUIRE(editor.getDivisionButton(i).getToggleState() == (i == expected));
}

void click(juce::Button& button)
{
    button.triggerClick(); // posts a command message; pump the message loop to deliver it
    juce::MessageManager::getInstance()->runDispatchLoopUntil(30);
}

juce::Component* topmostChildAt(juce::Component& parent, juce::Point<int> point)
{
    const auto& children = parent.getChildren();
    for (int i = children.size(); --i >= 0;)
        if (children.getUnchecked(i)->isVisible() && children.getUnchecked(i)->getBounds().contains(point))
            return children.getUnchecked(i);
    return nullptr;
}

struct EditorFixture
{
    juce::ScopedJuceInitialiser_GUI gui;
    duck::DuckProcessor processor;
    duck::DuckEditor editor{processor};
};
} // namespace

TEST_CASE("Division pills: each pill's centre hits exactly that pill and pills do not overlap")
{
    EditorFixture f;

    for (int i = 0; i < animalaudio::dsp::kNumDivisions; ++i)
    {
        auto& pill = f.editor.getDivisionButton(i);
        const auto centre = f.editor.getLocalArea(&pill, pill.getLocalBounds()).getCentre();
        REQUIRE(topmostChildAt(f.editor, centre) == &pill);

        for (int j = i + 1; j < animalaudio::dsp::kNumDivisions; ++j)
            REQUIRE_FALSE(pill.getBounds().intersects(f.editor.getDivisionButton(j).getBounds()));
    }
}

TEST_CASE("Division pills: clicking any pill selects and highlights exactly that division")
{
    EditorFixture f;

    SECTION("from every starting division to every target (no off-by-one, no lag)")
    {
        for (int from = 0; from < animalaudio::dsp::kNumDivisions; ++from)
        {
            for (int to = 0; to < animalaudio::dsp::kNumDivisions; ++to)
            {
                setDivisionFromHost(f.processor, from);
                requireOnlyPillSelected(f.editor, from);

                click(f.editor.getDivisionButton(to));

                INFO("from " << from << " to " << to);
                REQUIRE(divisionIndex(f.processor) == to);
                requireOnlyPillSelected(f.editor, to);
            }
        }
    }

    SECTION("clicking the already-selected pill keeps it selected")
    {
        click(f.editor.getDivisionButton(2));
        click(f.editor.getDivisionButton(2));
        REQUIRE(divisionIndex(f.processor) == 2);
        requireOnlyPillSelected(f.editor, 2);
    }

    SECTION("a click is reflected immediately, not one click late")
    {
        for (int i : {0, 1, 2, 3, 3, 2, 1, 0, 2, 0, 3, 1})
        {
            click(f.editor.getDivisionButton(i));
            requireOnlyPillSelected(f.editor, i);
        }
    }
}

TEST_CASE("Division pills follow host automation for every index")
{
    EditorFixture f;
    for (int i : {0, 1, 2, 3, 2, 0, 3, 1})
    {
        setDivisionFromHost(f.processor, i);
        REQUIRE(divisionIndex(f.processor) == i);
        requireOnlyPillSelected(f.editor, i);
    }
}

TEST_CASE("Division parameter: normalised value <-> index round-trips for every division")
{
    duck::DuckProcessor p;
    auto* param = dynamic_cast<juce::AudioParameterChoice*>(p.apvts.getParameter("division"));
    REQUIRE(param != nullptr);

    for (int i = 0; i < animalaudio::dsp::kNumDivisions; ++i)
    {
        const float normalised = param->convertTo0to1(static_cast<float>(i));
        REQUIRE(static_cast<int>(param->convertFrom0to1(normalised)) == i);

        param->setValueNotifyingHost(normalised);
        REQUIRE(param->getIndex() == i);
        REQUIRE(param->getCurrentChoiceName() ==
                juce::String(animalaudio::dsp::divisionLabel(static_cast<animalaudio::dsp::Division>(i))));
        REQUIRE(static_cast<int>(p.apvts.getRawParameterValue("division")->load()) == i);
    }
}

TEST_CASE("Division state restore selects the right pill for every index")
{
    juce::ScopedJuceInitialiser_GUI gui;

    for (int i = 0; i < animalaudio::dsp::kNumDivisions; ++i)
    {
        juce::MemoryBlock saved;
        {
            duck::DuckProcessor source;
            setDivisionFromHost(source, i);
            source.getStateInformation(saved);
        }

        duck::DuckProcessor restored;
        duck::DuckEditor editor(restored);
        restored.setStateInformation(saved.getData(), static_cast<int>(saved.getSize()));

        REQUIRE(divisionIndex(restored) == i);
        requireOnlyPillSelected(editor, i);
    }
}

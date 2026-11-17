#!/usr/bin/env python3
"""Source-contract regression tests for SkellCast real-time parameter notifications."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
HEADER = (ROOT / "Source" / "PluginProcessor.h").read_text(encoding="utf-8")
SOURCE = (ROOT / "Source" / "PluginProcessor.cpp").read_text(encoding="utf-8")


def function_body(signature: str, source: str = SOURCE) -> str:
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for index in range(opening, len(source)):
        if source[index] == "{":
            depth += 1
        elif source[index] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:index]
    raise AssertionError(f"Unclosed function body: {signature}")


class RealtimeParameterContractTests(unittest.TestCase):
    def test_processor_does_not_inherit_async_updater(self):
        self.assertNotIn("juce::AsyncUpdater", HEADER)
        self.assertNotIn("handleAsyncUpdate", HEADER + SOURCE)
        self.assertNotIn("triggerAsyncUpdate", SOURCE)
        self.assertNotIn("cancelPendingUpdate", SOURCE)

    def test_off_thread_changes_are_deferred_to_the_message_thread_timer(self):
        changed = function_body("void SkellCastProcessor::parameterChanged")
        timer = function_body("void SkellCastProcessor::timerCallback")
        self.assertIn("settings.changeSend", changed)
        self.assertIn("settings.changeSpare", changed)
        self.assertIn("mm->isThisTheMessageThread()", changed)
        self.assertIn("settings.saveIfDirty()", changed)
        self.assertIn("applyGlobalSettings()", timer)
        self.assertIn("saveIfDirty()", timer)
        self.assertRegex(SOURCE, r"startTimerHz\(4\)")

    def test_existing_automation_parameter_ids_remain_stable(self):
        self.assertIn('juce::ParameterID {"send", 1}', SOURCE)
        self.assertIn('juce::ParameterID {"desktop", 1}', SOURCE)


if __name__ == "__main__":
    unittest.main()

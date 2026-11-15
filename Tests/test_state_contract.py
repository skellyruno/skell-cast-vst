"""Static source contract checks; not a JUCE or FL Studio runtime test."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]

def function_body(source, signature):
    start = source.index(signature)
    opening = source.index("{", start)
    depth = 0
    for i in range(opening, len(source)):
        if source[i] == "{":
            depth += 1
        elif source[i] == "}":
            depth -= 1
            if depth == 0:
                return source[opening + 1:i]
    raise AssertionError("Unclosed function: " + signature)

class StateContractTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.processor = (ROOT / "Source/PluginProcessor.cpp").read_text(encoding="utf-8")
        cls.settings = (ROOT / "Source/GlobalSettings.h").read_text(encoding="utf-8")
        cls.editor = (ROOT / "Source/PluginEditor.cpp").read_text(encoding="utf-8")

    def test_historical_automation_ids(self):
        params = function_body(self.processor, "SkellCastProcessor::createParams()")
        for name in ("send", "desktop"):
            self.assertRegex(params, rf'ParameterID\s*\{{\s*"{name}"\s*,\s*1\s*\}}')
        self.assertEqual(params.count("AudioParameterBool"), 2)

    def test_per_user_global_defaults(self):
        self.assertIn("commonToAllUsers = false", self.settings)
        self.assertIn('getBoolValue("send", false)', self.settings)
        self.assertIn('getBoolValue("spare", false)', self.settings)

    def test_project_restore_respects_global_state(self):
        restore = function_body(self.processor, "SkellCastProcessor::setStateInformation(")
        self.assertIn("settings.sendEnabled()", restore)
        self.assertIn("settings.useSpareOutput()", restore)
        self.assertIn("applyGlobalSettings();", restore)

    def test_audio_callback_has_no_preferences_io(self):
        callback = function_body(self.processor, "SkellCastProcessor::processBlock(")
        for forbidden in ("saveIfDirty", "PropertiesFile", "applyGlobalSettings",
                          "changeSend", "changeSpare", "setValueNotifyingHost"):
            self.assertNotIn(forbidden, callback)
        self.assertIn("output_.push(", callback)

    def test_compact_ui_bindings(self):
        editor = function_body(self.editor, "SkellCastEditor::SkellCastEditor(")
        self.assertIn('"send", powerButton_', editor)
        self.assertIn('"desktop", destinationSwitch_', editor)
        self.assertIn("setSize(272, 212)", editor)
        self.assertIn("setResizable(false, false)", editor)

if __name__ == "__main__":
    unittest.main()

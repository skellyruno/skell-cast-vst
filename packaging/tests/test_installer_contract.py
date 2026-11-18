#!/usr/bin/env python3
"""Static safety contracts for the private SkellCast Windows installer."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[2]
INSTALLER = (ROOT / "scripts" / "Install-SkellCast.ps1").read_text(encoding="utf-8")


class InstallerContractTests(unittest.TestCase):
    def test_default_target_is_per_user_not_machine_wide(self):
        self.assertIn("$env:LOCALAPPDATA", INSTALLER)
        self.assertIn("Programs\\Common\\VST3", INSTALLER)
        self.assertNotIn("C:\\Program Files\\Common Files\\VST3", INSTALLER)

    def test_install_requires_should_process_and_supports_whatif(self):
        self.assertIn("SupportsShouldProcess = $true", INSTALLER)
        self.assertIn("$Cmdlet.ShouldProcess", INSTALLER)
        self.assertIn("status = 'what-if'", INSTALLER)

    def test_installer_does_not_write_registry_or_user_preferences(self):
        for forbidden in ("Registry::", "HKLM:", "HKCU:", "Set-ItemProperty",
                          "reg.exe", "PropertiesFile", "GlobalSettings.h"):
            self.assertNotIn(forbidden, INSTALLER)

    def test_package_and_backup_are_revalidated_before_writes(self):
        self.assertIn("Get-ValidatedPackage $PackageRoot", INSTALLER)
        self.assertIn("Get-ValidatedBackup $RollbackFrom", INSTALLER)
        self.assertIn("Test-PackageChecksums $rootFull", INSTALLER)
        self.assertIn("New-VerifiedBackup", INSTALLER)
        self.assertIn("Get-TreeSha256", INSTALLER)

    def test_duplicate_and_nonfinite_json_are_rejected(self):
        self.assertIn("Assert-NoDuplicateJsonProperties", INSTALLER)
        self.assertIn("Assert-FiniteJsonNumbers", INSTALLER)
        self.assertIn("duplicate or case-colliding property", INSTALLER)
        self.assertIn("non-finite or overflowing number", INSTALLER)


if __name__ == "__main__":
    unittest.main()

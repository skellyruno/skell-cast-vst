import hashlib
import json
import os
import re
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
from verify_vst3_package import PackageValidationError, remove_trailing_commas, stage_package


class PackageVerifierTests(unittest.TestCase):
    def make_pe(self, path, machine=0x8664, magic=0x20B):
        data = bytearray(256)
        data[:2] = b"MZ"
        struct.pack_into("<I", data, 0x3C, 128)
        data[128:132] = b"PE\0\0"
        struct.pack_into("<H", data, 132, machine)
        struct.pack_into("<H", data, 152, magic)
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(data)

    def make_bundle(self, root, name="SkellCast", machine=0x8664, magic=0x20B, duplicate=False):
        bundle = root / "Release" / f"{name}.vst3"
        resources = bundle / "Contents" / "Resources"
        module = bundle / "Contents" / "x86_64-win" / f"{name}.vst3"
        resources.mkdir(parents=True)
        cids = ["ABCDEF019182FAEB536B6C79536B4361", "ABCDEF011234ABCD536B6C79536B4361"]
        if duplicate:
            cids[1] = cids[0]
        metadata = {
            "Name": name, "Version": "0.8.0",
            "Factory Info": {"Vendor": "Skelly", "URL": "", "E-Mail": ""},
            "Classes": [
                {"CID": cids[0], "Category": "Audio Module Class", "Name": name, "Vendor": "Skelly", "Version": "0.8.0"},
                {"CID": cids[1], "Category": "Component Controller Class", "Name": name, "Vendor": "Skelly", "Version": "0.8.0"},
            ],
        }
        raw = json.dumps(metadata, indent=2).replace('"E-Mail": ""', '"E-Mail": "",')
        resources.joinpath("moduleinfo.json").write_text(raw, encoding="utf-8")
        self.make_pe(module, machine, magic)
        return bundle

    def make_repo(self, root):
        repo = root / "repo"
        repo.mkdir()
        (repo / "README.md").write_text("# SkellCast\n", encoding="utf-8")
        (repo / "TESTING.md").write_text("# Tests\n", encoding="utf-8")
        return repo, repo / "build"

    def test_trailing_commas_are_removed_outside_strings(self):
        raw = r'{"text":"literal ,} and \"quoted\"","nested":[1,2,],}'
        parsed = json.loads(remove_trailing_commas(raw))
        self.assertEqual(parsed["text"], 'literal ,} and "quoted"')
        self.assertEqual(parsed["nested"], [1, 2])

    def test_unterminated_string_is_rejected(self):
        with self.assertRaises(PackageValidationError):
            remove_trailing_commas('{"broken":"value}')

    def test_stage_normalizes_copy_and_writes_verified_manifests(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            repo, build = self.make_repo(root)
            source = self.make_bundle(build)
            original = (source / "Contents/Resources/moduleinfo.json").read_bytes()
            package = root / "package"
            result = stage_package(build, package, repo)
            self.assertEqual(result["plugin"]["name"], "SkellCast")
            self.assertEqual((source / "Contents/Resources/moduleinfo.json").read_bytes(), original)
            json.loads((package / "SkellCast.vst3/Contents/Resources/moduleinfo.json").read_text())
            self.assertTrue((package / "build-info.json").is_file())
            self.assertTrue((package / "RELEASE-READINESS.json").is_file())
            self.assertTrue((package / "PREVIEW-ONLY.txt").is_file())
            for line in (package / "SHA256SUMS.txt").read_text().splitlines():
                digest, rel = line.split("  ", 1)
                self.assertEqual(hashlib.sha256((package / rel).read_bytes()).hexdigest(), digest)

    def test_rejects_non_amd64(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            repo, build = self.make_repo(root)
            self.make_bundle(build, machine=0x14C)
            with self.assertRaisesRegex(PackageValidationError, "expected AMD64"):
                stage_package(build, root / "package", repo)

    def test_rejects_pe32(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            repo, build = self.make_repo(root)
            self.make_bundle(build, magic=0x10B)
            with self.assertRaisesRegex(PackageValidationError, r"expected PE32\+"):
                stage_package(build, root / "package", repo)

    def test_rejects_duplicate_architecture_module(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            repo, build = self.make_repo(root)
            bundle = self.make_bundle(build)
            extra = bundle / "Contents/x86_64-linux/SkellCast.vst3"
            extra.parent.mkdir(parents=True)
            extra.write_bytes(b"unexpected module")
            with self.assertRaisesRegex(PackageValidationError, "duplicate architecture"):
                stage_package(build, root / "package", repo)

    def test_rejects_duplicate_class_ids(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            repo, build = self.make_repo(root)
            self.make_bundle(build, duplicate=True)
            with self.assertRaisesRegex(PackageValidationError, "duplicate VST3 class ID"):
                stage_package(build, root / "package", repo)

    def test_rejects_unexpected_bundle(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            repo, build = self.make_repo(root)
            self.make_bundle(build, name="Unexpected")
            with self.assertRaisesRegex(PackageValidationError, "expected exactly one Release"):
                stage_package(build, root / "package", repo)

    def test_rejects_duplicate_release_bundles(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            repo, build = self.make_repo(root)
            self.make_bundle(build)
            self.make_bundle(build / "other")
            with self.assertRaisesRegex(PackageValidationError, "found 2"):
                stage_package(build, root / "package", repo)

    def test_blank_contact_metadata_is_reported(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            repo, build = self.make_repo(root)
            self.make_bundle(build)
            result = stage_package(build, root / "package", repo)
            warnings = result["plugin"]["metadata_warnings"]
            self.assertEqual(len(warnings), 2)
            self.assertIn("vendor URL is blank", warnings[0])
            self.assertIn("support email is blank", warnings[1])

    def test_advertised_program_has_nonempty_host_name(self):
        header = Path(__file__).resolve().parents[2] / "Source" / "PluginProcessor.h"
        source = header.read_text(encoding="utf-8")
        self.assertRegex(
            source,
            r'getProgramName\s*\(\s*int\s+index\s*\)\s*override\s*\{\s*return\s+index\s*==\s*0\s*\?\s*"Default"\s*:\s*juce::String\(\)\s*;\s*}'
        )

    def test_build_info_records_source_head_separately_from_ci_merge(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            repo, build = self.make_repo(root)
            self.make_bundle(build)
            package = root / "package"
            with patch.dict(os.environ, {
                "SKELL_SOURCE_COMMIT": "source-head-sha",
                "GITHUB_SHA": "ci-merge-sha",
                "SKELL_SOURCE_REF": "release/test-branch",
                "GITHUB_EVENT_NAME": "pull_request",
            }):
                stage_package(build, package, repo)
            info = json.loads((package / "build-info.json").read_text())
            self.assertEqual(info["source_commit"], "source-head-sha")
            self.assertEqual(info["ci_commit"], "ci-merge-sha")
            self.assertEqual(info["source_ref"], "release/test-branch")
            self.assertEqual(info["event"], "pull_request")

    def test_global_audio_sharing_defaults_off(self):
        header = Path(__file__).resolve().parents[2] / "Source" / "GlobalSettings.h"
        source = header.read_text(encoding="utf-8")
        self.assertRegex(source, r"std::atomic<bool>\s+send_\s*\{false\}")
        self.assertRegex(source, r"std::atomic<bool>\s+spare_\s*\{false\}")


    def test_rejects_non_object_factory_info(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            repo, build = self.make_repo(root)
            bundle = self.make_bundle(build)
            metadata_path = bundle / "Contents/Resources/moduleinfo.json"
            metadata = json.loads(remove_trailing_commas(metadata_path.read_text(encoding="utf-8")))
            metadata["Factory Info"] = "not-an-object"
            metadata_path.write_text(json.dumps(metadata), encoding="utf-8")
            with self.assertRaisesRegex(PackageValidationError, "Factory Info must be an object"):
                stage_package(build, root / "package", repo)

    def test_rejects_non_string_class_category(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            repo, build = self.make_repo(root)
            bundle = self.make_bundle(build)
            metadata_path = bundle / "Contents/Resources/moduleinfo.json"
            metadata = json.loads(remove_trailing_commas(metadata_path.read_text(encoding="utf-8")))
            metadata["Classes"][0]["Category"] = []
            metadata_path.write_text(json.dumps(metadata), encoding="utf-8")
            with self.assertRaisesRegex(PackageValidationError, "invalid VST3 class category or CID"):
                stage_package(build, root / "package", repo)


if __name__ == "__main__":
    unittest.main()

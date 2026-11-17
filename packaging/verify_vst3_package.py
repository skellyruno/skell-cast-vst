#!/usr/bin/env python3
"""Validate and stage the single Windows VST3 preview shipped by SkellCast."""
from __future__ import annotations
import argparse
import hashlib
import json
import os
import re
import shutil
import struct
import sys
from pathlib import Path
from typing import Any

NAME, VERSION, VENDOR = "SkellCast", "0.8.0", "Skelly"
CLASSES = {"Audio Module Class", "Component Controller Class"}
CID_RE = re.compile(r"^[0-9A-Fa-f]{32}$")


class PackageValidationError(RuntimeError):
    pass


def remove_trailing_commas(text: str) -> str:
    """Remove trailing commas outside JSON strings only."""
    out, quoted, escaped, i = [], False, False, 0
    while i < len(text):
        ch = text[i]
        if quoted:
            out.append(ch)
            if escaped:
                escaped = False
            elif ch == "\\":
                escaped = True
            elif ch == '"':
                quoted = False
            i += 1
            continue
        if ch == '"':
            quoted = True
            out.append(ch)
            i += 1
            continue
        if ch == ",":
            j = i + 1
            while j < len(text) and text[j].isspace():
                j += 1
            if j < len(text) and text[j] in "]}":
                i += 1
                continue
        out.append(ch)
        i += 1
    if quoted:
        raise PackageValidationError("moduleinfo.json contains an unterminated string")
    return "".join(out)


def require_amd64_pe(path: Path) -> None:
    with path.open("rb") as f:
        dos = f.read(64)
        if len(dos) < 64 or dos[:2] != b"MZ":
            raise PackageValidationError(f"{path}: missing DOS MZ header")
        offset = struct.unpack_from("<I", dos, 0x3C)[0]
        if offset < 64:
            raise PackageValidationError(f"{path}: invalid PE header offset {offset}")
        f.seek(offset)
        header = f.read(6)
        f.seek(offset + 24)
        magic = f.read(2)
    if len(header) != 6 or header[:4] != b"PE\0\0":
        raise PackageValidationError(f"{path}: missing PE signature")
    machine = struct.unpack_from("<H", header, 4)[0]
    if machine != 0x8664:
        raise PackageValidationError(f"{path}: expected AMD64 0x8664, found 0x{machine:04x}")
    if magic != b"\x0b\x02":
        value = struct.unpack("<H", magic)[0] if len(magic) == 2 else -1
        raise PackageValidationError(f"{path}: expected PE32+ 0x20b, found 0x{value:04x}")


def validate_bundle(bundle: Path) -> dict[str, Any]:
    contents = bundle / "Contents"
    metadata_path = contents / "Resources" / "moduleinfo.json"
    binary_dir = contents / "x86_64-win"
    if not metadata_path.is_file() or not binary_dir.is_dir():
        raise PackageValidationError(f"{bundle}: missing VST3 metadata or x86_64-win directory")
    modules = [p for p in binary_dir.iterdir() if p.is_file() and p.suffix.lower() == ".vst3"]
    all_modules = [p for p in contents.rglob("*.vst3") if p.is_file()]
    if len(modules) != 1 or len(all_modules) != 1 or all_modules[0] != modules[0]:
        raise PackageValidationError(f"{bundle}: expected exactly one Windows module and no duplicate architecture modules")
    module = modules[0]
    if module.name != f"{NAME}.vst3":
        raise PackageValidationError(f"{bundle}: Windows module filename mismatch")
    require_amd64_pe(module)
    try:
        raw = metadata_path.read_text(encoding="utf-8-sig")
        normalized = remove_trailing_commas(raw)
        meta = json.loads(normalized)
    except (OSError, UnicodeError, json.JSONDecodeError) as exc:
        raise PackageValidationError(f"{metadata_path}: invalid JSON: {exc}") from exc
    if not isinstance(meta, dict) or meta.get("Name") != NAME or meta.get("Version") != VERSION:
        raise PackageValidationError(f"{metadata_path}: product name/version mismatch")
    factory, entries = meta.get("Factory Info") or {}, meta.get("Classes")
    if factory.get("Vendor") != VENDOR:
        raise PackageValidationError(f"{metadata_path}: vendor mismatch")
    if not isinstance(entries, list) or len(entries) != 2:
        raise PackageValidationError(f"{metadata_path}: expected processor and controller classes")
    cids, categories = set(), set()
    for entry in entries:
        if not isinstance(entry, dict):
            raise PackageValidationError(f"{metadata_path}: class entry must be an object")
        category, cid = entry.get("Category"), entry.get("CID")
        if category not in CLASSES or not isinstance(cid, str) or not CID_RE.fullmatch(cid):
            raise PackageValidationError(f"{metadata_path}: invalid VST3 class category or CID")
        if cid.upper() in cids:
            raise PackageValidationError(f"{metadata_path}: duplicate VST3 class ID {cid}")
        if (entry.get("Name"), entry.get("Vendor"), entry.get("Version")) != (NAME, VENDOR, VERSION):
            raise PackageValidationError(f"{metadata_path}: class name/vendor/version mismatch")
        cids.add(cid.upper())
        categories.add(category)
    if categories != CLASSES:
        raise PackageValidationError(f"{metadata_path}: processor/controller pair incomplete")
    # Only the staged copy is normalized; the compiled source bundle is untouched.
    metadata_path.write_text(normalized.rstrip() + "\n", encoding="utf-8", newline="\n")
    warnings = []
    if not factory.get("URL"):
        warnings.append("vendor URL is blank; owner decision required before commercial release")
    if not factory.get("E-Mail"):
        warnings.append("support email is blank; owner decision required before commercial release")
    return {
        "name": NAME, "version": VERSION, "vendor": VENDOR, "module": module.name,
        "module_bytes": module.stat().st_size,
        "module_sha256": hashlib.sha256(module.read_bytes()).hexdigest(),
        "class_ids": sorted(cids), "metadata_warnings": warnings,
    }


def stage_package(source_dir: Path, package_dir: Path, repo_root: Path) -> dict[str, Any]:
    if not source_dir.is_dir() or source_dir.resolve() == package_dir.resolve():
        raise PackageValidationError("source directory is missing or equals package directory")
    bundles = [
        p for p in source_dir.rglob(f"{NAME}.vst3")
        if p.is_dir() and any(part.lower() == "release" for part in p.parts)
    ]
    if len(bundles) != 1:
        raise PackageValidationError(f"expected exactly one Release {NAME}.vst3 bundle; found {len(bundles)}")
    if package_dir.exists():
        shutil.rmtree(package_dir)
    package_dir.mkdir(parents=True)
    bundle = package_dir / f"{NAME}.vst3"
    shutil.copytree(bundles[0], bundle)
    plugin = validate_bundle(bundle)
    for filename in ("README.md", "TESTING.md"):
        source = repo_root / filename
        if not source.is_file():
            raise PackageValidationError(f"required package document missing: {filename}")
        shutil.copy2(source, package_dir / filename)
    info = {
        "product": NAME, "release_status": "preview-only; not a commercial release",
        "format": "VST3", "platform": "Windows x86_64", "version": VERSION,
        "vendor": VENDOR, "source_commit": os.getenv("SKELL_SOURCE_COMMIT", os.getenv("GITHUB_SHA", "local-unrecorded")),
        "ci_commit": os.getenv("GITHUB_SHA", "local-unrecorded"),
        "source_ref": os.getenv("SKELL_SOURCE_REF", os.getenv("GITHUB_REF", "local-unrecorded")),
        "event": os.getenv("GITHUB_EVENT_NAME", "local"),
        "workflow_run_id": os.getenv("GITHUB_RUN_ID", "local-unrecorded"),
        "workflow_run_attempt": os.getenv("GITHUB_RUN_ATTEMPT", "local-unrecorded"),
        "plugin": plugin,
    }
    (package_dir / "build-info.json").write_text(
        json.dumps(info, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    readiness = {
        "commercial_release": "blocked_pending_owner_approval",
        "confirmed_package_checks": [
            "single Windows x64 VST3 bundle", "AMD64 PE32+ module",
            "strict moduleinfo.json after staging normalization",
            "unique processor/controller class IDs", "build provenance and SHA-256 manifest"],
        "open_release_gates": [
            "owner-approved vendor URL and support email",
            "JUCE and third-party license review and notices",
            "FL Studio project recall and OBS/WASAPI manual acceptance",
            "verify global audio-sharing remains opt-in and its ON behavior is clearly explained to users",
            "signed installer, updater, backup and rollback",
            "target-hardware CPU, latency and long-session glitch tests"],
    }
    (package_dir / "RELEASE-READINESS.json").write_text(
        json.dumps(readiness, indent=2, sort_keys=True) + "\n", encoding="utf-8", newline="\n")
    (package_dir / "PREVIEW-ONLY.txt").write_text(
        "SkellCast Windows x64 VST3 preview package.\n"
        "Not a commercial release and not a signed installer.\n"
        "Read README.md before use. AUDIO SHARING is a global per-user opt-in preference;\n"
        "the default is OFF. Confirm it remains OFF unless you explicitly intend to share.\n"
        "Back up existing plugin installations and FL Studio projects before testing.\n",
        encoding="utf-8", newline="\n")
    lines = []
    for path in sorted(p for p in package_dir.rglob("*") if p.is_file()):
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        lines.append(f"{digest}  {path.relative_to(package_dir).as_posix()}")
    (package_dir / "SHA256SUMS.txt").write_text(
        "\n".join(lines) + "\n", encoding="ascii", newline="\n")
    return info


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source-dir", required=True, type=Path)
    parser.add_argument("--package-dir", required=True, type=Path)
    parser.add_argument("--repo-root", required=True, type=Path)
    args = parser.parse_args()
    try:
        result = stage_package(args.source_dir, args.package_dir, args.repo_root)
    except PackageValidationError as exc:
        print(f"ERROR: {exc}", file=sys.stderr)
        return 1
    plugin = result["plugin"]
    print(f"PASS {plugin['name']} {plugin['version']}: {plugin['module_bytes']:,} bytes, AMD64 PE32+, 2 unique CIDs")
    for warning in plugin["metadata_warnings"]:
        print(f"RELEASE WARNING: {warning}")
    print(f"Staged preview package: {args.package_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

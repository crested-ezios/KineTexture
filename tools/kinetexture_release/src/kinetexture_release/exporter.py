"""Fail-closed allowlist copying, independent of the Core and VMD packages.

The caller owns provenance decisions; 'approved' is a recorded review input,
not an automated legal determination. This tool never deletes source assets,
copies Git history, follows reparse points, runs Git, or publishes anything.
"""
from __future__ import annotations

import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import re
import stat


RECEIPT = "release_receipt.json"
FIELDS = {"source", "destination", "sha256", "owner", "source_type", "license",
          "upstream_url", "upstream_revision", "notice", "review_status"}
RESERVED = {"con", "prn", "aux", "nul", *(f"com{i}" for i in range(1, 10)),
            *(f"lpt{i}" for i in range(1, 10))}


class ReleaseError(ValueError):
    """The explicit release contract was not satisfied."""


def _json(data):
    def invalid(value):
        raise ReleaseError(f"non-finite manifest JSON constant: {value}")
    def unique(pairs):
        result = {}
        for name, value in pairs:
            if name in result:
                raise ReleaseError(f"duplicate JSON key: {name}")
            result[name] = value
        return result
    return json.loads(data, parse_constant=invalid, object_pairs_hook=unique)


def _relative(value):
    if not isinstance(value, str) or not value or "\\" in value:
        raise ReleaseError("allowlist paths must be explicit POSIX relative file paths")
    path = PurePosixPath(value)
    parts = value.split("/")
    if path.is_absolute() or any(p in ("", ".", "..") for p in parts):
        raise ReleaseError(f"absolute/noncanonical/traversal path rejected: {value}")
    for part in parts:
        if (part.casefold() == ".git" or part[-1] in " ." or
                any(c in '<>:"|?*' or ord(c) < 32 for c in part) or
                part.split(".", 1)[0].casefold() in RESERVED):
            raise ReleaseError(f"Git/reserved/glob/unsafe path rejected: {value}")
    return path


def _no_links(path):
    """Check path and all existing ancestors without resolving away a junction."""
    path = Path(os.path.abspath(path))
    if any(part.casefold() == ".git" for part in path.parts):
        raise ReleaseError("Git metadata root/ancestor is forbidden")
    for current in reversed((path, *path.parents)):
        try:
            info = current.lstat()
        except FileNotFoundError:
            continue
        if stat.S_ISLNK(info.st_mode) or getattr(info, "st_file_attributes", 0) & getattr(stat, "FILE_ATTRIBUTE_REPARSE_POINT", 0x400):
            raise ReleaseError(f"symlink/junction/reparse path rejected: {current}")
    return path


def _regular(path, *, max_bytes):
    path = _no_links(path)
    info = path.lstat()
    if not stat.S_ISREG(info.st_mode):
        raise ReleaseError(f"source is not a regular file: {path}")
    if info.st_nlink != 1:
        raise ReleaseError(f"hard-linked source/output rejected: {path}")
    if info.st_size > max_bytes:
        raise ReleaseError(f"file byte budget exceeded: {path}")
    with path.open("rb") as stream:
        current = os.fstat(stream.fileno())
        if (current.st_dev, current.st_ino) != (info.st_dev, info.st_ino):
            raise ReleaseError("file identity changed during validation")
        data = stream.read(max_bytes+1)
    if len(data) > max_bytes:
        raise ReleaseError(f"file grew beyond byte budget: {path}")
    _no_links(path)
    return data


def validate_manifest(manifest):
    if not isinstance(manifest, dict) or manifest.get("schema_version") != 1:
        raise ReleaseError("manifest schema_version must equal 1")
    if not isinstance(manifest.get("snapshot"), dict):
        raise ReleaseError("manifest requires snapshot metadata object")
    entries = manifest.get("files")
    if not isinstance(entries, list) or not entries:
        raise ReleaseError("manifest requires a nonempty file-level allowlist")
    if len(entries) > 100_000:
        raise ReleaseError("manifest file count exceeds budget")
    destinations, sources, directory_spellings = set(), set(), {}
    for entry in entries:
        if not isinstance(entry, dict) or FIELDS - entry.keys():
            raise ReleaseError(f"file entry lacks required fields: {sorted(FIELDS - entry.keys()) if isinstance(entry, dict) else 'object required'}")
        source, destination = _relative(entry["source"]), _relative(entry["destination"])
        src, dst = str(source).casefold(), str(destination).casefold()
        if src in sources or dst in destinations or dst == RECEIPT.casefold():
            raise ReleaseError("duplicate source/destination or reserved receipt destination")
        sources.add(src)
        destinations.add(dst)
        for parent in destination.parents:
            spelling = str(parent)
            if spelling == ".":
                continue
            key = spelling.casefold()
            if key in directory_spellings and directory_spellings[key] != spelling:
                raise ReleaseError("case-colliding destination directory spelling")
            directory_spellings[key] = spelling
        if not isinstance(entry["sha256"], str) or not re.fullmatch("[0-9a-f]{64}", entry["sha256"]):
            raise ReleaseError("SHA256 must be 64 lowercase hexadecimal characters")
        if entry["review_status"] != "approved":
            raise ReleaseError("pending/unknown/unapproved file cannot be exported")
        if entry["source_type"] not in ("original", "generated", "third_party"):
            raise ReleaseError("unknown source_type")
        for name in ("owner", "license"):
            if not isinstance(entry[name], str) or not entry[name].strip():
                raise ReleaseError(f"entry requires nonempty {name}")
        if not isinstance(entry["notice"], str):
            raise ReleaseError("notice must be a string (empty is explicit)")
        for name in ("upstream_url", "upstream_revision"):
            value = entry[name]
            if value is not None and (not isinstance(value, str) or not value.strip()):
                raise ReleaseError(f"{name} must be null or a nonempty string")
            if entry["source_type"] == "third_party" and not value:
                raise ReleaseError("third-party entry requires upstream URL and revision")
    # Fail before copying if one file destination is another file's parent.
    for destination in destinations:
        if any(str(p).casefold() in destinations for p in PurePosixPath(destination).parents if str(p) != "."):
            raise ReleaseError("destination file/directory collision")
    return manifest


def export_release(source_root, manifest_path, output, *, max_total_bytes=128 * 1024 * 1024,
                   expected_manifest_sha256=None):
    root = _no_links(source_root)
    if not root.is_dir():
        raise ReleaseError("source root must be an existing directory")
    output = _no_links(output)
    if output.exists():
        raise ReleaseError("output must not exist; unknown target overwrite is forbidden")
    if not output.parent.is_dir():
        raise ReleaseError("output parent must already exist")
    raw_manifest = _regular(manifest_path, max_bytes=16 * 1024 * 1024)
    if (expected_manifest_sha256 is not None and
            hashlib.sha256(raw_manifest).hexdigest() != expected_manifest_sha256):
        raise ReleaseError("review manifest changed after preflight")
    manifest = validate_manifest(_json(raw_manifest.decode("utf-8")))
    payloads, total = [], 0
    for entry in manifest["files"]:
        relative = _relative(entry["source"])
        source = root.joinpath(*relative.parts)
        data = _regular(source, max_bytes=max_total_bytes-total)
        digest = hashlib.sha256(data).hexdigest()
        if digest != entry["sha256"]:
            raise ReleaseError(f"source SHA256 mismatch: {entry['source']}")
        total += len(data)
        payloads.append((entry, data))
    receipt = {"schema_version": 1, "tool": "kinetexture-release/0.dev20261019",
               "manifest_sha256": hashlib.sha256(raw_manifest).hexdigest(),
               "manifest_canonical_sha256": hashlib.sha256(json.dumps(manifest, sort_keys=True, separators=(",", ":"), allow_nan=False).encode("utf-8")).hexdigest(),
               "manifest": manifest, "file_count": len(payloads), "payload_bytes": total,
               "review_note": "approval records are caller-supplied; this tool makes no legal determination"}
    receipt_bytes = (json.dumps(receipt, indent=2, sort_keys=True, allow_nan=False)+"\n").encode("utf-8")
    # Entire allowlist and payload hashes are validated before the first write.
    _no_links(output.parent)
    output.mkdir(exist_ok=False)
    for entry, data in payloads:
        destination = output.joinpath(*_relative(entry["destination"]).parts)
        destination.parent.mkdir(parents=True, exist_ok=True)
        _no_links(destination)
        with destination.open("xb") as stream:
            stream.write(data)
    with (output/RECEIPT).open("xb") as stream:
        stream.write(receipt_bytes)
    return verify_release(output, max_total_bytes=max_total_bytes)


def verify_release(output, *, max_total_bytes=128 * 1024 * 1024):
    root = _no_links(output)
    if not root.is_dir():
        raise ReleaseError("release output must be an existing directory")
    receipt = _json(_regular(root/RECEIPT, max_bytes=16 * 1024 * 1024).decode("utf-8"))
    if receipt.get("schema_version") != 1:
        raise ReleaseError("unknown receipt schema")
    manifest = validate_manifest(receipt.get("manifest"))
    canonical = hashlib.sha256(json.dumps(manifest, sort_keys=True, separators=(",", ":"), allow_nan=False).encode("utf-8")).hexdigest()
    if (receipt.get("manifest_canonical_sha256") != canonical or
            not re.fullmatch("[0-9a-f]{64}", str(receipt.get("manifest_sha256")))):
        raise ReleaseError("receipt manifest digest is malformed or changed")
    expected = {str(_relative(e["destination"])): e for e in manifest["files"]}
    actual, directories, total = set(), set(), 0
    for name in expected:
        directories.update(str(p) for p in PurePosixPath(name).parents if str(p) != ".")
    for directory, subdirs, names in os.walk(root, followlinks=False):
        _no_links(directory)
        for subdir in subdirs:
            path = Path(directory)/subdir
            _no_links(path)
            relative = path.relative_to(root).as_posix()
            _relative(relative)
            if relative not in directories:
                raise ReleaseError(f"unlisted release directory: {relative}")
        for name in names:
            path = Path(directory)/name
            relative = path.relative_to(root).as_posix()
            _relative(relative)
            if relative == RECEIPT:
                continue
            if relative not in expected:
                raise ReleaseError(f"unlisted release file: {relative}")
            data = _regular(path, max_bytes=max_total_bytes-total)
            total += len(data)
            if hashlib.sha256(data).hexdigest() != expected[relative]["sha256"]:
                raise ReleaseError(f"release SHA256 mismatch: {relative}")
            actual.add(relative)
    if actual != set(expected):
        raise ReleaseError(f"missing release files: {sorted(set(expected)-actual)}")
    if receipt.get("file_count") != len(actual) or receipt.get("payload_bytes") != total:
        raise ReleaseError("receipt count/size does not match output")
    return {"status": "verified", "file_count": len(actual), "payload_bytes": total,
            "manifest_sha256": receipt["manifest_sha256"]}

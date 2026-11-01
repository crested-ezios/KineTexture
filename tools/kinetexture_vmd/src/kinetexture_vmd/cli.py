"""Installed entry points. Conversion is deliberately narrower than MMD playback."""
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import sys

from .fixture import make_fixture
from .gkclip_compat import serialize_gkclip
from .parser import VmdError, parse_vmd
from .profiles import read_json, sample_compatible


def _write_new(path, data):
    with Path(path).open("xb") as stream:
        stream.write(data)


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    fixture = sub.add_parser("make-fixture", help="generate self-authored synthetic VMD/profiles in a new directory")
    fixture.add_argument("--output", required=True)
    inspect = sub.add_parser("inspect", help="bounded source parsing without target inference")
    inspect.add_argument("input")
    sample = sub.add_parser("sample", help="sample explicit compatible-local profiles; no arbitrary retarget/IK")
    sample.add_argument("input")
    sample.add_argument("--source-profile", required=True)
    sample.add_argument("--target-profile", required=True)
    sample.add_argument("--format", choices=("gkclip", "diagnostic-json"), default="gkclip")
    sample.add_argument("--output", required=True)
    args = parser.parse_args(argv)
    try:
        if args.command == "make-fixture":
            print(make_fixture(args.output))
            return 0
        vmd = parse_vmd(args.input)
        if args.command == "inspect":
            print(json.dumps({"format": "VMD 0002", "model_name": vmd.model_name,
                              "max_frame": vmd.max_frame, "fps": "not stored in VMD",
                              "bone_tracks": {n: len(k) for n, k in vmd.bones.items()},
                              "morph_tracks": {n: len(k) for n, k in vmd.morphs.items()},
                              "camera_keys": len(vmd.cameras), "property_keys": len(vmd.properties),
                              "light_keys": vmd.light_count, "shadow_keys": vmd.shadow_count},
                             ensure_ascii=True, indent=2))
            return 0
        source, source_hash = read_json(args.source_profile, with_hash=True)
        target, target_hash = read_json(args.target_profile, with_hash=True)
        samples = sample_compatible(vmd, source, target)
        output = Path(args.output)
        sidecar = Path(str(output)+".profile.json")
        if output.exists() or sidecar.exists():
            raise VmdError("refusing to overwrite output or profile sidecar")
        payload = (serialize_gkclip(samples, target) if args.format == "gkclip" else
                   (json.dumps(samples, indent=2, allow_nan=False)+"\n").encode("utf-8"))
        provenance = {"schema": "kinetexture.vmd.provenance/1", "output_format": args.format,
                      "source_vmd_sha256": vmd.source_sha256,
                      "source_profile_sha256": source_hash,
                      "target_profile_sha256": target_hash,
                      "output_sha256": hashlib.sha256(payload).hexdigest(),
                      "source_profile": source, "target_profile": target,
                      "limitations": ["compatible-local only", "no basis conversion or arbitrary retargeting",
                                      "no IK/append/morph/camera evaluation", "synthetic acceptance is not game visual acceptance"]}
        _write_new(output, payload)
        _write_new(sidecar, (json.dumps(provenance, indent=2, allow_nan=False)+"\n").encode("utf-8"))
        print(json.dumps({"output": str(output), "sha256": provenance["output_sha256"],
                          "frames": samples["frame_count"], "joints": len(samples["joints"])}))
        return 0
    except (OSError, ValueError, TypeError, KeyError) as exc:
        print(f"kinetexture-vmd: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

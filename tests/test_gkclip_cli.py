"""Independent end-to-end checks of native CLI artifacts (standard library only)."""
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import zipfile
import ast


VMD, SMPL = map(Path, sys.argv[1:3])


def run(exe, *args, ok=True):
    result = subprocess.run([str(exe), *map(str, args)], capture_output=True, text=True, encoding="utf-8")
    assert (result.returncode == 0) == ok, (result.returncode, result.stdout, result.stderr)
    return result


def clip(path):
    # Six 120fps samples: last authored timestamp is 5/120, independent of
    # the legacy header duration 6/120. Both root and local rotation vary.
    data = bytearray(b"GKCLIPCL" + struct.pack("<IfIfIf", 4, 120, 6, 6/120, 2, .083))
    for hid, name in [(0, b"Hips"), (7, b"Spine")]:
        data += struct.pack("<HBB", hid, len(name), 1) + name
    for f in range(6):
        data += struct.pack("<3f", f, 2*f, -f)
        for angle in [f*math.pi/10, 0]:
            data += struct.pack("<4f", 0, 0, math.sin(angle/2), math.cos(angle/2))
    data += b"\1"  # Deliberately include a camera and an authored-zero morph.
    data += struct.pack("<7f", 0, 1, 2, 3, 4, 5, 45) * 6
    data += struct.pack("<I", 1) + struct.pack("<H", 4) + b"face" + struct.pack("<6f", *([0]*6))
    path.write_bytes(data)


def npy(archive, name):
    data = archive.read(name + ".npy")
    assert data[:8] == b"\x93NUMPY\1\0"
    length = struct.unpack_from("<H", data, 8)[0]
    header = ast.literal_eval(data[10:10+length].decode("ascii"))
    raw = data[10+length:]
    return header, raw


with tempfile.TemporaryDirectory(prefix="kinetexture-cli-") as temp:
    folder = Path(temp) / "中文 路径"
    folder.mkdir()
    source = folder / "motion.gkclip"
    clip(source)
    profile = folder / "vmd.profile"
    profile.write_text("""kinetexture-gk-export-profile 1
id "analytic-local"
target vmd
source_versions 1 5
approximate 0
basis 1 0 0 0 1 0 0 0 1
meters_per_unit 1
root_offset 10 0 0
root_track 726f6f74
joint 0 -1 68697073 0 0 0 1 0 0 0 1
joint 7 0 7370696e65 0 0 0 1 0 0 0 1
""", encoding="ascii")
    target = folder / "result.vmd"
    run(VMD, source, "--profile", profile, "--output", target, ok=False)
    assert not target.exists() and not Path(str(target)+".report.json").exists()
    run(VMD, source, "--profile", profile, "--output", target, "--body-only")
    data = target.read_bytes()
    assert data[:len(b"Vocaloid Motion Data 0002")] == b"Vocaloid Motion Data 0002"
    count = struct.unpack_from("<I", data, 50)[0]
    assert count == 9  # 3 tracks x 3 output grid keys.
    tracks = {}
    for i in range(count):
        offset = 54 + 111*i
        name = data[offset:offset+15].split(b"\0")[0].decode("ascii")
        frame, *values = struct.unpack_from("<I7f", data, offset+15)
        tracks.setdefault(name, []).append((frame, values))
    assert [k[0] for k in tracks["root"]] == [0, 1, 2]
    # 30fps grid: source frames 0,4,endpoint5. Root scale is NOT reapplied.
    assert [k[1][:3] for k in tracks["root"]] == [[10,0,0],[14,8,-4],[15,10,-5]]
    assert abs(tracks["hips"][-1][1][-2] - math.sin(math.pi/4)) < 1e-6
    receipt = json.loads(Path(str(target)+".report.json").read_text(encoding="utf-8"))
    assert receipt["omitted_camera"] and receipt["omitted_morph_names"] == ["face"]
    assert receipt["source_frames"] == 6 and receipt["output_frames"] == 3
    assert receipt["input"] == str(source) and receipt["output"] == str(target)
    original = data
    run(VMD, source, "--profile", profile, "--output", target, "--body-only", ok=False)
    assert target.read_bytes() == original
    run(VMD, source, "--profile", profile, "--output", folder/"bad.vmd", "--body-only", "--fps", 60, ok=False)
    # Generated profiles must round-trip through the native config parser.
    for preset, joints, width in [("canonical-smpl",24,69),("canonical-smplx",22,63)]:
        p = folder / (preset+".profile")
        run(SMPL, "--preset", preset, "--write-profile", p)
        output = folder / (preset+".npz")
        run(SMPL, source, "--profile", p, "--output", output, "--body-only", "--neutral-shape")
        with zipfile.ZipFile(output) as archive:
            assert archive.testzip() is None
            header, _ = npy(archive, "body_pose")
            assert header["shape"] == (6,width)
            header, raw = npy(archive, "transl")
            assert header["shape"] == (6,3)
            assert struct.unpack_from("<3d", raw, 5*24) == (-5,10,-5)
            header, raw = npy(archive, "joint_valid")
            assert header["shape"] == (joints,) and raw[0] == 1 and sum(raw) == 2
            header, raw = npy(archive, "betas")
            assert header["shape"] == (1,10) and set(raw) == {0}
        report = json.loads(Path(str(output)+".report.json").read_text(encoding="utf-8"))
        assert report["shape"] == "explicit_neutral_preview"
        assert report["approximate_profile"]
    # Neither malformed source nor bad target options can create output.
    source.write_bytes(source.read_bytes() + b"unexpected")
    malformed = folder / "malformed.npz"
    run(SMPL, source, "--preset", "canonical-smpl", "--output", malformed, "--body-only", ok=False)
    assert not malformed.exists()
print("Native CLI artifact, timebase, omission, profile and failure checks passed")

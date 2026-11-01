"""Install local wheels in an independent venv and run synthetic installed CLIs.

No network/index, repository imports, game data, editable install or inherited
PYTHONPATH are used. Work directory and report must be new. This does not audit
the wheels' legal provenance; that remains a separate release gate.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import venv


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--wheels", required=True, type=Path)
    parser.add_argument("--work-dir", required=True, type=Path)
    parser.add_argument("--report", required=True, type=Path)
    args = parser.parse_args()
    wheels = sorted(args.wheels.resolve().glob("kinetexture_*.whl"))
    if len(wheels) != 2:
        raise SystemExit("exactly one VMD and one release wheel are required")
    if args.report.exists():
        raise SystemExit("report already exists")
    work = args.work_dir.resolve()
    work.mkdir(exist_ok=False)
    environment = dict(os.environ)
    for key in ("PYTHONPATH", "PYTHONHOME"):
        environment.pop(key, None)
    environment.update({"PIP_NO_INDEX": "1", "PIP_CONFIG_FILE": os.devnull,
                        "PYTHONNOUSERSITE": "1", "PYTHONUTF8": "1"})
    records = []

    def run(argv, expected=0):
        process = subprocess.run([str(a) for a in argv], cwd=work, env=environment,
                                 capture_output=True, text=True, encoding="utf-8")
        records.append({"argv": [str(a) for a in argv], "returncode": process.returncode,
                        "stdout": process.stdout, "stderr": process.stderr})
        if process.returncode != expected:
            raise RuntimeError(f"unexpected exit {process.returncode}: {argv}\n{process.stderr}")
        return process.stdout

    copied = []
    for wheel in wheels:
        target = work/wheel.name
        shutil.copyfile(wheel, target)
        copied.append(target)
    venv.EnvBuilder(with_pip=True, system_site_packages=False).create(work/"venv")
    binary = work/"venv"/("Scripts" if os.name == "nt" else "bin")
    python = binary/("python.exe" if os.name == "nt" else "python")
    run([python, "-I", "-m", "pip", "install", "--no-index", "--no-deps",
         "--disable-pip-version-check", *copied])
    imported = run([python, "-I", "-c", "import json,kinetexture_vmd,kinetexture_release; print(json.dumps([kinetexture_vmd.__file__,kinetexture_release.__file__]))"])
    for path in json.loads(imported):
        if not Path(path).is_relative_to(work/"venv"):
            raise RuntimeError("package imported from outside the clean venv")
    run([python, "-I", "-m", "pip", "check", "--disable-pip-version-check"])
    command_suffix = ".exe" if os.name == "nt" else ""
    vmd = binary/("kinetexture-vmd"+command_suffix)
    release = binary/("kinetexture-release"+command_suffix)
    run([vmd, "make-fixture", "--output", "demo"])
    run([vmd, "inspect", "demo/synthetic.vmd"])
    run([vmd, "sample", "demo/synthetic.vmd", "--source-profile", "demo/source.json",
         "--target-profile", "demo/target.json", "--output", "demo/motion.gkclip"])
    payload = (work/"demo/motion.gkclip").read_bytes()
    entry = {"source": "motion.gkclip", "destination": "synthetic/motion.gkclip",
             "sha256": hashlib.sha256(payload).hexdigest(), "owner": "self-authored synthetic test generator",
             "source_type": "generated", "license": "Apache-2.0", "upstream_url": None,
             "upstream_revision": None, "notice": "synthetic fixture, no real motion or model",
             "review_status": "approved"}
    manifest = {"schema_version": 1, "snapshot": {"purpose": "synthetic installed-CLI test only"}, "files": [entry]}
    (work/"allowlist.json").write_text(json.dumps(manifest, indent=2)+"\n", encoding="utf-8")
    run([release, "export", "--source-root", "demo", "--manifest", "allowlist.json", "--output", "candidate"])
    run([release, "verify", "--output", "candidate"])
    (work/"candidate/unlisted.txt").write_text("negative test", encoding="utf-8")
    run([release, "verify", "--output", "candidate"], expected=2)
    # Restore the self-authored negative probe; the exported bytes are untouched.
    (work/"candidate/unlisted.txt").unlink()
    run([release, "verify", "--output", "candidate"])
    report = {"status": "passed", "python": sys.version, "work_directory": str(work),
              "wheel_sha256": {p.name: hashlib.sha256(p.read_bytes()).hexdigest() for p in copied},
              "fixture_gkclip_sha256": hashlib.sha256(payload).hexdigest(),
              "commands": records, "scope": "isolated install + synthetic installed CLIs; no visual/legal acceptance"}
    with args.report.open("x", encoding="utf-8") as stream:
        json.dump(report, stream, indent=2)
        stream.write("\n")
    print(json.dumps({k: v for k, v in report.items() if k != "commands"}, indent=2))


if __name__ == "__main__":
    main()

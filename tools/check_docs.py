"""Check maintained local links and the generated installed README.

This development-only checker uses the Python standard library. It performs no
network requests and does not claim to validate remote URLs or historical reports.
"""
import argparse
from pathlib import Path
import re
import sys
from urllib.parse import unquote, urlsplit

ROOT = Path(__file__).resolve().parents[1]
REPOSITORY = "https://github.com/crested-ezios/KineTexture/blob/main/"
PAGES = ("README.md", "src/KineTexture/README.md", "python/README.md",
         "examples/two_bone_ik/README.md")
LINK = re.compile(r"\[[^\]]*\]\(([^\s)]+)\)")


def installed_readme():
    text = (ROOT / "README.md").read_text(encoding="utf-8-sig")

    def rewrite(match):
        target = match[1]
        if urlsplit(target).scheme or target.startswith("#") or target == "CHANNEL_SEMANTICS.md":
            return match[0]
        return match[0].replace("(" + target + ")", "(" + REPOSITORY + target + ")")

    return ("<!-- GENERATED from README.md by tools/check_docs.py; do not edit. -->\n\n"
            + LINK.sub(rewrite, text))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--write-installed-readme", action="store_true")
    args = parser.parse_args()
    expected = installed_readme()
    destination = ROOT / "src/KineTexture/README.md"
    if args.write_installed_readme:
        destination.write_text(expected, encoding="utf-8", newline="\n")
    errors = []
    if destination.read_text(encoding="utf-8-sig") != expected:
        errors.append("Installed README is stale: run python tools/check_docs.py --write-installed-readme")
    count = 0
    for name in PAGES:
        page = ROOT / name
        # Fenced shell examples are not Markdown links.
        text = re.sub(r"```.*?```", "", page.read_text(encoding="utf-8-sig"), flags=re.S)
        for match in LINK.finditer(text):
            target = urlsplit(match[1])
            if target.scheme or not target.path:
                continue
            path = (page.parent / unquote(target.path)).resolve()
            count += 1
            if not path.is_relative_to(ROOT) or not path.exists():
                errors.append(f"{name}: missing/outside local target {match[1]}")
    for error in errors:
        print("ERROR:", error, file=sys.stderr)
    print(f"Maintained documentation: {len(PAGES)} pages, {count} local targets, "
          f"installed README {'PASS' if not errors else 'FAIL'}")
    return bool(errors)


if __name__ == "__main__":
    sys.exit(main())

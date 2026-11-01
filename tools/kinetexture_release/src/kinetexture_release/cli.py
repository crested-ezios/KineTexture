"""Export only reviewed, SHA-bound files to a new local directory."""
import argparse
import json
import sys

from .exporter import export_release, verify_release


def main(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)
    export = sub.add_parser("export")
    export.add_argument("--source-root", required=True)
    export.add_argument("--manifest", required=True)
    export.add_argument("--output", required=True)
    verify = sub.add_parser("verify")
    verify.add_argument("--output", required=True)
    args = parser.parse_args(argv)
    try:
        result = (export_release(args.source_root, args.manifest, args.output)
                  if args.command == "export" else verify_release(args.output))
        print(json.dumps(result, sort_keys=True))
        return 0
    except (OSError, ValueError, TypeError, KeyError) as exc:
        print(f"kinetexture-release: {exc}", file=sys.stderr)
        return 2


if __name__ == "__main__":
    raise SystemExit(main())

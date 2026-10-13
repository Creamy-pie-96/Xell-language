#!/usr/bin/env python3

from pathlib import Path
import argparse


EXTENSIONS = {".cpp", ".hpp"}


def count_code_lines(path: Path) -> int:
    count = 0
    in_block_comment = False

    try:
        source = path.read_text(encoding="utf-8", errors="replace")
    except OSError as exc:
        print(f"Warning: cannot read {path}: {exc}")
        return 0

    for line in source.splitlines():
        i = 0
        has_code = False
        in_string = None
        escaped = False

        while i < len(line):
            char = line[i]
            nxt = line[i + 1] if i + 1 < len(line) else ""

            if in_block_comment:
                if char == "*" and nxt == "/":
                    in_block_comment = False
                    i += 2
                else:
                    i += 1
                continue

            if in_string:
                has_code = True

                if escaped:
                    escaped = False
                elif char == "\\":
                    escaped = True
                elif char == in_string:
                    in_string = None

                i += 1
                continue

            if char in ('"', "'"):
                in_string = char
                has_code = True
                i += 1
                continue

            if char == "/" and nxt == "/":
                break

            if char == "/" and nxt == "*":
                in_block_comment = True
                i += 2
                continue

            if not char.isspace():
                has_code = True

            i += 1

        if has_code:
            count += 1

    return count


def main():
    parser = argparse.ArgumentParser(
        description="Count nonblank, non-comment C++ code lines."
    )
    parser.add_argument(
        "root", nargs="?", default=".",
        help="Project directory to scan (default: current directory)"
    )
    parser.add_argument(
        "--exclude", action="append", default=[],
        help="Directory name to exclude; can be repeated"
    )
    args = parser.parse_args()

    root = Path(args.root).resolve()
    excluded = set(args.exclude)
    results = {}

    for path in sorted(root.rglob("*")):
        if not path.is_file() or path.suffix not in EXTENSIONS:
            continue

        relative = path.relative_to(root)

        if any(part in excluded for part in relative.parts[:-1]):
            continue

        directory = str(relative.parent)
        results.setdefault(directory, []).append(
            (relative.name, count_code_lines(path))
        )

    grand_total = 0

    for directory in sorted(results):
        print(f"\n{directory}/")

        for filename, lines in sorted(results[directory]):
            print(f"  {filename} -> {lines}")
            grand_total += lines

    print(f"\nTOTAL VALID CODE LINES: {grand_total}")


if __name__ == "__main__":
    main()
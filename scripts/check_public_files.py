#!/usr/bin/env python3
"""Reject private configuration, credentials and firmware in tracked Git files."""

from pathlib import Path
import re
import subprocess
import sys

ROOT = Path(__file__).resolve().parents[1]


def main():
    result = subprocess.run(["git", "ls-files", "-z"], cwd=ROOT, capture_output=True, check=True)
    paths = [Path(p.decode()) for p in result.stdout.split(b"\0") if p]
    issues = []
    forbidden_parts = {".local", ".pio", "build", "__pycache__"}
    forbidden_names = {"arduino_secrets.h", "config.h", ".env"}
    forbidden_suffixes = {".bin", ".elf", ".hex", ".map", ".uf2", ".pem", ".key", ".zip", ".log"}
    # Splitting the signatures keeps this checker from matching its own source.
    token_pattern = re.compile(r"(?:gh[pousr]_" + r"[A-Za-z0-9_]{20,}|github_pat_" + r"[A-Za-z0-9_]{20,})")
    private_key_marker = "-----BEGIN " + "PRIVATE KEY-----"
    private_ip = re.compile(r'"(?:10\.(?:\d{1,3}\.){2}\d{1,3}|192\.168\.\d{1,3}\.\d{1,3}|172\.(?:1[6-9]|2\d|3[01])\.\d{1,3}\.\d{1,3})"')
    for relative in paths:
        if (set(relative.parts) & forbidden_parts or relative.name in forbidden_names or
                relative.name.startswith(".env.") or relative.suffix in forbidden_suffixes):
            issues.append(f"Private file must not be tracked: {relative}")
            continue
        path = ROOT / relative
        if not path.is_file():
            continue
        text = path.read_text(encoding="utf-8")
        if token_pattern.search(text) or private_key_marker in text or private_ip.search(text):
            issues.append(f"Potential credential/private address in {relative} (value omitted).")
        if relative.name.endswith(".example.h"):
            for line in text.splitlines():
                match = re.match(r'^#define (SECRET_\w+)\s+(.+)$', line)
                if match and match[1] != "SECRET_MQTT_PORT" and match[2].strip() != '""':
                    issues.append(f"Nonempty personal setting in template: {relative}: {match[1]}")
    if issues:
        print("\n".join(issues), file=sys.stderr)
        return 1
    print(f"Privacy check passed for {len(paths)} tracked files.")
    return 0


if __name__ == "__main__":
    sys.exit(main())

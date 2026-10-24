#!/usr/bin/env python3
"""Build a correctly named Arduino sketch, independent of the checkout name."""

import argparse
import os
from pathlib import Path
import re
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SKETCH = "heltec_wirelesspaper_mqtt_hybrid_final"
FQBN = "Heltec-esp32:esp32:heltec_wireless_paper"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--example", action="store_true", help="Compile with isolated, nonfunctional test credentials.")
    parser.add_argument("--insecure", action="store_true", help="With --example, compile the explicit insecure TLS option.")
    parser.add_argument("--cli", default=os.environ.get("ARDUINO_CLI", "arduino-cli"))
    parser.add_argument("--fqbn", default=FQBN)
    parser.add_argument("--output-dir", type=Path, help="Optional local firmware output. Binaries contain credentials.")
    args = parser.parse_args()
    if args.insecure and not args.example:
        parser.error("--insecure only applies to --example; edit config.h for a real deployment.")
    if shutil.which(args.cli) is None:
        parser.error("arduino-cli not found; install it or set ARDUINO_CLI / --cli.")
    if not args.example:
        for name in ("arduino_secrets.h", "config.h"):
            if not (ROOT / name).is_file():
                parser.error(f"Missing {name}; copy and fill in the corresponding .example.h file.")

    with tempfile.TemporaryDirectory(prefix="heltec-build-") as temporary:
        sketch = Path(temporary) / SKETCH
        sketch.mkdir()
        for name in (f"{SKETCH}.ino", "display_logic.h", "config_validation.h"):
            shutil.copy2(ROOT / name, sketch / name)
        if args.example:
            values = {
                "SECRET_WIFI_SSID": '"build-only-network"',
                "SECRET_WIFI_PASSWORD": '""',
                "SECRET_MQTT_SERVER": '"broker.example.invalid"',
                "SECRET_MQTT_PORT": "8883",
                "SECRET_MQTT_USER": '""',
                "SECRET_MQTT_PASSWORD": '""',
                "SECRET_MQTT_CLIENT_ID": '"build-only-device"',
                "SECRET_MQTT_TOPIC_PREFIX": '"build-only/device"',
                "SECRET_MQTT_CA_CERT": '"BUILD-ONLY-NOT-A-CERTIFICATE"',
            }
            secrets = (ROOT / "arduino_secrets.example.h").read_text()
            for key, value in values.items():
                secrets = re.sub(rf"^#define {key} .*?$", f"#define {key} {value}", secrets, flags=re.MULTILINE)
            (sketch / "arduino_secrets.h").write_text(secrets)
            config = (ROOT / "config.example.h").read_text()
            if args.insecure:
                config = config.replace("MQTT_TLS_INSECURE = false", "MQTT_TLS_INSECURE = true")
            (sketch / "config.h").write_text(config)
        else:
            for name in ("arduino_secrets.h", "config.h"):
                shutil.copy2(ROOT / name, sketch / name)
        command = [args.cli, "compile", "--fqbn", args.fqbn, "--warnings", "all",
                   "--build-path", str(ROOT / "build"), "--jobs", "2"]
        if args.output_dir:
            command += ["--output-dir", str(args.output_dir.resolve())]
        subprocess.run(command + [str(sketch)], check=True)


if __name__ == "__main__":
    main()

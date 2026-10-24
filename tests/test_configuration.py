"""Compile the real validation header with valid and invalid configurations."""

from pathlib import Path
import subprocess
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
VALUES = {
    "SECRET_WIFI_SSID": '"test-network"',
    "SECRET_WIFI_PASSWORD": '""',
    "SECRET_MQTT_SERVER": '"broker.example.invalid"',
    "SECRET_MQTT_PORT": "8883",
    "SECRET_MQTT_USER": '""',
    "SECRET_MQTT_PASSWORD": '""',
    "SECRET_MQTT_CLIENT_ID": '"test-device"',
    "SECRET_MQTT_TOPIC_PREFIX": '"test/device"',
    "SECRET_MQTT_CA_CERT": '"test-only-not-a-certificate"',
}


class ConfigurationTests(unittest.TestCase):
    def compiles(self, changes=None, missing=None, config_change=None):
        values = VALUES | (changes or {})
        values.pop(missing, None)
        definitions = "\n".join(f"#define {key} {value}" for key, value in values.items())
        config = (ROOT / "config.example.h").read_text()
        if config_change:
            config = config.replace(*config_change)
        source = f"#define HIGH 1\n#define LOW 0\n{definitions}\n{config}\n"
        source += '#include "config_validation.h"\n'
        with tempfile.TemporaryDirectory(prefix="heltec-config-test-") as directory:
            path = Path(directory) / "test.cpp"
            path.write_text(source)
            result = subprocess.run(
                ["g++", "-std=c++17", "-fsyntax-only", "-I", str(ROOT), str(path)],
                capture_output=True,
            )
        return result.returncode == 0

    def test_valid_configuration_including_anonymous_auth(self):
        self.assertTrue(self.compiles())

    def test_no_missing_credentials_have_fallbacks(self):
        for key in VALUES:
            with self.subTest(key=key):
                self.assertFalse(self.compiles(missing=key))

    def test_empty_required_values(self):
        for key in ("SECRET_WIFI_SSID", "SECRET_MQTT_SERVER", "SECRET_MQTT_CLIENT_ID", "SECRET_MQTT_TOPIC_PREFIX"):
            with self.subTest(key=key):
                self.assertFalse(self.compiles({key: '""'}))

    def test_invalid_topics(self):
        for prefix in ("device/#", "device/+", "device/", "device name", "x" * 129):
            with self.subTest(prefix=prefix):
                self.assertFalse(self.compiles({"SECRET_MQTT_TOPIC_PREFIX": f'"{prefix}"'}))

    def test_invalid_ports(self):
        for port in ("0", "-1", "65536"):
            self.assertFalse(self.compiles({"SECRET_MQTT_PORT": port}))

    def test_ca_required_for_verified_tls(self):
        self.assertFalse(self.compiles({"SECRET_MQTT_CA_CERT": '""'}))
        self.assertTrue(self.compiles(
            {"SECRET_MQTT_CA_CERT": '""'},
            config_change=("MQTT_TLS_INSECURE = false", "MQTT_TLS_INSECURE = true"),
        ))

    def test_invalid_timing_and_rotation(self):
        for original, invalid in (
            ("SLEEP_MINUTES = 5", "SLEEP_MINUTES = 0"),
            ("FULL_REFRESH_EVERY = 10", "FULL_REFRESH_EVERY = 0"),
            ("DEFAULT_DISPLAY_ROTATION = 3", "DEFAULT_DISPLAY_ROTATION = 4"),
        ):
            with self.subTest(original=original):
                self.assertFalse(self.compiles(config_change=(original, invalid)))


if __name__ == "__main__":
    unittest.main()

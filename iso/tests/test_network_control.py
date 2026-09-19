import importlib.util
import io
import json
from pathlib import Path
import unittest
from unittest import mock


MODULE_PATH = Path(__file__).resolve().parents[1] / "overlay/usr/local/lib/josh-os/network-control.py"
SPEC = importlib.util.spec_from_file_location("josh_network_control", MODULE_PATH)
network = importlib.util.module_from_spec(SPEC)
assert SPEC.loader is not None
SPEC.loader.exec_module(network)


class NetworkControlTests(unittest.TestCase):
    def test_split_nmcli_handles_escaped_colons(self):
        self.assertEqual(network.split_nmcli(r"wlan0:Home\:Lab:wifi"), ["wlan0", "Home:Lab", "wifi"])

    def test_wifi_device_handles_failure_and_finds_wifi(self):
        with mock.patch.object(network, "run", return_value=(1, "", "failed")):
            self.assertIsNone(network.wifi_device())
        output = "eth0:ethernet\nwlan0:wifi"
        with mock.patch.object(network, "run", return_value=(0, output, "")):
            self.assertEqual(network.wifi_device(), "wlan0")

    def test_network_status_parses_device_and_ip_json(self):
        def fake_run(args, **_kwargs):
            joined = " ".join(args)
            if "GENERAL.STATE,GENERAL.CONNECTION" in joined:
                return 0, "GENERAL.STATE:100 (connected)\nGENERAL.CONNECTION:School\\: WiFi", ""
            if args[:5] == ["nmcli", "-t", "-f", "STATE", "general"]:
                return 0, "connected", ""
            if args[:5] == ["nmcli", "-t", "-f", "CONNECTIVITY", "general"]:
                return 0, "full", ""
            if args[:5] == ["nmcli", "-t", "-f", "WIFI", "radio"]:
                return 0, "enabled", ""
            if args[:4] == ["ip", "-j", "address", "show"]:
                return 0, '[{"ifname":"wlan0"}]', ""
            if args[:5] == ["ip", "-j", "route", "show", "default"]:
                return 0, '[{"dev":"wlan0"}]', ""
            raise AssertionError(args)

        with mock.patch.object(network, "wifi_device", return_value="wlan0"), mock.patch.object(network, "run", side_effect=fake_run):
            status = network.network_status()
        self.assertEqual(status["state"], "connected")
        self.assertEqual(status["connection"], "School: WiFi")
        self.assertEqual(status["addresses"][0]["ifname"], "wlan0")
        self.assertEqual(status["default_routes"][0]["dev"], "wlan0")

    def test_network_status_tolerates_bad_json_and_no_wifi(self):
        with mock.patch.object(network, "wifi_device", return_value=None), mock.patch.object(
            network, "run", side_effect=[
                (0, "", ""), (0, "", ""), (0, "", ""), (0, "{", ""), (0, "not-json", "")
            ]
        ):
            status = network.network_status()
        self.assertEqual(status["state"], "unknown")
        self.assertEqual(status["addresses"], [])
        self.assertEqual(status["default_routes"], [])
        self.assertEqual(status["wifi_state"], "unavailable")

    def test_wifi_scan_filters_sorts_and_handles_failures(self):
        with mock.patch.object(network, "wifi_device", return_value=None):
            self.assertEqual(network.wifi_scan(), [])
        with mock.patch.object(network, "wifi_device", return_value="wlan0"), mock.patch.object(
            network, "run", return_value=(1, "", "failed")
        ):
            self.assertEqual(network.wifi_scan(), [])

        rows = "\n".join([
            r":Cafe:--:bad",
            r"*:Home\:Lab:WPA2:78",
            r":Home\:Lab:WPA2:40",
            r":Guest:WPA2:60",
            r"malformed",
        ])
        with mock.patch.object(network, "wifi_device", return_value="wlan0"), mock.patch.object(
            network, "run", return_value=(0, rows, "")
        ):
            scanned = network.wifi_scan()
        self.assertEqual([row["ssid"] for row in scanned], ["Home:Lab", "Guest", "Cafe"])
        self.assertTrue(scanned[0]["active"])
        self.assertEqual(scanned[-1]["signal"], 0)
        self.assertEqual(scanned[-1]["security"], "--")

    def test_connect_wifi_validates_and_reports_nmcli_results(self):
        self.assertEqual(network.connect_wifi({"ssid": ""})[0], False)
        self.assertEqual(network.connect_wifi({"ssid": "x" * 33})[0], False)
        with mock.patch.object(network, "wifi_device", return_value=None):
            self.assertEqual(network.connect_wifi({"ssid": "Home"}), (False, "No Wi-Fi adapter detected."))

        calls = []
        def success_run(args, **kwargs):
            calls.append((args, kwargs))
            if "connect" in args:
                return 0, "connected", ""
            return 0, "", ""

        with mock.patch.object(network, "wifi_device", return_value="wlan0"), mock.patch.object(network, "run", side_effect=success_run):
            self.assertEqual(network.connect_wifi({"ssid": "Home", "password": "secret"}), (True, "connected"))
        self.assertTrue(any("connection.autoconnect-retries" in args for args, _ in calls))

        with mock.patch.object(network, "wifi_device", return_value="wlan0"), mock.patch.object(
            network, "run", side_effect=[(0, "", ""), (10, "", "bad password")]
        ):
            self.assertEqual(network.connect_wifi({"ssid": "Home", "password": "wrong"}), (False, "bad password"))

    def test_disconnect_wifi_reports_results(self):
        with mock.patch.object(network, "wifi_device", return_value=None):
            self.assertEqual(network.disconnect_wifi(), (False, "No Wi-Fi adapter detected."))
        with mock.patch.object(network, "wifi_device", return_value="wlan0"), mock.patch.object(
            network, "run", return_value=(0, "disconnected", "")
        ):
            self.assertEqual(network.disconnect_wifi(), (True, "disconnected"))
        with mock.patch.object(network, "wifi_device", return_value="wlan0"), mock.patch.object(
            network, "run", return_value=(1, "", "failed")
        ):
            self.assertEqual(network.disconnect_wifi(), (False, "failed"))

    def test_handler_read_json_validates_size_and_payload(self):
        handler = network.Handler.__new__(network.Handler)
        handler.headers = {"Content-Length": "7"}
        handler.rfile = io.BytesIO(b'{"x":1}')
        self.assertEqual(handler.read_json(), {"x": 1})

        for length, payload in [("bad", b""), ("0", b""), ("4097", b""), ("1", b"{")]:
            handler.headers = {"Content-Length": length}
            handler.rfile = io.BytesIO(payload)
            self.assertEqual(handler.read_json(), {})

    def _handler(self, path, payload=None):
        handler = network.Handler.__new__(network.Handler)
        handler.path = path
        handler.read_json = mock.Mock(return_value=payload or {})
        handler.send_json = mock.Mock()
        return handler

    def test_handler_post_routes_network_actions(self):
        handler = self._handler("/api/network/connect", {"ssid": "Home"})
        with mock.patch.object(network, "connect_wifi", return_value=(True, "Connected.")):
            handler.do_POST()
        handler.send_json.assert_called_once_with(200, {"ok": True, "message": "Connected."})

        handler = self._handler("/api/network/disconnect")
        with mock.patch.object(network, "disconnect_wifi", return_value=(False, "failed")):
            handler.do_POST()
        handler.send_json.assert_called_once_with(400, {"ok": False, "message": "failed"})

        handler = self._handler("/api/network/wifi-radio", {"enabled": False})
        with mock.patch.object(network, "run", return_value=(0, "disabled", "")) as run:
            handler.do_POST()
        run.assert_called_once_with(["nmcli", "radio", "wifi", "off"])
        handler.send_json.assert_called_once_with(200, {"ok": True, "message": "disabled"})

        handler = self._handler("/api/unknown")
        handler.do_POST()
        handler.send_json.assert_called_once_with(404, {"ok": False, "message": "Unknown API endpoint."})


if __name__ == "__main__":
    unittest.main()

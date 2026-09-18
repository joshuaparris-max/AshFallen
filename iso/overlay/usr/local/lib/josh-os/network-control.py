#!/usr/bin/env python3
import json
import os
import subprocess
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from urllib.parse import urlparse

HOST = "127.0.0.1"
PORT = 8765
SHELL_ROOT = "/opt/josh-os/shell"


def run(args, *, input_text=None, timeout=20):
    try:
        result = subprocess.run(
            args,
            input=input_text,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            timeout=timeout,
            check=False,
        )
        return result.returncode, result.stdout.strip(), result.stderr.strip()
    except (OSError, subprocess.TimeoutExpired) as exc:
        return 127, "", str(exc)


def split_nmcli(line):
    fields, current, escaped = [], [], False
    for ch in line:
        if escaped:
            current.append(ch)
            escaped = False
        elif ch == "\\":
            escaped = True
        elif ch == ":":
            fields.append("".join(current))
            current = []
        else:
            current.append(ch)
    fields.append("".join(current))
    return fields


def wifi_device():
    code, out, _ = run(["nmcli", "-t", "-e", "yes", "-f", "DEVICE,TYPE", "device", "status"])
    if code != 0:
        return None
    for line in out.splitlines():
        fields = split_nmcli(line)
        if len(fields) >= 2 and fields[1] == "wifi":
            return fields[0]
    return None


def network_status():
    _, state, _ = run(["nmcli", "-t", "-f", "STATE", "general"])
    _, connectivity, _ = run(["nmcli", "-t", "-f", "CONNECTIVITY", "general"])
    _, radio, _ = run(["nmcli", "-t", "-f", "WIFI", "radio"])
    device = wifi_device()

    wifi_state = "unavailable"
    connection = ""
    if device:
        _, detail, _ = run(
            ["nmcli", "-t", "-e", "yes", "-f", "GENERAL.STATE,GENERAL.CONNECTION", "device", "show", device]
        )
        values = {}
        for line in detail.splitlines():
            fields = split_nmcli(line)
            if len(fields) >= 2:
                values[fields[0]] = ":".join(fields[1:])
        wifi_state = values.get("GENERAL.STATE", "unknown")
        connection = values.get("GENERAL.CONNECTION", "")

    _, addresses, _ = run(["ip", "-j", "address", "show"])
    _, routes, _ = run(["ip", "-j", "route", "show", "default"])
    try:
        address_json = json.loads(addresses or "[]")
    except json.JSONDecodeError:
        address_json = []
    try:
        route_json = json.loads(routes or "[]")
    except json.JSONDecodeError:
        route_json = []

    return {
        "state": state or "unknown",
        "connectivity": connectivity or "unknown",
        "wifi_radio": radio or "unknown",
        "wifi_device": device,
        "wifi_state": wifi_state,
        "connection": connection if connection != "--" else "",
        "addresses": address_json,
        "default_routes": route_json,
    }


def wifi_scan(rescan=True):
    device = wifi_device()
    if not device:
        return []
    args = [
        "nmcli", "-t", "-e", "yes",
        "-f", "IN-USE,SSID,SECURITY,SIGNAL",
        "device", "wifi", "list", "ifname", device,
    ]
    if rescan:
        args += ["--rescan", "yes"]
    code, out, _ = run(args, timeout=30)
    if code != 0:
        return []

    rows = []
    seen = set()
    for line in out.splitlines():
        fields = split_nmcli(line)
        if len(fields) != 4:
            continue
        active, ssid, security, signal = fields
        if not ssid or ssid in seen:
            continue
        seen.add(ssid)
        try:
            signal_value = int(signal)
        except ValueError:
            signal_value = 0
        rows.append({
            "ssid": ssid,
            "security": security or "Open",
            "signal": signal_value,
            "active": active == "*",
        })
    rows.sort(key=lambda row: (not row["active"], -row["signal"], row["ssid"].lower()))
    return rows


def connect_wifi(payload):
    ssid = str(payload.get("ssid", "")).strip()
    password = str(payload.get("password", ""))
    if not ssid or len(ssid.encode("utf-8")) > 32:
        return False, "Invalid Wi-Fi network name."

    device = wifi_device()
    if not device:
        return False, "No Wi-Fi adapter detected."

    run(["nmcli", "radio", "wifi", "on"])
    args = ["nmcli", "--ask", "device", "wifi", "connect", ssid, "ifname", device]
    input_text = (password + "\n") if password else "\n"
    code, out, err = run(args, input_text=input_text, timeout=45)
    if code != 0:
        return False, err or out or "NetworkManager could not connect."

    # Make successful profiles resilient. Zero means retry forever.
    run(["nmcli", "connection", "modify", ssid, "connection.autoconnect", "yes",
         "connection.autoconnect-retries", "0"])
    return True, out or "Connected."


def disconnect_wifi():
    device = wifi_device()
    if not device:
        return False, "No Wi-Fi adapter detected."
    code, out, err = run(["nmcli", "device", "disconnect", device])
    return code == 0, out if code == 0 else (err or out or "Disconnect failed.")


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=SHELL_ROOT, **kwargs)

    def log_message(self, fmt, *args):
        pass

    def send_json(self, status, payload):
        body = json.dumps(payload, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Cache-Control", "no-store")
        self.send_header("Content-Length", str(len(body)))
        self.end_headers()
        self.wfile.write(body)

    def read_json(self):
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            length = 0
        if length <= 0 or length > 4096:
            return {}
        try:
            return json.loads(self.rfile.read(length).decode("utf-8"))
        except (UnicodeDecodeError, json.JSONDecodeError):
            return {}

    def do_GET(self):
        path = urlparse(self.path).path
        if path == "/api/health":
            self.send_json(200, {"ok": True})
        elif path == "/api/network/status":
            self.send_json(200, network_status())
        elif path == "/api/network/wifi":
            self.send_json(200, {"networks": wifi_scan(True)})
        else:
            super().do_GET()

    def do_POST(self):
        path = urlparse(self.path).path
        payload = self.read_json()
        if path == "/api/network/connect":
            ok, message = connect_wifi(payload)
            self.send_json(200 if ok else 400, {"ok": ok, "message": message})
        elif path == "/api/network/disconnect":
            ok, message = disconnect_wifi()
            self.send_json(200 if ok else 400, {"ok": ok, "message": message})
        elif path == "/api/network/wifi-radio":
            enabled = bool(payload.get("enabled", True))
            code, out, err = run(["nmcli", "radio", "wifi", "on" if enabled else "off"])
            ok = code == 0
            self.send_json(200 if ok else 400, {"ok": ok, "message": out if ok else err})
        else:
            self.send_json(404, {"ok": False, "message": "Unknown API endpoint."})


if __name__ == "__main__":
    os.chdir(SHELL_ROOT)
    server = ThreadingHTTPServer((HOST, PORT), Handler)
    server.serve_forever()

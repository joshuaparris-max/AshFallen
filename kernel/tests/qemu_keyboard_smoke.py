#!/usr/bin/env python3
import os
import socket
import subprocess
import sys
import time

READY = "JOSHOS_INTERRUPT_INPUT_READY"
KEY_OK = "JOSHOS_KEYBOARD_IRQ_OK"
BOOT_OK = "JOSHOS_BOOT_OK"

def read_log(path):
    try:
        with open(path, "r", encoding="utf-8", errors="replace") as handle:
            return handle.read()
    except FileNotFoundError:
        return ""

def wait_for(path, marker, process, timeout):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        content = read_log(path)
        if marker in content:
            return content
        if process.poll() is not None:
            raise RuntimeError(
                f"QEMU exited with {process.returncode} before {marker}\n{content}"
            )
        time.sleep(0.05)
    raise RuntimeError(f"timed out waiting for {marker}\n{read_log(path)}")

def connect_monitor(path, process, timeout):
    deadline = time.monotonic() + timeout
    last_error = None
    while time.monotonic() < deadline:
        if process.poll() is not None:
            raise RuntimeError(f"QEMU exited with {process.returncode}")
        client = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
        try:
            client.connect(path)
            client.settimeout(2.0)
            return client
        except OSError as exc:
            last_error = exc
            client.close()
            time.sleep(0.05)
    raise RuntimeError(f"could not connect to QEMU monitor: {last_error}")

def main():
    if len(sys.argv) != 2:
        print(f"usage: {sys.argv[0]} <JoshOS.iso>", file=sys.stderr)
        return 2

    iso = os.path.abspath(sys.argv[1])
    if not os.path.isfile(iso):
        print(f"ISO not found: {iso}", file=sys.stderr)
        return 2

    base = os.path.abspath("keyboard-smoke")
    log_path = base + ".log"
    monitor_path = base + ".monitor"
    for path in (log_path, monitor_path):
        try:
            os.unlink(path)
        except FileNotFoundError:
            pass

    command = [
        "qemu-system-x86_64",
        "-M", "q35",
        "-smp", "2",
        "-m", "256M",
        "-cdrom", iso,
        "-nic", "user,model=e1000",
        "-display", "none",
        "-serial", f"file:{log_path}",
        "-monitor", f"unix:{monitor_path},server=on,wait=off",
        "-no-reboot",
    ]

    process = subprocess.Popen(command)
    monitor = None
    try:
        content = wait_for(log_path, READY, process, 12.0)
        if BOOT_OK not in content:
            content = wait_for(log_path, BOOT_OK, process, 2.0)

        monitor = connect_monitor(monitor_path, process, 3.0)
        try:
            monitor.recv(4096)
        except socket.timeout:
            pass
        monitor.sendall(b"sendkey a\n")

        content = wait_for(log_path, KEY_OK, process, 3.0)
        print(content)
        print("Josh OS PS/2 hardware IRQ smoke test passed.")
        return 0
    except Exception as exc:
        print(str(exc), file=sys.stderr)
        print(read_log(log_path), file=sys.stderr)
        return 1
    finally:
        if monitor is not None:
            try:
                monitor.sendall(b"quit\n")
            except OSError:
                pass
            monitor.close()
        try:
            process.wait(timeout=2.0)
        except subprocess.TimeoutExpired:
            process.terminate()
            try:
                process.wait(timeout=1.0)
            except subprocess.TimeoutExpired:
                process.kill()
                process.wait()
        try:
            os.unlink(monitor_path)
        except FileNotFoundError:
            pass

if __name__ == "__main__":
    raise SystemExit(main())

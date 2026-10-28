#!/usr/bin/env python3
"""UNO Q Linux web server for the mecanum teleop UI."""

from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
import os
from pathlib import Path
from urllib.parse import urlsplit

from arduino.router_bridge import Bridge

HOST = os.environ.get("TELEOP_HOST", "0.0.0.0")
PORT = int(os.environ.get("TELEOP_PORT", "8080"))
WEB_ROOT = Path(__file__).with_name("web")
bridge = Bridge()


class TeleopHandler(BaseHTTPRequestHandler):
    server_version = "MecanumTeleop/1.0"

    def _json(self, status: int, payload: object) -> None:
        body = json.dumps(payload, separators=(",", ":")).encode("utf-8")
        self.send_response(status)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:
        path = urlsplit(self.path).path
        if path == "/api/status":
            try:
                raw = bridge.call("get_drive_status", timeout=0.4)
                status = json.loads(raw)
                self._json(200, status)
            except (OSError, ValueError, TimeoutError, ConnectionError) as error:  # Bridge is unavailable.
                self._json(503, {"error": type(error).__name__, "linkConnected": False})
            return
        if path not in ("/", "/index.html"):
            self._json(404, {"error": "not found"})
            return
        try:
            body = (WEB_ROOT / "index.html").read_bytes()
        except OSError:
            self._json(500, {"error": "teleop page is missing"})
            return
        self.send_response(200)
        self.send_header("Content-Type", "text/html; charset=utf-8")
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.send_header("X-Content-Type-Options", "nosniff")
        self.end_headers()
        self.wfile.write(body)

    def do_POST(self) -> None:
        if urlsplit(self.path).path != "/api/drive":
            self._json(404, {"error": "not found"})
            return
        try:
            length = int(self.headers.get("Content-Length", "0"))
        except ValueError:
            self._json(400, {"error": "invalid content length"})
            return
        if length <= 0 or length > 512:
            self._json(413, {"error": "request must be between 1 and 512 bytes"})
            return
        try:
            command = json.loads(self.rfile.read(length))
        except (UnicodeDecodeError, json.JSONDecodeError):
            self._json(400, {"error": "invalid JSON"})
            return
        if not isinstance(command, dict):
            self._json(400, {"error": "expected an object"})
            return

        values = [command.get("forward"), command.get("strafe"), command.get("turn")]
        if any(type(value) is not int or not -100 <= value <= 100 for value in values):
            self._json(400, {"error": "drive axes must be integers from -100 to 100"})
            return
        deadman = command.get("deadman")
        if type(deadman) is not bool:
            self._json(400, {"error": "deadman must be boolean"})
            return
        try:
            accepted = bridge.call("set_drive", *values, deadman, timeout=0.3)
        except (OSError, ValueError, TimeoutError, ConnectionError) as error:
            self._json(503, {"error": type(error).__name__, "accepted": False})
            return
        self._json(200 if accepted else 503, {"accepted": bool(accepted)})

    def log_message(self, format_string: str, *args: object) -> None:
        print("%s - %s" % (self.address_string(), format_string % args))


def main() -> None:
    if not bridge.connect(timeout=10):
        raise SystemExit("Cannot connect to the UNO Q Arduino Router; check arduino-router.service")
    server = ThreadingHTTPServer((HOST, PORT), TeleopHandler)
    server.daemon_threads = True
    print(f"Teleop UI ready at http://{HOST}:{PORT} (binds on all network interfaces)")
    try:
        server.serve_forever(poll_interval=0.25)
    except KeyboardInterrupt:
        print("Stopping teleop web server")
    finally:
        server.server_close()
        bridge.disconnect()


if __name__ == "__main__":
    main()

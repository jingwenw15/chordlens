#!/usr/bin/env python3
"""Local HTTP boundary between the browser UI and the C++ analyser."""
import json
import os
import re
import subprocess
import tempfile
from http import HTTPStatus
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
FRONTEND = ROOT / "web"
BINARY = Path(os.environ.get("CHORD_RECOGNITION_BINARY", ROOT / "build" / "chord_recognition"))
MAX_UPLOAD_BYTES = 30 * 1024 * 1024


def multipart_audio(handler):
    content_type = handler.headers.get("Content-Type", "")
    boundary_match = re.search(r"boundary=([^;]+)", content_type)
    length = int(handler.headers.get("Content-Length", "0"))
    if not boundary_match or length <= 0:
        raise ValueError("Expected a multipart audio upload.")
    if length > MAX_UPLOAD_BYTES:
        raise ValueError("Audio files must be 30 MB or smaller.")
    boundary = b"--" + boundary_match.group(1).strip('"').encode()
    for part in handler.rfile.read(length).split(boundary):
        if b"name=\"audio\"" not in part or b"\r\n\r\n" not in part:
            continue
        headers, data = part.split(b"\r\n\r\n", 1)
        filename = re.search(rb'filename="([^"]*)"', headers)
        suffix = Path(filename.group(1).decode("utf-8", "ignore") if filename else "audio.wav").suffix or ".wav"
        return data.rstrip(b"\r\n"), suffix
    raise ValueError("The request did not contain an audio file.")


class ChordLensHandler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(FRONTEND), **kwargs)

    def send_json(self, status, body):
        payload = json.dumps(body).encode()
        self.send_response(status)
        self.send_header("Content-Type", "application/json")
        self.send_header("Content-Length", str(len(payload)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(payload)

    def do_POST(self):
        if self.path != "/api/analyze":
            self.send_json(HTTPStatus.NOT_FOUND, {"error": "Unknown API endpoint."})
            return
        if not BINARY.is_file():
            self.send_json(HTTPStatus.SERVICE_UNAVAILABLE, {"error": "C++ analyser is not built. Run cmake -S . -B build && cmake --build build."})
            return
        try:
            audio, suffix = multipart_audio(self)
            with tempfile.NamedTemporaryFile(suffix=suffix) as upload:
                upload.write(audio)
                upload.flush()
                completed = subprocess.run([str(BINARY), upload.name, "--json"], capture_output=True, text=True, timeout=60, check=False)
            if completed.returncode:
                raise RuntimeError(completed.stderr.strip() or "The C++ analyser failed.")
            self.send_json(HTTPStatus.OK, json.loads(completed.stdout))
        except (ValueError, RuntimeError, json.JSONDecodeError) as error:
            self.send_json(HTTPStatus.BAD_REQUEST, {"error": str(error)})
        except subprocess.TimeoutExpired:
            self.send_json(HTTPStatus.REQUEST_TIMEOUT, {"error": "Analysis exceeded the 60-second limit."})

    def log_message(self, format, *args):
        print(f"{self.client_address[0]} - {format % args}")


if __name__ == "__main__":
    print("Chord Lens: http://localhost:8000")
    ThreadingHTTPServer(("127.0.0.1", 8000), ChordLensHandler).serve_forever()

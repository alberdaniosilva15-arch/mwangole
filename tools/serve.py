#!/usr/bin/env python3
import http.server
import os
import socketserver

PORT = 8090
WEB = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "web")


class Handler(http.server.SimpleHTTPRequestHandler):
    extensions_map = {
        **http.server.SimpleHTTPRequestHandler.extensions_map,
        ".wasm": "application/wasm",
        ".js": "text/javascript; charset=utf-8",
        ".mjs": "text/javascript; charset=utf-8",
        ".json": "application/json",
        ".glb": "model/gltf-binary",
        ".mwb": "application/octet-stream",
    }

    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=WEB, **kwargs)

    def do_POST(self):
        if self.path != "/__bench":
            self.send_error(404)
            return
        length = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(length)
        out = os.path.join(os.path.dirname(WEB), "bench_result.json")
        with open(out, "wb") as f:
            f.write(body)
        self.send_response(204)
        self.end_headers()

    def end_headers(self):
        self.send_header("Cache-Control", "no-store")
        super().end_headers()

    def log_message(self, *args):
        pass


def main():
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(("127.0.0.1", PORT), Handler) as httpd:
        print(f"MWANGOLÉ em http://127.0.0.1:{PORT}/")
        try:
            httpd.serve_forever()
        except KeyboardInterrupt:
            pass


if __name__ == "__main__":
    main()

#!/usr/bin/env python3
# HisokaOS Chromium streaming server.
# Runs on the host (the Mac). HisokaOS reaches it at 10.0.2.2:8090 via QEMU's
# user network. Given /render?url=SITE it renders the page with REAL headless
# Chromium, converts the screenshot to a BMP, and returns it. HisokaOS decodes
# the BMP and paints it in graphics mode -> real Chromium pages, displayed in
# our from-scratch OS.
import http.server, socketserver, urllib.parse, subprocess, os

PORT = 8090
W, H = 800, 600
CHROMIUM = "/opt/homebrew/bin/chromium"

class Handler(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a):
        print("  render:", self.path)
    def do_GET(self):
        q = urllib.parse.urlparse(self.path)
        url = urllib.parse.parse_qs(q.query).get("url", ["example.com"])[0]
        if not url.startswith("http"):
            host = url.split("/")[0]
            local = (host.startswith("localhost") or host.startswith("127.") or
                     host.startswith("10.0.2.") or host.startswith("192.168.") or
                     host.startswith("0.0.0.0"))
            url = ("http://" if local else "https://") + url
        png, bmp = "/tmp/hisoka_r.png", "/tmp/hisoka_r.bmp"
        for f in (png, bmp):
            try: os.remove(f)
            except OSError: pass
        try:
            subprocess.run([CHROMIUM, "--headless", "--disable-gpu", "--no-sandbox",
                            "--hide-scrollbars", "--force-device-scale-factor=1",
                            "--virtual-time-budget=6000", "--timeout=15000",
                            f"--screenshot={png}", f"--window-size={W},{H}", url],
                           timeout=45, capture_output=True)
        except Exception:
            pass
        if not os.path.exists(png):
            self.send_response(500); self.end_headers(); self.wfile.write(b"render failed"); return
        subprocess.run(["sips", "-z", str(H), str(W), "-s", "format", "bmp", png, "--out", bmp],
                       capture_output=True)
        data = open(bmp, "rb").read()
        self.send_response(200)
        self.send_header("Content-Type", "image/bmp")
        self.send_header("Content-Length", str(len(data)))
        self.send_header("Connection", "close")
        self.end_headers()
        self.wfile.write(data)

if __name__ == "__main__":
    socketserver.TCPServer.allow_reuse_address = True
    with socketserver.TCPServer(("0.0.0.0", PORT), Handler) as srv:
        print(f"HisokaOS Chromium streaming server on 0.0.0.0:{PORT} (reach from guest at 10.0.2.2:{PORT})")
        srv.serve_forever()

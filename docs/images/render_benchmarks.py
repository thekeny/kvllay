import os
import sys
import time
import json
import socket
import base64
import struct
import shutil
import tempfile
import subprocess
import urllib.request
import urllib.parse


class MinimalCDPClient:
    def __init__(self, ws_url):
        parsed = urllib.parse.urlparse(ws_url)
        self.host = parsed.hostname
        self.port = parsed.port
        self.path = parsed.path
        self.sock = socket.create_connection((self.host, self.port), timeout=20)
        self._handshake()
        self._id = 0

    def _handshake(self):
        key = base64.b64encode(os.urandom(16)).decode()
        req = (
            f"GET {self.path} HTTP/1.1\r\n"
            f"Host: {self.host}:{self.port}\r\n"
            f"Upgrade: websocket\r\n"
            f"Connection: Upgrade\r\n"
            f"Sec-WebSocket-Key: {key}\r\n"
            f"Sec-WebSocket-Version: 13\r\n\r\n"
        )
        self.sock.sendall(req.encode())
        resp = b""
        while b"\r\n\r\n" not in resp:
            chunk = self.sock.recv(1024)
            if not chunk:
                raise ConnectionResetError("Failed to handshake with Chrome CDP")
            resp += chunk

    def send_command(self, method, params=None):
        self._id += 1
        msg = json.dumps({"id": self._id, "method": method, "params": params or {}})
        payload = msg.encode("utf-8")

        # WebSocket frame: FIN=1, opcode=1 (text), masked=1
        frame = bytearray([0x81])
        length = len(payload)
        mask = os.urandom(4)
        if length < 126:
            frame.append(0x80 | length)
        elif length < 65536:
            frame.append(0x80 | 126)
            frame.extend(struct.pack(">H", length))
        else:
            frame.append(0x80 | 127)
            frame.extend(struct.pack(">Q", length))
        frame.extend(mask)
        frame.extend(payload[i] ^ mask[i % 4] for i in range(len(payload)))
        self.sock.sendall(frame)

        while True:
            header = self._recv_exact(2)
            b0, b1 = header[0], header[1]
            opcode = b0 & 0x0F
            masked = (b1 & 0x80) != 0
            payload_len = b1 & 0x7F
            if payload_len == 126:
                payload_len = struct.unpack(">H", self._recv_exact(2))[0]
            elif payload_len == 127:
                payload_len = struct.unpack(">Q", self._recv_exact(8))[0]

            mask_key = self._recv_exact(4) if masked else None
            data = self._recv_exact(payload_len)
            if masked:
                data = bytes(data[i] ^ mask_key[i % 4] for i in range(len(data)))

            if opcode == 1:
                obj = json.loads(data.decode("utf-8"))
                if obj.get("id") == self._id:
                    return obj.get("result", {})
            elif opcode == 8:
                break

    def _recv_exact(self, n):
        buf = bytearray()
        while len(buf) < n:
            chunk = self.sock.recv(n - len(buf))
            if not chunk:
                raise ConnectionResetError("Socket connection closed by browser")
            buf.extend(chunk)
        return bytes(buf)

    def close(self):
        try:
            self.sock.close()
        except Exception:
            pass


def find_chrome_binary():
    candidates = [
        "google-chrome",
        "google-chrome-stable",
        "chromium",
        "chromium-browser",
        "/home/kenyka/.nix-profile/bin/google-chrome",
        "/usr/bin/google-chrome",
        "/usr/bin/chromium",
    ]
    for c in candidates:
        path = shutil.which(c)
        if path:
            return path
    raise FileNotFoundError("Google Chrome or Chromium not found on system PATH.")


def render_all_charts():
    script_dir = os.path.dirname(os.path.abspath(__file__))
    html_file = os.path.join(script_dir, "benchmarks_preview.html")

    if not os.path.exists(html_file):
        raise FileNotFoundError(f"HTML source not found at: {html_file}")

    chrome_bin = find_chrome_binary()
    print(f"[render] Using browser: {chrome_bin}")

    port = 9388
    tmp_profile = tempfile.mkdtemp(prefix="kvllay_chrome_")

    chrome_cmd = [
        chrome_bin,
        "--headless",
        "--disable-gpu",
        "--disable-extensions",
        f"--user-data-dir={tmp_profile}",
        f"--remote-debugging-port={port}",
        "--no-sandbox",
        "--disable-dev-shm-usage",
        "--font-render-hinting=none",
        "about:blank",
    ]

    proc = subprocess.Popen(chrome_cmd, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    cdp = None
    try:
        # Wait for debugging port
        for _ in range(30):
            time.sleep(0.2)
            try:
                req = urllib.request.Request(f"http://127.0.0.1:{port}/json/list")
                with urllib.request.urlopen(req, timeout=1) as resp:
                    targets = json.loads(resp.read().decode())
                pages = [t for t in targets if t.get("type") == "page"]
                if pages:
                    ws_url = pages[0]["webSocketDebuggerUrl"]
                    cdp = MinimalCDPClient(ws_url)
                    break
            except Exception:
                continue

        if not cdp:
            raise RuntimeError("Failed to connect to Chrome DevTools Protocol port.")

        print(f"[render] Connected to Chrome CDP. Navigating to file://{html_file}")
        cdp.send_command("Page.enable")
        cdp.send_command("Page.navigate", {"url": f"file://{html_file}"})

        # Set viewport with 2x Device Scale Factor (for 1560px width Retina clarity)
        cdp.send_command(
            "Emulation.setDeviceMetricsOverride",
            {
                "width": 1600,
                "height": 3600,
                "deviceScaleFactor": 2,
                "mobile": False,
            },
        )

        # Allow fonts and layout to finish painting
        time.sleep(1.5)

        charts = [
            ("#chart-single-client", "benchmark_single_client.png"),
            ("#chart-multithreaded", "benchmark_multithreaded.png"),
            ("#chart-latency", "benchmark_latency.png"),
            ("#chart-ram", "benchmark_ram.png"),
            ("#chart-docker", "benchmark_docker.png"),
        ]

        for selector, filename in charts:
            expr = f"""
            (() => {{
                const el = document.querySelector('{selector}');
                if (!el) return null;
                const r = el.getBoundingClientRect();
                return {{
                    x: r.x + window.scrollX,
                    y: r.y + window.scrollY,
                    width: r.width,
                    height: r.height
                }};
            }})()
            """
            eval_res = cdp.send_command("Runtime.evaluate", {"expression": expr, "returnByValue": True})
            rect = eval_res.get("result", {}).get("value")

            if not rect:
                print(f"[render] Warning: Element {selector} not found on page, skipping.")
                continue

            clip = {
                "x": rect["x"],
                "y": rect["y"],
                "width": rect["width"],
                "height": rect["height"],
                "scale": 1,
            }

            shot_res = cdp.send_command(
                "Page.captureScreenshot",
                {"format": "png", "clip": clip, "captureBeyondViewport": True},
            )

            raw_png = base64.b64decode(shot_res["data"])
            out_path = os.path.join(script_dir, filename)
            with open(out_path, "wb") as f:
                f.write(raw_png)

            print(f"[render] Generated {filename} ({len(raw_png):,} bytes, dimensions ~{int(rect['width']*2)}x{int(rect['height']*2)})")

        print("[render] All benchmark charts successfully rendered to docs/images/")

    finally:
        if cdp:
            cdp.close()
        proc.terminate()
        try:
            proc.wait(timeout=3)
        except Exception:
            proc.kill()
        shutil.rmtree(tmp_profile, ignore_errors=True)


if __name__ == "__main__":
    try:
        render_all_charts()
    except Exception as exc:
        print(f"[render] Error: {exc}", file=sys.stderr)
        sys.exit(1)

#!/usr/bin/env python3
# alexis-server.py - the host side of Alexis, the HisokaOS assistant.
#
# HisokaOS can't run an LLM in its from-scratch kernel, so - exactly like the Chromium
# browser helper - the model runs HERE on the host (Ollama + the 'alexis' model) and we
# stream replies to HisokaOS over its TCP/IP stack. The guest hits 10.0.2.2:8091 with
# /chat?msg=... and we return a tiny delimited format the OS can parse:
#
#   <<THINK>> private reasoning (shown grey) <<SAY>> the answer <<END>>
#
# The Alexis identity, robot voice and anti-hallucination rules live in the Ollama
# Modelfile (alexis/Alexis.Modelfile), so they're baked into the model, not faked here.
import http.server, socketserver, urllib.parse, json, urllib.request

PORT  = 8091
MODEL = "alexis"
OLLAMA = "http://localhost:11434/api/chat"

def ask(msg):
    body = json.dumps({
        "model": MODEL,
        "messages": [{"role": "user", "content": msg}],
        "stream": False,
    }).encode()
    req = urllib.request.Request(OLLAMA, data=body, headers={"Content-Type": "application/json"})
    with urllib.request.urlopen(req, timeout=120) as r:
        out = json.loads(r.read().decode())
    return out.get("message", {}).get("content", "")

def split_think_say(text):
    think, say = "", text
    t = text
    if "THINK:" in t:
        after = t.split("THINK:", 1)[1]
        if "SAY:" in after:
            think = after.split("SAY:", 1)[0].strip()
            say   = after.split("SAY:", 1)[1].strip()
        else:
            say = after.strip()
    elif "SAY:" in t:
        say = t.split("SAY:", 1)[1].strip()
    # collapse whitespace, keep it tidy for the text-mode display
    think = " ".join(think.split())
    say = say.strip()
    return think, say

class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a):
        print("  alexis:", self.path[:80])
    def do_GET(self):
        q = urllib.parse.urlparse(self.path)
        msg = urllib.parse.parse_qs(q.query).get("msg", [""])[0]
        if not msg:
            self.send_response(400); self.end_headers(); self.wfile.write(b"no msg"); return
        try:
            think, say = split_think_say(ask(msg))
        except Exception as e:
            think, say = "", f"Alexis is offline on the host (is Ollama running, and the 'alexis' model built?). {e}"
        payload = f"<<THINK>>{think}<<SAY>>{say}<<END>>".encode("utf-8", "replace")
        self.send_response(200)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(payload)))
        self.end_headers()
        self.wfile.write(payload)

class Server(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True

if __name__ == "__main__":
    print(f"Alexis host server on 0.0.0.0:{PORT}  (model: {MODEL})")
    print("HisokaOS reaches it at 10.0.2.2:%d via the 'alexis' command." % PORT)
    Server(("0.0.0.0", PORT), H).serve_forever()

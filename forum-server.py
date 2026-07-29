#!/usr/bin/env python3
# forum-server.py - the host side of the Hisoka Forum. HisokaOS instances connect to
# this over their TCP stack (10.0.2.2:8092) the same way they reach Chromium/Alexis.
# It stores forums (genres), posts, comments, DMs and support messages in a JSON file,
# and answers in a simple TAB/line format the from-scratch OS can parse easily.
#
# Endpoints (all GET; the OS only does GET):
#   /forum/genres
#   /forum/posts?genre=Tech
#   /forum/post?id=3
#   /forum/new?genre=Tech&author=lab&title=Hi&body=hello
#   /forum/comment?id=3&author=lab&body=nice
#   /forum/search?q=helo                 (fuzzy - handles typos)
#   /dm/send?from=a&to=b&body=hi
#   /dm/inbox?user=b
#   /support?from=a&body=please add X     (forwarded to Jeffery via support.log)
import http.server, socketserver, urllib.parse, json, os, time, difflib

PORT = 8092
DB = os.path.join(os.path.dirname(os.path.abspath(__file__)), "forum.json")
SUPPORT_LOG = os.path.join(os.path.dirname(os.path.abspath(__file__)), "support.log")
GENRES = ["Tech", "Gaming", "Cars", "Planes", "Trains", "Finance", "AI", "Off-Topic", "Other"]

def load():
    if os.path.exists(DB):
        try: return json.load(open(DB))
        except Exception: pass
    return {"posts": [], "dms": [], "next": 1}

def save(d):
    json.dump(d, open(DB, "w"))

def san(s):
    return (s or "").replace("\t", " ").replace("\n", " ").strip()

class H(http.server.BaseHTTPRequestHandler):
    def log_message(self, *a): print("  forum:", self.path[:90])
    def reply(self, text):
        b = text.encode("utf-8", "replace")
        self.send_response(200); self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(b))); self.end_headers(); self.wfile.write(b)
    def do_GET(self):
        u = urllib.parse.urlparse(self.path)
        q = {k: v[0] for k, v in urllib.parse.parse_qs(u.query).items()}
        path = u.path
        d = load()
        if path == "/forum/genres":
            self.reply("\n".join(GENRES) + "\n"); return
        if path == "/forum/posts":
            g = q.get("genre", "")
            rows = [p for p in d["posts"] if p["genre"] == g]
            rows.sort(key=lambda p: (not p.get("pinned"), -p["id"]))
            self.reply("".join(f'{p["id"]}\t{san(p["title"])}\t{san(p["author"])}\t{len(p["comments"])}\t{"P" if p.get("pinned") else "-"}\n' for p in rows)); return
        if path == "/forum/post":
            pid = int(q.get("id", "0"))
            p = next((x for x in d["posts"] if x["id"] == pid), None)
            if not p: self.reply("not found\n"); return
            out = f'{san(p["title"])}\t{san(p["author"])}\n{p["body"]}\n<<COMMENTS>>\n'
            out += "".join(f'{san(c["author"])}\t{san(c["body"])}\t{"P" if c.get("pinned") else "-"}\n' for c in p["comments"])
            self.reply(out); return
        if path == "/forum/new":
            g = q.get("genre", "Other"); g = g if g in GENRES else "Other"
            p = {"id": d["next"], "genre": g, "author": san(q.get("author", "anon")) or "anon",
                 "title": san(q.get("title", "(untitled)")) or "(untitled)", "body": (q.get("body", "")[:1000]),
                 "comments": [], "pinned": False, "ts": int(time.time())}
            d["posts"].append(p); d["next"] += 1; save(d)
            self.reply(f'ok\t{p["id"]}\n'); return
        if path == "/forum/comment":
            pid = int(q.get("id", "0"))
            p = next((x for x in d["posts"] if x["id"] == pid), None)
            if not p: self.reply("not found\n"); return
            p["comments"].append({"author": san(q.get("author", "anon")) or "anon", "body": san(q.get("body", "")), "pinned": False, "ts": int(time.time())})
            save(d); self.reply("ok\n"); return
        if path == "/forum/search":
            ql = san(q.get("q", "")).lower()
            if not ql: self.reply(""); return
            scored = []
            for p in d["posts"]:
                hay = (p["title"] + " " + p["body"]).lower()
                score = 0
                if ql in hay: score = 100
                else:
                    words = hay.split()
                    m = difflib.get_close_matches(ql, words, n=1, cutoff=0.6)  # fuzzy: handles typos
                    if m: score = int(difflib.SequenceMatcher(None, ql, m[0]).ratio() * 90)
                if score: scored.append((score, p))
            scored.sort(key=lambda t: -t[0])
            self.reply("".join(f'{p["id"]}\t{san(p["title"])}\t{san(p["author"])}\t{len(p["comments"])}\t-\n' for _, p in scored[:30])); return
        if path == "/dm/send":
            d["dms"].append({"from": san(q.get("from", "anon")), "to": san(q.get("to", "")), "body": san(q.get("body", "")), "ts": int(time.time())})
            save(d); self.reply("ok\n"); return
        if path == "/dm/inbox":
            user = san(q.get("user", ""))
            rows = [m for m in d["dms"] if m["to"] == user]
            rows.sort(key=lambda m: -m["ts"])
            self.reply("".join(f'{san(m["from"])}\t{san(m["body"])}\n' for m in rows)); return
        if path == "/support":
            line = f'[{time.strftime("%Y-%m-%d %H:%M")}] {san(q.get("from","anon"))}: {san(q.get("body",""))}\n'
            open(SUPPORT_LOG, "a").write(line)
            self.reply("ok\n"); return
        self.reply("unknown endpoint\n")

class S(socketserver.ThreadingMixIn, http.server.HTTPServer):
    daemon_threads = True

if __name__ == "__main__":
    print(f"Hisoka Forum server on 0.0.0.0:{PORT}  (data: {DB})")
    print("HisokaOS reaches it at 10.0.2.2:%d via the 'forum' app." % PORT)
    S(("0.0.0.0", PORT), H).serve_forever()

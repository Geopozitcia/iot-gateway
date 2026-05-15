#!/usr/bin/env python3
import os
import glob
from http.server import HTTPServer, BaseHTTPRequestHandler
from datetime import datetime

PHOTOS_DIR = "/home/ilya/diploma/photos"
PORT = 8080

class PhotoHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        if self.path == "/":
            self.serve_index()
        elif self.path.startswith("/photos/"):
            self.serve_photo()
        else:
            self.send_error(404)

    def serve_index(self):
        photos = sorted(glob.glob(f"{PHOTOS_DIR}/*.jpg"), reverse=True)[:10]
        html = """<!DOCTYPE html>
<html>
<head>
    <meta charset="utf-8">
    <title>IoT Gateway</title>
    <meta http-equiv="refresh" content="30">
    <style>
        body { background: #1a1a1a; color: white; font-family: Arial; padding: 20px; }
        h1 { color: #4CAF50; }
        .grid { display: grid; grid-template-columns: repeat(auto-fill, minmax(300px, 1fr)); gap: 15px; }
        .card { background: #2a2a2a; border-radius: 8px; padding: 10px; }
        img { width: 100%; border-radius: 4px; }
        .time { color: #aaa; font-size: 13px; margin-top: 8px; text-align: center; }
    </style>
</head>
<body>
    <h1>IoT Gateway — последние фото</h1>
    <div class="grid">"""

        for photo in photos:
            name = os.path.basename(photo)
            try:
                dt = datetime.strptime(name[:15], "%Y%m%d_%H%M%S")
                time_str = dt.strftime("%d.%m.%Y %H:%M:%S")
            except:
                time_str = name

            html += f"""
    <div class="card">
        <img src="/photos/{name}">
        <div class="time">{time_str}</div>
    </div>"""

        html += """
    </div>
</body>
</html>"""

        self.send_response(200)
        self.send_header("Content-type", "text/html; charset=utf-8")
        self.end_headers()
        self.wfile.write(html.encode())

    def serve_photo(self):
        filename = self.path[8:]
        filepath = f"{PHOTOS_DIR}/{filename}"
        if not os.path.exists(filepath):
            self.send_error(404)
            return
        self.send_response(200)
        self.send_header("Content-type", "image/jpeg")
        self.end_headers()
        with open(filepath, "rb") as f:
            self.wfile.write(f.read())

    def log_message(self, format, *args):
        pass

if __name__ == "__main__":
    server = HTTPServer(("0.0.0.0", PORT), PhotoHandler)
    print(f"Веб-сервер запущен на порту {PORT}")
    server.serve_forever()
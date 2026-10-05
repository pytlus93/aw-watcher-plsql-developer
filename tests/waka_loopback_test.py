import base64
import json
import pathlib
import subprocess
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

requests = []
class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        assert self.path == '/api/v1/users/current/heartbeats'
        assert self.headers['Authorization'] == 'Basic '+base64.b64encode(b'synthetic-key').decode()
        data = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        requests.append(data)
        self.send_response(503 if len(requests) == 1 else 201)
        self.send_header('Content-Length', '2')
        self.end_headers()
        self.wfile.write(b'{}')
    def log_message(self, *args): pass

server = ThreadingHTTPServer(('127.0.0.1', 56662), Handler)
thread = threading.Thread(target=server.serve_forever, daemon=True)
thread.start()
try:
    subprocess.run([str(pathlib.Path('waka_tests.exe').resolve())], check=True, timeout=30)
finally:
    server.shutdown(); server.server_close(); thread.join()
assert len(requests) == 3
assert requests[0]['type'] == 'file' and requests[2]['type'] == 'app'
assert requests[2]['entity'] == 'PL/SQL Developer: SYNTHETIC_OWNER.SYNTHETIC_OBJECT'
assert all(r['language'] == 'PL/SQL' and r['category'] == 'coding' and r['is_write'] is False for r in requests)
assert all('DO_NOT_SEND' not in json.dumps(r) and 'synthetic-key' not in json.dumps(r) for r in requests)
print('PASS WakaTime JSON/authentication/503 recovery/rate-limit assertions; no cloud calls')

"""Synthetic real WinHTTP delivery and outage recovery; no third-party packages."""
import json
import pathlib
import subprocess
import threading
import sys
import socket
from urllib.parse import unquote
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

requests = []
failed_heartbeat = False


class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        global failed_heartbeat
        body = self.rfile.read(int(self.headers['Content-Length']))
        data = json.loads(body)
        assert 'Authorization' not in self.headers
        requests.append((self.path, data))
        status = 200
        if '/heartbeat?' not in self.path:
            # AW returns 304 for a bucket retained from a previous watcher run.
            status = 304
        if '/heartbeat?' in self.path and not failed_heartbeat:
            failed_heartbeat = True
            status = 503
        self.send_response(status)
        self.send_header('Content-Type', 'application/json')
        self.send_header('Content-Length', '0' if status == 304 else '2')
        self.end_headers()
        if status != 304:
            self.wfile.write(b'{}')

    def log_message(self, *args):
        pass


if __name__ == '__main__':
    server = ThreadingHTTPServer(('127.0.0.1', 56661), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    try:
        command = [str(pathlib.Path('aw_client_tests.exe').resolve())] if '--headless' in sys.argv else [str(pathlib.Path('sdk_harness.exe').resolve()), '--aw']
        subprocess.run(command, check=True, timeout=30)
    finally:
        server.shutdown()
        server.server_close()
        thread.join()
    pulses = [data for path, data in requests if '/heartbeat?' in path]
    buckets = [data for path, data in requests if '/heartbeat?' not in path]
    print(f'Loopback observed {len(buckets)} bucket requests and {len(pulses)} pulses')
    assert failed_heartbeat and len(pulses) >= 2, 'delivery retries on following live pulse'
    assert len(buckets) >= 2, 'bucket initialization retries after outage'
    assert all(pulse['data']['connection_alias'] == 'SYNTHETIC_ALIAS' for pulse in pulses)
    assert all(pulse['data']['interval_id'] for pulse in pulses)
    assert all(pulse['duration'] == 0 for pulse in pulses)
    assert all(pulse['timestamp'].endswith('Z') for pulse in pulses)
    assert all(bucket['hostname'].lower() == socket.gethostname().lower() for bucket in buckets)
    assert all(unquote(path.split('/')[4]).lower() == ('aw-watcher-plsql_'+socket.gethostname()).lower() for path, data in requests)
    print(f'PASS 8 real loopback HTTP assertions ({len(pulses)} pulses); one synthetic 503 recovered; correct computer name')

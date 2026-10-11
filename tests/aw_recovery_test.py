"""Synthetic lifecycle regressions; never contacts a production AW server."""
import json
import pathlib
import subprocess
import threading
import time
import sys
from urllib.parse import parse_qs, urlsplit
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

observed = []
existing = False
reject_creation = False
reject_heartbeat = False
strict = '--strict' in sys.argv
merge_windows = []
delivery_failures = []


class Server(ThreadingHTTPServer):
    allow_reuse_address = True


class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        global existing, reject_creation, reject_heartbeat
        body = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        pulse = '/heartbeat?' in self.path
        status = 200
        if pulse and reject_heartbeat:
            status = 503 if reject_heartbeat == 2 else 304
            reject_heartbeat -= 1
        elif not pulse:
            if reject_creation:
                reject_creation = False
                status = 500
            else:
                status = 304 if existing else 200
                existing = True
        observed.append((pulse, status, body))
        if pulse:
            merge_windows.append((status, int(parse_qs(urlsplit(self.path).query)['pulsetime'][0]), body))
        self.send_response(status)
        self.send_header('Content-Length', '0')
        self.end_headers()

    def log_message(self, *args):
        pass


def start():
    server = Server(('127.0.0.1', 56661), Handler)
    thread = threading.Thread(target=server.serve_forever, daemon=True)
    thread.start()
    return server, thread


def stop(pair):
    server, thread = pair
    server.shutdown()
    server.server_close()
    thread.join()


def launch(samples=32):
    process = subprocess.Popen([str(pathlib.Path('aw_client_tests.exe').resolve()), '--paced', '--strict' if strict else '--extended', '56661', str(samples)], stdout=subprocess.PIPE, text=True)
    def read_output():
        for line in process.stdout:
            if line.startswith('Synthetic AW delivery failed'):
                delivery_failures.append(time.monotonic())
    threading.Thread(target=read_output, daemon=True).start()
    return process


def wait(process):
    assert process.wait(timeout=25) == 0


def resumed(rows):
    pulses = [body for pulse, status, body in rows if pulse and status == 200]
    assert len(pulses) >= 2, 'actual successful heartbeats must resume'
    assert all(body['data']['title'] == ('Activity (context unavailable)' if strict else 'SYNTHETIC_ALIAS') for body in pulses)
    assert all(body['duration'] == 0 for body in pulses)
    assert all(body['timestamp'].endswith('Z') for body in pulses)
    assert len({body['timestamp'] for body in pulses}) == len(pulses), 'no replay or rewritten timestamps'
    if strict:
        assert all(list(body['data']) == ['file', 'project', 'language', 'title'] for body in pulses), 'strict wire keys/order'
    else:
        assert all(body['data']['delivery_segment_id'] for body in pulses)


# First registration, then a new producer process against the retained bucket.
pair = start()
try:
    wait(launch())
    assert any(not pulse and status == 200 for pulse, status, _ in observed)
    resumed(observed)
    observed.clear()
    wait(launch())
    assert any(not pulse and status == 304 for pulse, status, _ in observed)
    resumed(observed)
    observed.clear()
    reject_creation = True
    reject_heartbeat = 2
    wait(launch())
    assert observed[0][0:2] == (False, 500)
    assert observed[1][0:2] == (False, 304), 'creation failure must retry before heartbeat'
    assert any(pulse and status == 503 for pulse, status, _ in observed)
    assert any(pulse and status == 304 for pulse, status, _ in observed), '304 heartbeat must be treated as failure'
    failed = next(body for pulse, status, body in observed if pulse and status == 503)
    recovered = next(body for pulse, status, body in observed if pulse and status == 200)
    if not strict:
        assert failed['data']['delivery_segment_id'] != recovered['data']['delivery_segment_id'], 'outages split merge identity'
    assert next(window for status, window, body in merge_windows if body is recovered) == 0, 'recovery pulse cannot bridge a gap'
    resumed(observed)
finally:
    stop(pair)

# Producer runs before AW listens, then the server goes away and restarts.
observed.clear()
process = launch(48)
pair = None
try:
    time.sleep(0.65)
    pair = start()
    deadline = time.monotonic() + 5
    while sum(pulse and status == 200 for pulse, status, _ in observed) < 2:
        assert time.monotonic() < deadline, 'late server startup must receive heartbeats'
        time.sleep(0.05)
    failures_before = len(delivery_failures)
    stop(pair)
    pair = None
    before = len(observed)
    deadline = time.monotonic() + 8
    while len(delivery_failures) == failures_before:
        assert time.monotonic() < deadline, 'server outage must produce an actual failed delivery'
        time.sleep(0.05)
    pair = start()
    wait(process)
    resumed(observed[:before])
    resumed(observed[before:])
    prior = [body for pulse, status, body in observed[:before] if pulse and status == 200][-1]
    after = [body for pulse, status, body in observed[before:] if pulse and status == 200][0]
    if not strict:
        assert prior['data']['delivery_segment_id'] != after['data']['delivery_segment_id']
    assert next(window for status, window, body in merge_windows if body is after) == 0
finally:
    if pair:
        stop(pair)
    if process.poll() is None:
        process.kill()
        process.wait()

print('PASS first creation, retained-bucket producer restart, creation/heartbeat failures, late AW startup and server restart; heartbeats resume')

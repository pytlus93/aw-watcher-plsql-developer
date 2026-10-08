"""Synthetic lifecycle regressions; never contacts a production AW server."""
import json
import pathlib
import subprocess
import threading
import time
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer

observed = []
existing = False
reject_creation = False
reject_heartbeat = False


class Server(ThreadingHTTPServer):
    allow_reuse_address = True


class Handler(BaseHTTPRequestHandler):
    def do_POST(self):
        global existing, reject_creation, reject_heartbeat
        body = json.loads(self.rfile.read(int(self.headers['Content-Length'])))
        pulse = '/heartbeat?' in self.path
        status = 200
        if pulse and reject_heartbeat:
            reject_heartbeat = False
            status = 503
        elif not pulse:
            if reject_creation:
                reject_creation = False
                status = 500
            else:
                status = 304 if existing else 200
                existing = True
        observed.append((pulse, status, body))
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


def launch():
    return subprocess.Popen([str(pathlib.Path('aw_client_tests.exe').resolve()), '--paced'])


def wait(process):
    assert process.wait(timeout=15) == 0


def resumed(rows):
    pulses = [body for pulse, status, body in rows if pulse and status == 200]
    assert len(pulses) >= 2, 'actual successful heartbeats must resume'
    assert all(body['data']['title'] == 'SYNTHETIC_ALIAS' for body in pulses)
    assert all(body['duration'] == 0 for body in pulses)
    assert all(body['timestamp'].endswith('Z') for body in pulses)
    assert len({body['timestamp'] for body in pulses}) == len(pulses), 'no replay or rewritten timestamps'
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
    reject_heartbeat = True
    wait(launch())
    assert observed[0][0:2] == (False, 500)
    assert observed[1][0:2] == (False, 304), 'creation failure must retry before heartbeat'
    assert any(pulse and status == 503 for pulse, status, _ in observed)
    failed = next(body for pulse, status, body in observed if pulse and status == 503)
    recovered = next(body for pulse, status, body in observed if pulse and status == 200)
    assert failed['data']['delivery_segment_id'] != recovered['data']['delivery_segment_id'], 'outages split merge identity'
    resumed(observed)
finally:
    stop(pair)

# Producer runs before AW listens, then the server goes away and restarts.
observed.clear()
process = launch()
pair = None
try:
    time.sleep(0.65)
    pair = start()
    deadline = time.monotonic() + 5
    while sum(pulse and status == 200 for pulse, status, _ in observed) < 2:
        assert time.monotonic() < deadline, 'late server startup must receive heartbeats'
        time.sleep(0.05)
    before = len(observed)
    stop(pair)
    pair = None
    time.sleep(0.65)
    pair = start()
    wait(process)
    resumed(observed[:before])
    resumed(observed[before:])
    prior = [body for pulse, status, body in observed[:before] if pulse and status == 200][-1]
    after = [body for pulse, status, body in observed[before:] if pulse and status == 200][0]
    assert prior['data']['delivery_segment_id'] != after['data']['delivery_segment_id']
finally:
    if pair:
        stop(pair)
    if process.poll() is None:
        process.kill()
        process.wait()

print('PASS first creation, retained-bucket producer restart, creation/heartbeat failures, late AW startup and server restart; heartbeats resume')

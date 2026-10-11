"""Isolated installed AW server: synthetic events only, no production processes/data."""
import datetime as dt
import json
import os
import pathlib
import socket
import subprocess
import sys
import time
import urllib.request

executable = pathlib.Path(sys.argv[1]).resolve()
root = pathlib.Path('synthetic-installed-aw').resolve() / str(time.time_ns())
root.mkdir(parents=True)
environment = os.environ.copy()
# Windows platformdirs ignores app-data overrides in this installed version.
# Use verified memory storage; the binary may still write its normal testing log/config.
base = 'http://127.0.0.1:56663/api/0/'
bucket = 'aw-watcher-plsql_' + socket.gethostname()
log = (root / 'server.log').open('wb')
server = None
producer = None
with socket.socket() as probe:
    probe.bind(('127.0.0.1', 56663))  # Refuse to interfere with an existing test service.


def get(endpoint):
    with urllib.request.urlopen(base + endpoint, timeout=3) as response:
        return json.load(response)


def start():
    process = subprocess.Popen([str(executable), '--storage', 'memory', '--testing', '--host', '127.0.0.1', '--port', '56663'],
                               env=environment, stdout=log, stderr=log,
                               creationflags=subprocess.CREATE_NO_WINDOW)
    deadline = time.monotonic() + 15
    while True:
        assert process.poll() is None, 'isolated server failed; inspect synthetic-installed-aw log'
        try:
            assert get('info')['testing'] is True
            content = (root / 'server.log').read_bytes()
            assert b'Using storage method: memory' in content and b'Using database file:' not in content
            return process
        except OSError:
            assert time.monotonic() < deadline, 'isolated server startup timed out'
            time.sleep(0.1)


def launch(paused=False):
    return subprocess.Popen([str(pathlib.Path('aw_client_tests.exe').resolve()),
                             '--paused' if paused else '--paced', '--strict', '56663'])


def events():
    return get('buckets/' + bucket + '/events?limit=100')


def finish(process):
    process.terminate()
    try:
        process.wait(timeout=5)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait()


try:
    # The only server we stop is this child, using isolated config/data/log directories.
    server = start()
    observation_start = dt.datetime.now(dt.timezone.utc)
    producer = launch()
    deadline = time.monotonic() + 10
    while True:
        try:
            initial = events()
            if initial and initial[0]['duration'] >= 1:
                break
        except OSError:
            pass
        assert time.monotonic() < deadline
        time.sleep(0.1)
    producer.kill()  # Abrupt termination of our synthetic producer, not the editor.
    producer.wait()
    producer = None
    before = events()
    snapshot = get('buckets/' + bucket + '/export')
    last_end = max(dt.datetime.fromisoformat(event['timestamp']) + dt.timedelta(seconds=event['duration']) for event in before)
    finish(server)
    server = None
    time.sleep(1.2)
    server = start()
    # Rehydrate only our synthetic bucket in memory; no shared on-disk test database.
    request = urllib.request.Request(base + 'import', data=json.dumps(snapshot).encode(),
                                     headers={'Content-Type': 'application/json'}, method='POST')
    with urllib.request.urlopen(request, timeout=3) as response:
        assert response.status == 200
    producer = launch()
    assert producer.wait(timeout=15) == 0
    producer = None
    after = events()
    observation_end = dt.datetime.now(dt.timezone.utc)
    assert all(observation_start - dt.timedelta(seconds=1) <= dt.datetime.fromisoformat(event['timestamp']) <= observation_end + dt.timedelta(seconds=1) for event in after), 'API preserves UTC observation instants'
    assert len(after) > len(before), 'producer/server restart must create a separate interval'
    newer = [event for event in after if dt.datetime.fromisoformat(event['timestamp']) > last_end]
    assert newer and any(event['duration'] >= 1 for event in newer), 'heartbeats really resume'
    assert all(set(event['data']) == {'file', 'project', 'language', 'title'} for event in after)
    assert all(event['data']['title'] == 'Activity (context unavailable)' for event in after)
    assert get('buckets/' + bucket)['type'] == 'app.editor.activity'
    count = len(after)
    producer = launch(paused=True)
    assert producer.wait(timeout=15) == 0
    producer = None
    assert len(events()) == count, 'intentional pause must emit nothing'
    try:
        subprocess.Popen([str(root / 'missing-producer.exe')])
        raise AssertionError('missing executable unexpectedly launched')
    except FileNotFoundError:
        assert get('info')['testing'] is True, 'failed isolated launch leaves server running'
    print('PASS installed AW', get('info')['version'], 'memory-only synthetic strict contract, crash/server restart with snapshot rehydration, preserved gap, resumed duration, intentional pause and failed harness launch')
    print('Server API data key order:', list(after[0]['data']))
finally:
    if producer and producer.poll() is None:
        finish(producer)
    if server and server.poll() is None:
        finish(server)
    log.close()

"""Parse actual synthetic-host artifacts with independent stdlib readers."""
import csv
import datetime
import json
import pathlib
import sys

root = pathlib.Path(sys.argv[1]) / 'synthetic-host-history' / 'plsql-activity-v1'
ids = set()
intervals = {}
checks = 0
for file in root.glob('*.jsonl'):
    for line in file.read_text(encoding='utf-8').splitlines():
        record = json.loads(line)
        assert record['schema_version'] == 1
        assert record['id'] not in ids
        ids.add(record['id'])
        assert record['timestamp'].endswith('Z')
        assert 'password' not in record['context'] and 'sql' not in record['context']
        if record['type'] == 'interval':
            start = datetime.datetime.fromisoformat(record['start'])
            end = datetime.datetime.fromisoformat(record['end'])
            assert round((end-start).total_seconds()*1000) == record['duration_ms'] >= 0
            intervals[record['interval_id']] = record
        checks += 1
for file in root.glob('*.csv'):
    with file.open(encoding='utf-8', newline='') as stream:
        rows = list(csv.DictReader(stream))
    assert len(rows) == 1
    interval = intervals[rows[0]['id']]
    assert rows[0]['start_utc'] == interval['start']
    assert rows[0]['end_utc'] == interval['end']
    assert abs(float(rows[0]['duration_seconds'])*1000-interval['duration_ms']) < 0.01
    checks += 1
for file in root.glob('*.ics'):
    physical = file.read_bytes().split(b'\r\n')
    assert all(len(line) <= 75 for line in physical)
    unfolded = file.read_bytes().replace(b'\r\n ', b'').decode('utf-8')
    assert unfolded.startswith('BEGIN:VCALENDAR\r\n')
    assert unfolded.endswith('END:VCALENDAR\r\n')
    uid = next(line[4:] for line in unfolded.splitlines() if line.startswith('UID:')).split('@')[0]
    assert uid in intervals
    checks += 1
print(f'PASS independent JSON/CSV/ICS parsing ({checks} artifacts/records)')

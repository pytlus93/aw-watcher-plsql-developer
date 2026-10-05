# Local activity schema v1

When local activity files are enabled, data resides under `<destination>/plsql-activity-v1/`. UTF-8 JSONL files are named `activity-YYYY-MM-DD-<session>-<rotation>.jsonl`. Each line is a complete JSON object:

```json
{"schema_version":1,"id":"synthetic-record-1","type":"interval","timestamp":"2026-10-05T09:00:15.000Z","session_id":"synthetic-session","interval_id":"synthetic-interval","context":{"app":"PL/SQL Developer","language":"PL/SQL","window_id":"synthetic-worksheet","window_type":1,"file":"C:\\synthetic\\demo.sql","object_type":"","object_owner":"","object_name":"","connection_alias":""},"start":"2026-10-05T09:00:00.000Z","end":"2026-10-05T09:00:15.000Z","duration_ms":15000,"reason":"focus_lost"}
```

Record types:

- `start`: a focused, non-idle, enabled document context became active.
- `heartbeat`: periodic confirmation of the same active interval.
- `interval`: a completed interval; includes `start`, `end`, nonnegative `duration_ms`, and `reason`.
- `gap`: sampling stalled, monotonic time regressed, or UTC moved independently of elapsed time. The previous interval ends at its last observation.

Every record has an ID, UTC timestamp, session ID, interval ID and copied context. Window IDs are independent of file names: multiple unsaved worksheets remain distinct, and saving changes context without losing the live window identity. IDs are stable in stored records and exports; they are not permanent identities across application restarts.

Intervals close on context change, idle, focus loss, pause/settings, disable, shutdown and detected gaps. Sampling occurs at one-second resolution. Abrupt process termination leaves a start/heartbeat tail with **no completed interval**. Consumers must mark it incomplete rather than bridge it to the next session. Only completed intervals are exported. Graceful shutdown caps an unobserved tail at one second beyond the last sample. No trailing heartbeat is stretched across a suspension or crash.

Privacy flags clear file/object/connection metadata before recording or delivery. Database aliases default on for new installations; paths and object metadata default on. Titles, SQL text, usernames and passwords are never part of the schema. History can still reveal work context; select privacy options before enabling sensitive metadata. Changing options affects future records, not previously stored history.

Completed intervals produce individual `interval-YYYY-MM-DD-<interval-id>.csv` and `.ics` files when enabled. CSV uses quoted RFC4180 fields with formula protection and millisecond precision. ICS is a complete calendar containing one VEVENT, UTC DTSTART/DTEND at second precision, stable UID, escaped text and UTF-8 line folding. Subsecond intervals can have equal ICS start/end times; use JSONL or CSV for precise durations. Import suitability depends on the receiving calendar; these files are interchange artifacts, not automatic calendar synchronization.

Rotation happens on UTC date changes or before an append exceeds the configured MB limit. A record larger than the limit can occupy its own file. Retention removes only recognized date-prefixed `.jsonl`, `.csv` and `.ics` files under the dedicated namespace; it never recurses or follows symbolic links. It runs when a new active interval starts, so an indefinitely paused watcher does not clean files until resumed.

ActivityWatch is the default live adapter bound to **127.0.0.1** with direct WinHTTP and no proxy. Heartbeat data includes `interval_id` to prevent merging separate focus intervals. The bucket ID and hostname use the actual Windows computer name. Live delivery is best effort and retries on subsequent pulses without replaying historical failures. Failures never enable activity files implicitly. With local activity files enabled, JSONL retains observations independently of delivery; with files off, missed offline pulses are not retained.


ActivityWatch is the default destination. Local activity files default off and disable every journal/export/retention operation. Configuration persistence remains enabled. WakaTime is an optional HTTPS heartbeat adapter; see README. SQL-reference context uses object_type=SQL_REFERENCE and object_name containing a sorted list of identifiers, without SQL text. Closing AW pulses carry the same interval identity to account for activity shorter than the heartbeat period.

# Version 1.1 validation — 2026-10-05

Both x64 and x86 production DLLs and test DLLs build with MSVC 14.51.36231, C++17, /W4 /WX, static CRT and Windows SDK resources. No dependencies were installed.

- 65 core assertions pass on each architecture, including focus/idle/context boundaries, short-interval closing heartbeats, gap handling, optional activity files, rotation/retention, privacy, CSV/ICS encoding, and conservative SQL identifier extraction.
- 14 settings assertions pass on each architecture: AW-first defaults, file logging off, destination flag persistence, WakaTime enablement, synthetic key round-trip and absence of key material from the UTF-16 configuration.
- 8 real loopback AW protocol checks pass on each architecture, including actual Windows hostname/bucket naming, a simulated 503 and recovery. Start, periodic and closing heartbeats are observed with activity files off.
- WakaTime protocol tests pass on each architecture against a loopback-only mock server: documented endpoint, exact Basic authentication, JSON fields, file/app entities, API-key/connection exclusion from payloads, retry after 503 and throttling. No requests were sent to WakaTime accounts. The production adapter only allows HTTPS to api.wakatime.com; the HTTP test-port override exists only in ACTIVITY_TEST builds.
- Native settings resources and the new execution export compile on both architectures. The updated dialog has not been visually or interactively revalidated; previous synthetic UI runs tested the old layout. No more windows were opened after the user's refusal of GUI reruns.
- The user supplied a real 1.0 ActivityWatch export confirming host loading and heartbeat delivery. It also exposed the old hostname placeholder and missing SQL-window context, addressed in 1.1. Real-host invocation of BeforeExecuteWindow and the new SQL-reference inference remain unverified.

Windows Credential Manager read/write code is compiled but was not exercised with real credentials; tests use a process-local fake store. Real WakaTime account acceptance, corporate proxy behavior, API-key errors and the updated host UI require user validation. No account keys or SQL text were read from the user's live host during development.

An earlier GUI-based loopback run failed to receive pulses; the deliberate validation popup could interfere with foreground acquisition. The harness now waits for dismissal, rejects foreground-acquisition failure and excludes previous-run artifacts. Its revised interactive path has not been rerun. Headless core-to-network tests establish transport behavior independently.

Test data are synthetic. Other topic directories and old AW buckets were not modified. Packages contain production DLLs only, without test endpoint overrides or a persistent offline queue.

# Existing-bucket recovery correction (2026-10-08)

The local AW server returned HTTP 304 when registering the existing PL/SQL bucket. The adapter previously accepted only 2xx responses, preventing heartbeats after a watcher restart. Bucket registration now accepts 304; heartbeat requests still require 2xx. The headless loopback regression returns 304 for registration and 503 for the first heartbeat, checking subsequent recovery. Both x64 and x86 builds passed 65 core assertions, 14 settings assertions and eight loopback assertions. Real-host receipt after installing this correction remains unverified. Activity from the missing period cannot be recovered when local history was disabled.

## Policy validation audit (2026-10-08)

Read root policy commits d5e2966 and 7e5d79f. Reviewed the official AW Watchers, Writing a watcher, Data model and REST API documentation, plus maintained aw-client/client.py and aw-server/rest.py. The server implementation explicitly returns 304 for existing buckets; the maintained client accepts it. The deployed local /api/0/info reports v0.13.2, and metadata-only inspection confirms the existing bucket type app.editor.activity. This audit did not read private live activity or install a DLL.

Added privacy-filtered data.title summaries and a neutral fallback without application prefixes. Normalized file separators and supplied an empty project field for unknown project context. Added AW delivery segment identities that change on failure/restart to prevent merging through an outage within pulsetime. No persistent retry or timestamp rewriting occurs; failed observations remain gaps. No runtime bucket-type conflict guard or authentication for newer protected server versions is implemented.

The deliverable remains the native editor DLL; test executables are harnesses only. GUI visualization, revised settings UI, actual plugin restart with the new DLL, real Credential Manager operations and WakaTime account acceptance still require separate user validation. Adding title does not prove a custom bucket appears in any particular AW GUI view.

Primary sources: https://docs.activitywatch.net/en/latest/watchers.html ; https://docs.activitywatch.net/en/latest/examples/writing-watchers.html ; https://docs.activitywatch.net/en/latest/buckets-and-events.html ; https://docs.activitywatch.net/en/latest/api/rest.html ; https://github.com/ActivityWatch/aw-client/blob/master/aw_client/client.py ; https://github.com/ActivityWatch/aw-server/blob/master/aw_server/rest.py

Audit results: x64 and x86 MSVC 14.51.36231 builds passed with /W4 /WX, 71 core assertions and 14 headless settings assertions. Both architectures passed the eight-assertion AW loopback test, the new first-creation/existing-304/producer-restart/creation-500/heartbeat-503/late-server/server-restart suite with successful resumed heartbeats and changed delivery segment IDs, and the WakaTime authentication/retry/throttle/privacy loopback suite. All endpoints used synthetic local servers; no cloud credentials or intrusive UI harness was used. Rebuilt packages contain production DLLs and current documentation; no live installation occurred.

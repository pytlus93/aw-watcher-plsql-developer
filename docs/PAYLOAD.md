# Payload contract and privacy controls (1.3)

## Official contract and modes

The bucket is `app.editor.activity`. The [official editor contract](https://docs.activitywatch.net/en/latest/buckets-and-events.html#app-editor-activity) describes `file` as a filesystem path with forward slashes, `project` as the cwd path, and `language` as the language name. An AW event has a UTC/RFC3339 `timestamp`, numeric `duration` in seconds and object `data`. Our live observations use duration zero; the server merges heartbeats to derive intervals.

**Strict + title** sends only `file`, `project`, `language`, then `title`. Title is this workspace's addition, not part of the official editor contract. Strict uses an actual saved file path or an empty file value; project is empty because the SDK does not provide cwd. Object/connection context is neither captured nor included in titles or history in this mode.

**Extended** retains those four fields and adds selected extensions. It also supports the existing logical database display mapping: if there is no saved file, enabled object identifiers become the file label, and the shortened enabled connection alias becomes project. These values are database labels, not filesystem paths/cwd. `file_kind` distinguishes that mapping when selected. Inferred references require both object_type and object_name selected, so they always retain `object_type=SQL_REFERENCE`; they are not validated database objects. Disabling provenance suppresses SQL-reference reading and inferred names, rather than silently adding a disabled field. Multiple inferred identifiers are a comma-separated label, not a single file.

The Settings dialog exposes Extended payload (unchecked means strict + title) and a checkbox for each extension below. Include file paths is a collection permission for actual paths; it cannot remove the required `file` key, which becomes empty when no allowed label is available. Include object owner/name and Include database/TNS alias are master permissions in addition to individual field choices. Their disabled optional fields are omitted, not replaced with empty keys. Disabled owner/name/connection values are removed before constructing titles, virtual editor fields, WakaTime entities, JSONL, CSV and ICS. Choosing not to send one independent metadata field does not disable a different explicitly selected field containing related information (e.g. numeric and readable window type).

New settings default to strict + title, AW on, local history off and WakaTime off. Object type/owner/name and connection extensions start unchecked even when switching a new configuration to extended; selecting them is explicit private-context opt-in. Existing INI files without `payloadMode` migrate to extended mode, preserving existing collection permissions; all extension choices default selected to preserve the previous payload. This does not enable previously disabled private context. On Save, settings version 2 stores `payloadMode=strict|extended` and `field_<name>=0|1`. Switching mode retains choices for the next switch, but strict suppresses all extensions. Configuration/credential paths keep the legacy PLSQLActivity names. Existing history is not rewritten.

## Every available AW data field

The default column refers to new strict settings; selected extended defaults are shown explicitly. A disabled extension is absent, including from JSONL context. Empty/unknown behavior applies only when the field is enabled. Examples are synthetic.

| Field | Type | Meaning and example | Applicability | Privacy impact | Default | Unavailable |
|---|---|---|---|---|---|---|
| file | string | Saved path `C:/synthetic/demo.sql`; extended logical label `TEST_OBJECT` | Every event; actual path or extended database context | Path/object names; governed by file/object permissions | Required | Empty string |
| project | string | Cwd is unknown; extended logical connection `TEST_DEV` | Every event | Connection/customer hint in extended; follows alias permission | Required | Empty string |
| language | string | Host language `PL/SQL` | Every event, not a claim of parsing the current statement | Static host metadata; never an action/window-type field | Required | Always `PL/SQL` |
| title | string | `TEST_OBJECT @ TEST_DEV`, or `demo.sql` | Every event; workspace addition | Only permitted path basename/object/owner/alias context | Required | `Activity (context unavailable)` |
| app | string | `PL/SQL Developer` | Extended events and JSONL context | Static host identification | Absent; selected in extended | Constant |
| window_id | string | Random session-local worksheet ID `synthetic-window` | Extended events and JSONL context; optional CSV column | Correlates worksheet attention; no HWND/title | Absent; selected in extended | No event when no valid worksheet exists |
| window_type | integer | SDK category, e.g. `1` for SQL window | Extended events and JSONL context | Reveals window category | Absent; selected in extended | `0`; collection currently gates supported categories 1–6 |
| window_type_name | string | Readable category `SQL window` | Extended events and JSONL context | Same category context as window_type | Absent; selected in extended | `Unknown` |
| file_kind | string | `path`, `database_reference` or `unknown` | Extended events and JSONL context | Describes actual vs logical file label | Absent; selected in extended | `unknown` |
| object_type | string | Native `PACKAGE` or inferred `SQL_REFERENCE` | Extended, object master permission | Object category/provenance | Absent; off for new extended settings, selected for migrated settings | Empty string |
| object_owner | string | Native schema `TEST_OWNER` | Extended, object master permission; optional CSV/ICS context | Schema/customer identifier; omitted from title if disabled | Absent; off for new extended settings, selected for migrated settings | Empty string |
| object_name | string | Native object or inferred list `TEST_OBJECT, TEST_VIEW` | Extended, object master permission; optional CSV/ICS context | Database identifiers; no SQL text; disabled means no inferred label/title and no SQL-reference extraction | Absent; off for new extended settings, selected for migrated settings | Empty string |
| connection_alias | string | Full SDK TNS/database alias `TEST_DEV.WORLD` | Extended, alias master permission; optional CSV | Database/customer hint; disabled also clears project/title alias | Absent; off for new extended settings, selected for migrated settings | Empty string |
| interval_id | string | Random focus interval ID `synthetic-interval` | Extended live AW events only | Correlates an observed interval | Absent; selected in extended | Generated for every active interval |
| delivery_segment_id | string | Random transport segment ID `synthetic-segment` | Extended live AW events only | Correlates deliveries between transport failures | Absent; selected in extended | Generated per adapter; renewed on failure |

Title has no application/watcher prefix, replaces controls with spaces, and is capped at 240 UTF-8 bytes without splitting a character. A trailing `.WORLD` is hidden case-insensitively in title/extended project only; the full alias is retained when its extension is enabled. The suffix rule is currently fixed, not configurable.

The JSONL ledger's schema version, record/session/interval identities, timestamps and completed interval bounds are separate mandatory bookkeeping, not selectable event-context extensions. Disabling `interval_id` in AW data does not disable the ledger's interval identity needed for replay/deduplication. CSV/ICS contain only redacted context; disabled optional CSV columns are omitted and ICS summary is neutral Activity. The raw SDK window identity remains internal for foreground interval boundaries even when window_id is disabled.

## Transport boundaries and presentation order

Serialization emits official fields first, title next, and enabled extensions last. JSON object order is not semantic. The isolated installed AW server test uses verified memory storage, checks returned keys independently and reports API order. Windows app-data environment overrides do not isolate this installed binary; it may still write its normal testing log/config. The deployed v0.13.2 timeline code explicitly renders editor Filename, Path, Language; it ignores title, project and custom context in that tooltip. Reordering payload keys cannot change this template. Different tooltip content/order needs work in AW's presentation layer, outside this plugin; no GUI display promise is made.

First-time bucket POST and existing-bucket HTTP 304 both initialize delivery. HTTP 304 is not accepted for heartbeats. The first pulse of a focus interval, after adapter startup or after a failed request uses `pulsetime=0`, preventing positive gaps from merging even in strict mode without identity extensions. Normal pulses use heartbeat + 2 seconds. Extended identity fields add another merge boundary. Missed pulses are dropped, never replayed or timestamp-shifted; optional history remains independent. Zero-time coincident observations cannot establish positive elapsed activity. No calendar import/reconciliation is implemented; optional CSV/ICS are completed-interval exports only.

## Optional WakaTime body (not AW event data)

WakaTime delivery remains off by default and requires explicit cloud enablement. This adapter sends the documented [WakaTime heartbeat](https://wakatime.com/developers/#heartbeats) body rather than an AW data object. AW extension choices redact the source context before cloud delivery as well. API key is an authentication header from Credential Manager, never a body/history field.

| Field | Type | Meaning/example | Applicability/privacy | Default and unavailable |
|---|---|---|---|---|
| entity | string | Allowed file path, or `PL/SQL Developer: TEST_OWNER.TEST_OBJECT` | Every enabled cloud heartbeat; permitted paths/native or inferred identifiers; no alias | Cloud off; generic `PL/SQL Developer` without allowed context |
| type | string | `file` or `app` | Entity classification, not an observed action | Cloud off; `app` without a path |
| category | string | `coding` | Fixed category | Cloud off; constant |
| time | number | Unix observation seconds `1791187200.000` | Original UTC observation time; attention timing | Cloud off; supplied from sample |
| language | string | `PL/SQL` | Static language metadata | Cloud off; constant |
| is_write | boolean | `false` | Never asserts an actual edit | Cloud off; constant false |

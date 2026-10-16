# ActivityTracker for PL/SQL Developer 1.3

A native Allround Automations PL/SQL Developer watcher, written independently from the official installed SDK. No den-kozlov/PLSQLDeveloperWakatime implementation was used.

## Update or install

Close PL/SQL Developer, extract the matching x64/x86 ZIP, and install `ActivityTracker.dll` in the IDE's `PlugIns` directory. Restart the IDE. Enable **ActivityTracker** in the plug-in manager and open Configure, or **Plug-Ins → ActivityTracker → Settings**. For an upgrade, close the IDE and remove the old `PLSQLActivity.dll` from PlugIns before installing ActivityTracker.dll; keeping both would load duplicate producers. Existing configuration and credentials keep their legacy PLSQLActivity storage names. Before version 12, use **Tools → ActivityTracker → Settings**.

## ActivityWatch only

- Enable **Send to local ActivityWatch**, normally port **5600**.
- Turn **Save local activity files** off. This disables all JSONL, CSV, ICS, activity logs, directory probes and retention writes, even if export boxes were previously checked. Network failures do not turn logging back on.
- New settings use **strict + title**: saved path, unknown project, language and privacy-safe title only.
- Enable **Extended payload** for logical database labels and choose each extra field; collection permissions still apply. Existing configurations migrate to extended mode with their existing privacy choices.
- Keep heartbeat **15 seconds** and idle **300 seconds** initially.

New installations default to ActivityWatch on and activity files off. Existing explicit destination/privacy settings are preserved. Settings themselves remain in `%LOCALAPPDATA%\PLSQLActivity\settings.ini`; that is configuration, not an activity log. Previously created activity files are not deleted automatically.

Unsaved SQL worksheets have distinct window IDs and no fabricated file path. On execution, the adapter reads the selected SQL, or the worksheet text when nothing is selected, transiently on the IDE thread. It extracts FROM/JOIN/UPDATE/INTO references and discards SQL text. Only names are transmitted, marked `SQL_REFERENCE`; tables and views cannot be distinguished without database queries. Comments, ordinary literals, Oracle q literals and CTE aliases are excluded. This conservative parser is not a complete Oracle parser: comma joins, dynamic SQL, table functions and very large scripts may have missing context. With no selection, references can include other statements in the worksheet. Strict mode, disabling **Include object owner/name**, or disabling either **object_type** or **object_name** prevents this SQL-reference reading and extraction.

The connection field is the SDK's database/TNS string, not the connection-list display label. Program windows use the native object callback. SQL contents and database usernames/passwords are never transmitted or written to activity files. Every AW event includes a readable title derived only from enabled metadata, with no application-name prefix.

## WakaTime

Enable **Send to WakaTime (cloud)** and enter your API key. The masked key is stored in Windows Credential Manager under `PLSQLActivity/WakaTime`, never in the INI or activity files. Disabling WakaTime stops delivery; clearing the key removes the stored credential on Save.

This optional adapter uses the [official heartbeat API](https://wakatime.com/developers/#heartbeats) over verified HTTPS to api.wakatime.com, with HTTP Basic authentication. It sends file entities when paths are enabled/available, otherwise app entities with available object context. SQL text, database connection aliases and database credentials are excluded from WakaTime. Unchanged context is sent at most every 120 seconds; changed context and failures are limited to one attempt per 15 seconds. No persistent offline queue, CLI downloads, Git inspection or SQL execution is involved. Missed offline heartbeats are not replayed. This direct API adapter has synthetic protocol tests; acceptance by a real WakaTime account remains unverified.

## Optional local history

Enable **Save local activity files** to use rotating JSONL and optional CSV/ICS completed-interval exports. The destination, rotation, retention and export controls are disabled when local activity files are off. See [schema](docs/SCHEMA.md).

## Build and validation

Windows, MSVC C++ tools and Windows SDK are required; no dependencies are installed.

```powershell
./scripts/build.ps1 -Platform x64 -Test -SkipUI -Package
./scripts/build.ps1 -Platform x86 -Test -SkipUI -Package
cd build/x64
python ../../tests/loopback_test.py --headless
python ../../tests/waka_loopback_test.py
```

Omit `-SkipUI` only for an explicit interactive settings/SDK test; it deliberately exercises a validation popup. The revised settings layout is compiled but has not been visually revalidated. The older documentation screenshot describes version 1.0 only and is not included in current packages.

The reusable tracking core, native file adapter, SDK adapter, AW/WakaTime delivery adapters and native settings UI remain separate. The official 16.0.8 x64 SDK is installed; both architectures build. The user's 1.0 export confirms real ActivityWatch receipt, but new SQL-reference callbacks, credential storage, WakaTime account behavior and the revised host UI still require real-host validation. See [SDK findings](docs/SDK.md) and [validation](docs/VALIDATION.md).

MIT license. No changes were made to other workspace projects.

See [the complete field catalog, defaults and migration](docs/PAYLOAD.md). Standard fields are serialized before title and extensions; this does not control AW tooltip order. The installed editor tooltip explicitly renders Filename, Path and Language. Our v0.13.2 isolated-server validation reports actual API key order separately.

The DLL includes Windows version metadata. See [release notes and update registration](docs/RELEASES.md) for GitHub and the built-in update checker.

# ActivityTracker for PL/SQL Developer

ActivityTracker is a native PL/SQL Developer plugin that sends focused editor activity to [ActivityWatch](https://activitywatch.net/). Optional destinations are WakaTime and local JSONL history with CSV/ICS exports.

Tracking pauses when the IDE loses focus or the computer is idle. SQL text and database credentials are never sent or saved in activity history.

## Requirements

Windows and PL/SQL Developer with the matching x64/x86 plugin architecture. ActivityWatch must run locally for AW delivery. WakaTime additionally requires an account, network access and an explicitly configured API key. See [validation](docs/VALIDATION.md) for tested behavior and compatibility limits.

## Install or upgrade

Version **1.4.0** is distributed as a ZIP; there is no setup installer.

1. Choose the package matching **PL/SQL Developer's architecture**: `aw-watcher-plsql-developer-1.4.0-x64.zip` for a 64-bit IDE or `aw-watcher-plsql-developer-1.4.0-x86.zip` for a 32-bit IDE.
2. Close PL/SQL Developer. Extract `ActivityTracker.dll` into its `PlugIns` directory.
3. If upgrading from the earlier plugin, remove `PLSQLActivity.dll` from that directory to avoid loading both copies.
4. Restart the IDE and enable **ActivityTracker** in the plug-in manager.
5. Open **Plug-Ins → ActivityTracker → Settings**, or use **Configure** in the plug-in manager. Older IDE menus use **Tools → ActivityTracker → Settings**.

The plugin runs inside PL/SQL Developer. Check [validation notes](docs/VALIDATION.md) for tested host versions and remaining compatibility checks.

## Configuration and everyday use

Start ActivityWatch, then configure the plugin:

| Setting | Recommended starting value |
| --- | --- |
| Enable tracking | On |
| Pause tracking | Off |
| Send to local ActivityWatch | On; port `5600` |
| Testing server | Off; maps default port `5600` to `5666` |
| Heartbeat | `15` seconds |
| Idle timeout | `300` seconds |
| Save local activity files | Off |
| Send to WakaTime (cloud) | Off |

These are the defaults for a new configuration. Existing settings are preserved. With local activity files off, the plugin does not write activity logs or exports; delivery failures do not enable file logging.

The repository and release-package name is `aw-watcher-plsql-developer`. The host
plugin remains `ActivityTracker.dll`; the `aw-watcher-plsql_<hostname>` bucket and
legacy settings paths are preserved for upgrades. Testing and custom-port controls
are in the host Settings dialog rather than standalone `--testing` / `--port` flags.
An explicit nondefault port takes precedence over Testing; use a separate AW testing
server. Testing changes only the AW endpoint, not WakaTime or local history settings.

### Choose the context to include

By default, **Extended payload** is off. This sends the editor fields `file`, `project`, `language`, plus a readable `title`. Saved file paths are included when permitted; unknown file/project values are empty. Database object and connection context are off in this mode.

To show database context:

1. Enable **Extended payload**.
2. Enable **Include object owner/name**, and select **object_type** and **object_name**. Select **object_owner** if you also want the schema name.
3. To include the connection, enable **Include database/TNS alias** and select **connection_alias**.

Program windows use IDE object metadata. SQL worksheets can infer object references on execution: this briefly reads selected SQL (or the worksheet), extracts identifiers and discards the text. The result is inferred context, not a verified database object. Object/connection fields start unchecked, even when extended mode is enabled. See [the field catalog](docs/PAYLOAD.md) for examples and extraction limits.

ActivityWatch's editor tooltip may show only Filename, Path and Language, even when the event contains a title and connection. See the [field catalog](docs/PAYLOAD.md) for all fields, privacy controls, migration behavior and display limitations.

## Optional destinations

### WakaTime

Enable **Send to WakaTime (cloud)** and enter your API key. The implementation stores the key in Windows Credential Manager, not in the settings file or activity history. Clearing the key and saving removes the stored credential.

WakaTime receives permitted file or object context over HTTPS. Connection aliases and database credentials are excluded. This is optional cloud delivery and is off by default. Real Credential Manager/account acceptance remains unverified; payload and delivery details are in the [field catalog](docs/PAYLOAD.md).

### Local history and exports

Enable **Save local activity files** and choose a history directory. JSONL history rotates according to the size and retention settings. **Export CSV** and **Export ICS** add completed-interval exports. Existing history is not deleted when recording is disabled. See the [history schema](docs/SCHEMA.md).

## Troubleshooting

- **No new ActivityWatch events:** confirm ActivityWatch is running, the configured port matches, tracking is enabled and not paused, and an editor window is focused. The plugin retries ActivityWatch delivery when the server returns; missed observations are not replayed.
- **Object or connection is missing:** check the extended settings above. SQL references appear after execution and may be unavailable for unsupported syntax. The connection is the IDE's database/TNS alias, which can differ from its friendly connection-list label.
- **Tracking stopped after an internal or storage error:** correct the reported problem, then open Settings and save to retry.
- **Updates shows no latest version:** see [release notes and update registration](docs/RELEASES.md). A versioned DLL alone does not establish an online update source.

Configuration is stored in `%LOCALAPPDATA%\PLSQLActivity\settings.ini`. That legacy directory name is retained for upgrades; it is separate from optional activity history.

## Build and validate locally

Requires Windows, MSVC C++ tools and the Windows SDK. The build script does not install dependencies. Python is needed for the loopback tests.

```powershell
./scripts/build.ps1 -Platform x64 -Test -SkipUI -Package
./scripts/build.ps1 -Platform x86 -Test -SkipUI -Package
Push-Location build/x64
python ../../tests/loopback_test.py --headless
python ../../tests/aw_recovery_test.py
python ../../tests/aw_recovery_test.py --strict
python ../../tests/waka_loopback_test.py
Pop-Location
```

Packages are written to `dist/aw-watcher-plsql-developer-1.4.0-x64.zip` and
`dist/aw-watcher-plsql-developer-1.4.0-x86.zip`, each with a `.sha256` sidecar.
Repeat the loopback tests from `build/x86` to validate that architecture. Keep
`-SkipUI` for unattended builds; omitting it launches the interactive test harness.
No GitHub Actions workflows are required.

See [SDK integration](docs/SDK.md) and [validation results](docs/VALIDATION.md) for protocol recovery tests, toolchain details and the distinction between synthetic tests and real-host evidence. The revised settings UI and real WakaTime account behavior still need acceptance testing.

## License

[MIT](LICENSE), with the bundled picojson parser's BSD license retained in
[third-party notices](THIRD-PARTY-NOTICES.md).

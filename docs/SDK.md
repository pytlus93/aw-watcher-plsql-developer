# SDK verification

Inspected the official, installed Allround Automations distribution on 2026-10-05:

- Host: PL/SQL Developer **16.0.8.2176 (64 bit)** at `C:\Program Files\PLSQL Developer 16`.
- Sources: `PlugInDoc/plugindoc.pdf`, `PlugInDoc/Interface/PlSqlDevFunctions.h`, and the vendor-supplied `PlugInDoc/VisualStudio/PLSQLDeveloperVSDemoPlugIn` example. The example is reference material, not copied source.
- Vendor discovery documentation: <https://www.allroundautomations.com/products/pl-sql-developer/plug-ins/>.
- No source, clone or implementation from den-kozlov/PLSQLDeveloperWakatime is used.

The PDF specifies C calling conventions, 32-bit integers and booleans, and single-byte strings. The vendor's C++ header uses HWND for callbacks 16 and 18, which avoids truncating handles in x64 even though the PDF's older declaration says `int`. Exports and callback typedefs explicitly use `__cdecl`. The module definition file provides undecorated exported names. Win32 window procedures use CALLBACK, not the SDK convention.

`OnCreate` does not call the host. `OnActivate` starts a message-only timer window on the calling IDE thread. All host metadata calls occur on this same thread. The storage/network worker receives copied values only. The documentation does not give a general thread-safety guarantee or a general lifetime guarantee for returned strings: copying immediately and never calling the SDK from a worker are conservative implementation choices, not claims about undocumented vendor promises.

Callbacks used:

| Index | Official callback | Purpose |
|---|---|---|
| 1 | SYS_Version | Menu versus ribbon syntax |
| 14 | IDE_GetWindowType | SQL/test/program/command/plan/report type |
| 16 | IDE_GetWindowHandle | Main window owner |
| 18 | IDE_GetChildHandle | Stable identity for a live worksheet |
| 23 | IDE_Filename | Active document path |
| 110 | IDE_GetWindowObject | Program-window object type, owner and name |
| 240 | IDE_GetConnectionInfoEx | Optional database/TNS string |
| 245 | IDE_GetWindowConnection | Active window connection index |

The optional connection field is the database/TNS alias returned by the SDK, **not** the connection-list's friendly display description. The API also supplies username/password pointers: the adapter passes required output parameters but never dereferences, copies or logs those values. Object context is supported by the vendor only for program windows. Version 1.1 infers SQL-window references transiently at BeforeExecuteWindow using callbacks 31 (selection) and 30 (worksheet text); only identifiers are retained. It does not resolve actual database object types.

With object context enabled, the plugin reads selected/worksheet text transiently at execution for conservative identifier extraction. It never calls SQL execution APIs, result-grid APIs, or title capture. Idle detection is system-wide Windows LASTINPUTINFO, not an assertion of actual edits. Foreground focus is checked by foreground process ID once per second and on host callbacks. Very brief changes between samples may be missed. A blocked IDE cannot service timers; the core detects the ensuing gap rather than inventing activity.

Creation generates a new per-window identity; closed handles are discarded. `OnWindowClose` does not assume the subsequent close will succeed, so a cancelled close keeps the same identity. Deactivation stops timers and drains the worker before unloading. DLL_PROCESS_ATTACH performs only module-handle bookkeeping; work is never started under the loader lock.

The test host validates exports and callback affinity with synthetic callbacks. It is **not** PL/SQL Developer and cannot prove host compatibility, ribbon placement, Oracle connection behavior, or unload order. Real-host acceptance remains a separate manual validation step.

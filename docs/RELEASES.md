# Release notes

## 1.4.0 — 2026-10-24

- Rename the repository and versioned release packages to `aw-watcher-plsql-developer`.
- Add a Testing server setting: default port 5600 maps to 5666; a nondefault port
  takes precedence. Existing configurations retain testing off.
- Preserve `ActivityTracker.dll`, SDK update identity, the existing AW bucket,
  configuration/credential paths and privacy defaults.
- Strengthen synthetic HTTP/1.1 regressions with bodyless 304 responses advertising
  Content-Length: 67, including producer restart and subsequent heartbeat delivery.
- Derive ZIP names from DLL version metadata, verify versions and add SHA256 sidecars.
- Verify the existing bucket's JSON metadata is `app.editor.activity` before sending
  heartbeats; reject incompatible, missing-type and malformed metadata.
- Add a noninteractive SDK export check for both architectures; `-SkipUI` continues
  to avoid opening the settings/host UI harness.

No automatic update feed or vendor registration is enabled by this release.

## 1.3.1

Adds Windows DLL version metadata (1.3.1.0), the stable SDK update identity ActivityTracker, and the GitHub project URL in About. No automatic download or installation is performed.

## Distribution and update registration

Distribution currently consists of architecture-specific ZIP packages, not a setup executable. Close the IDE before replacing the DLL; see README.md for installation and legacy DLL removal.

The installed SDK documents PlugInName as the online-update identity and requires contacting vendor support for registration. The vendor describes the update-entry format at https://forums.allroundautomations.com/threads/check-online-updates.5428/ . release/ActivityTracker.updates is a prepared entry for submission, not an active feed. This registration format comes from historical vendor guidance and should be confirmed with support for the current service.

After the repository becomes public, publish locally built and tested x64/x86 ZIPs on GitHub Releases, and submit the entry to Allround Automations. The releases page lets users choose their architecture. No release or vendor submission has been performed. GitHub Actions is not required or used. Adding a GitHub URL alone does not populate Latest Version in the IDE.

The version resource and SDK export are checked locally; the IDE Updates dialog still requires real-host verification after installing the rebuilt DLL. The previous date display is consistent with missing version metadata, but its exact fallback behavior is not documented by the SDK.

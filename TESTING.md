# Testing Plan

Per-worker verification harness. Combines what we can verify automatically (build, load, log) with what needs manual streaming + network throttling.

## Automated checks (run after every change)

```
build_release.bat                      # build + installer
cmake --build build --target install-plugin --config Release
"/c/Program Files/obs-studio/bin/64bit/obs64.exe" --multi  # launch OBS
# wait 8s, grep newest log under %APPDATA%/obs-studio/logs/
```

Every worker MUST add at least one `blog(LOG_INFO, "[cm] ...")` marker on success that we can grep.

## Worker A — Scaffolding

**Status:** ✅ Done.

**Verified by:**
- `build/Release/obs-connection-manager.dll` exists (~10–20 KB)
- `dist/obs-connection-manager-0.1.0-setup.exe` exists (~2 MB)
- OBS log contains `[cm] loaded`
- OBS log lists `obs-connection-manager.dll` under loaded modules

## Worker B — Core adaptive logic

**Files:** `connection-monitor.c/h`, `bitrate-controller.c/h`, `session-log.c/h`, `dbr-conflict.c/h`

**Markers in OBS log (gate by these):**
- `[cm] frontend-event STREAMING_STARTED`
- `[cm] encoder caps: dyn=1`  (or `dyn=0` if encoder doesn't support live update)
- `[cm] sample congestion=X.XX dropped=N state=NAME`  (every poll)
- `[cm] decision step-down|step-up|hold|breaking-enter|breaking-exit kbps=N`

**Self-test mode:**
- Setting `selftest_on_load` (default true while in dev). On plugin load, after 5 s, monitor runs a scripted congestion sequence (0→0→0.4→0.7→0.95→0.95→0.95→0.5→0.1→0) at 1 Hz with synthetic target=6000.
- Should produce logged transitions: HEALTHY → DEGRADING → AT_FLOOR → BREAKING (entered) → BREAKING (still) → AT_FLOOR → recovery → HEALTHY.
- Verification: grep OBS log for the 5 expected state-change lines after plugin load.

**Manual test (real stream):**
1. Set up local RTMP sink: `docker run -p 1935:1935 tiangolo/nginx-rtmp` (or use any RTMP test endpoint)
2. Configure OBS to stream to it at 6000 kbps
3. Start streaming
4. Run `clumsy.exe` filtering outbound → `tcp.DstPort == 1935`, throttle 80%
5. Watch OBS log: should see `step-down kbps=5000`, `step-down kbps=4000`, then `step-up` lines after disabling clumsy
6. Verify `Settings → Output` shows the encoder bitrate matching final value
7. Stop stream → log shows bitrate restored to 6000

## Worker C — Dock UI

**Files:** `dock.cpp/h`, `notification-bridge.h`

**Markers:**
- `[cm] dock registered`
- `[cm] dock target changed -> N kbps` (when user edits the target spinbox)
- `[cm] dock banner=on|off`

**Verified by:**
- Open OBS → `View → Docks → Connection Manager` is present
- Dock shows current bitrate, editable target spinbox, floor, congestion bar, state badge
- During self-test mode, badge text matches state machine transitions
- Editing the target spinbox saves to `config.json` and is honored on the next monitor tick (the live ceiling for step-up)

## Worker D — Settings dialog + persistence

**Files:** `settings-store.c/h`, `settings-dialog.cpp/h`

**Markers:**
- `[cm] settings loaded keys=N`
- `[cm] settings dialog opened`
- `[cm] settings saved`

**Verified by:**
- `Tools → Connection Manager Settings…` opens a dialog
- Change `step_size_kbps` to 500 → save → settings file at `%APPDATA%/obs-studio/plugin_config/obs-connection-manager/config.json` contains `"step_size_kbps": 500`
- Restart OBS → settings persist (re-open dialog, verify field shows 500)
- "Reset to defaults" returns all fields to initial values

## Worker E — Notifications

**Files:** `toast.cpp/h`, plus banner wiring inside `dock.cpp`

**Markers:**
- `[cm] notification banner=on toast-fired=1`

**Verified by:**
- During self-test mode when state hits `BREAKING`:
  - Red banner appears at top of dock
  - Single Windows toast "OBS stream is breaking" fires
  - 30s cooldown prevents repeat toasts even if BREAKING re-asserts
- When state leaves `BREAKING`: banner disappears
- Toggle `notify_toast` off in settings → no toast on next BREAKING; banner still appears (independent gate)

## End-to-end (release-candidate gate)

Stream YouTube test endpoint at 6000 kbps. Mid-stream:

1. Launch clumsy.exe, throttle outbound to ~30% bandwidth
2. Verify in OBS log + dock: bitrate drops 6000→5000→4000→3000 over 4 seconds at default settings
3. Verify YouTube Studio shows the dropped bitrate
4. Disable clumsy → 5s recovery window observed → bitrate climbs back to 6000
5. Force complete network drop (disable adapter): banner + toast fire within 5s, no spam
6. Re-enable adapter → state recovers, banner clears
7. Stop stream → encoder bitrate restored to 6000, OBS built-in DBR setting (if present) restored to its prior value

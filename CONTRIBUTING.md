# Contributing to VirtualAP

A feature that does not exist is better than a broken one. VirtualAP runs as root on
rooted phones, on Wi-Fi chips whose drivers disagree about basic things, so a patch that
works on your phone and breaks on someone else's is a regression, not a contribution. And
ten lines that do the job beat a hundred that do the same job: delete before you add, reuse
before you write.

## What must hold

The rules for anyone changing this repository, agents included, are in
[AGENTS.md](./AGENTS.md): the style, the commit format, the choke points and the hard
constraints. In short: Android only, `backend/start-ap` is strict POSIX sh over the bundled
busybox and Android's own `ip` and `iptables`, the app runs on Android 8, and the binaries
come only from `scripts/build-static.sh`.

VirtualAP runs on Qualcomm, Broadcom FullMAC (Samsung) and MediaTek chips. A fix for one
chip's quirk is accepted only when the backend adapts to it at runtime, by detection, a
conditional path or a fallback, without changing behaviour on the others; `sta_channel`
reading the link frequency because Samsung omits the channel line, and `ensure_5g_ir` firing
only when the phy reports NO-IR, are the pattern. If the fix cannot be gated that way, it
belongs in a fork. Test on a real phone, there is no emulator for a Wi-Fi chip, and say in
your PR which devices, drivers, bands and upstreams you tested on; an untested claim counts
as untested.

## Building

```
./scripts/build-static.sh    # from the repository root: both arches, needs Docker
./build.sh [release]         # from Android/: the APK, needs JDK 17 or newer and ANDROID_HOME
```

What the scripts do, and what CI does, is in the Building section of
[How-It-Works.md](./Documentation/How-It-Works.md). A release is a tag: bump `VERSION`,
commit it as `VirtualAP: bump vX.Y.Z`, then push `main` and the tag `vX.Y.Z`.

## Code style

The four rules are in AGENTS.md, Style. On top of them:

- Backend: coreutils are `$ECHO`, `$GREP`, `$SED` and friends, never bare, and never quoted.
  Output is `log`, `warn`, `error`. Subcommands are `cmd_*`, Droidspaces helpers `ds_*`,
  radio probes `phy_*`, policy `pick_*` and `resolve_*`. Teardown mirrors setup step for step
  and every step tolerates absence. Each daemon logs to its own file under `logs/`.
- App: state lives in a ViewModel, not in a stateful composable and not in a `util`
  singleton. Lists render with `LazyColumn` and stable keys. Colours come from
  `MaterialTheme.colorScheme`, type from `MaterialTheme.typography`, timings from
  `AnimationUtils`, every other visual value from [DESIGN.md](./DESIGN.md). Any value that
  reaches a root shell goes through `Backend.quote()` or an allow-list validator.

Commits are signed off and prefixed as AGENTS.md, Commits, says.

## Reuse inventory

Grep this list before you write anything new. If something close exists, extend it rather
than adding a sibling. Android paths are relative to
`Android/app/src/main/java/com/virtualap/app/`; backend symbols live in `backend/start-ap`.
This list answers "what do I call"; DESIGN.md answers "what should it look like" when
nothing here fits. The shared components were ported from Droidspaces; when a caller for one
of its other components appears, copy the upstream file rather than writing a new one.

Forms and inputs:

- `DsDropdown(label, selected, options, displayName, onSelect, ...)` in
  `ui/component/DsDropdown.kt`: any select field. Never hand-roll `ExposedDropdownMenuBox`.
- `DsTextFieldDefaults.colors()` in `ui/component/DsTextFieldDefaults.kt`: every
  `OutlinedTextField`.
- `ClearFocusOnClickOutside` in `ui/util/FocusUtils.kt`: the main screen's root, which drops
  the keyboard on an outside tap.

Dialogs and sheets:

- `DsDialog(onDismiss, modifier) { }` in `ui/component/DsDialog.kt`: every dialog. Never set a
  width, padding or scroll.
- `DialogCloseButton(onClick, enabled)` in `ui/component/DialogCloseButton.kt`: the 36dp close
  square in a header row, for content that closes from the top.
- `LogActionRow(logs, isBlocking, onClear)` in `ui/component/LogActionRow.kt`: the Clear and
  Copy row above a log console; `onClear = null` gives Copy alone.
- The Wi-Fi QR sheet and the logs sheet are private to `ui/screen/MainScreen.kt`, the About
  dialog to `ui/screen/SettingsScreen.kt`. Do not import or copy them.

Bars, setup, feedback:

- `PrimaryActionBottomBar(label, icon, onClick, ...)` in
  `ui/component/PrimaryActionBottomBar.kt`: the Start and Stop bar. The overload taking a
  `content` slot is for a button that swaps its contents.
- `SetupPage`, `SetupFrame`, `SetupHero`, `SetupActionBar` in `ui/component/SetupHero.kt`:
  every screen of the setup flow, the welcome pager, the root check and the installer. See
  DESIGN.md, "The setup flow".
- `PullToRefreshWrapper(onRefresh) { }` in `ui/component/PullToRefreshWrapper.kt`: any pull to
  refresh screen body.
- `ToggleCard` in `ui/component/ToggleCard.kt`: a switch row in its own card, the main
  screen's options. `SwitchItem` in `ui/component/SwitchItem.kt`: the flat switch row inside a
  grouped surface, the Settings screen.
- `StatusPill(label, color)` in `ui/component/StatusPill.kt`: any small status chip.
  `SectionHeader(text)` in `ui/component/SectionHeader.kt`: a heading above a group of cards.
- `CardContentPadding`, `CardHeaderHeight` in `ui/component/CardMetrics.kt`: any card with a
  title-and-pill header, so the dividers line up.
- `LoadingIndicator(size, color)` with `LoadingSize`, and `FullScreenLoading(message)`, in
  `ui/util/LoadingIndicator.kt`: every spinner and every whole-screen loading state.
- `TerminalConsole(logs, isProcessing, maxHeight)` and `ShimmerAnimation(enabled) { }` in
  `ui/component/TerminalConsole.kt`: the scrolling log view and its frame.

Theme:

- `VirtualAPTheme(darkTheme, dynamicColor, amoledMode, themePalette)` in `ui/theme/Theme.kt`:
  the one theme root, applied in `MainActivity`.
- `rememberThemeState()` and `ThemeState` in `ui/theme/ThemeStateHolder.kt`: the live theme
  preferences.
- `ThemePalette` in `ui/theme/Color.kt`: adding an accent palette, here and nowhere else.
  `Theme.kt` and `Color.kt` are the only files that name a colour literally.
- `MaterialTheme.typography.*` and `JetBrainsMono` in `ui/theme/Type.kt`: all text styles,
  and the mono font for log and code text.
- `AnimationUtils` in `util/AnimationUtils.kt`: the tween specs. Never a literal `tween(300)`.
- `AccentColorPicker` and `ColorPaletteSwatch` in `ui/component/`: the palette picker.
- There is no spacing or shape token object; the numbers live in DESIGN.md and are written as
  literal dp.

Navigation:

- `Screen` and the `when` over it in `MainActivity.kt`: the one place a top-level screen is
  chosen, the welcome flag first, then root and install state, then the Settings flag. A new
  screen goes there.

Shell, root and the backend:

- `Backend.quote(value)`, `Backend.startAp`, `Backend.install(context)` in `util/Backend.kt`:
  every dynamic value that reaches a root command, the script's command prefix, and its
  extraction from the APK.
- `APManager.getStatus/start/stop/getContainers/getInterfaces/getCaps/isInstalled` in
  `util/APManager.kt`: driving `start-ap`. Never assemble one of these commands by hand.
- `Hotspot.start(cfg)/stop()/refresh()`, `phase`, `status`, `actionLogs` in `util/Hotspot.kt`:
  starting or stopping the AP from anywhere and reading the session state; also posts the
  running notification and wakes the tile.
- `APConfig` with `isValid()`, `isValidIpv4`, `isValidDnsList`, `channelsForBand` in
  `util/APConfig.kt`: the saved hotspot config, the one start validation, and the address
  checks the fields share with it.
- `BackendLogger` and `classifyLine(line)` in `util/BackendLogger.kt`: the log sink the AP
  calls take, and the `[ERROR]`/`[WARN]` level tagging.
- `PreferencesManager` with `apConfig` in `util/PreferencesManager.kt`: all settings; the
  hotspot config is read and written whole. `Constants` in `util/Constants.kt`: every path
  and preference key.
- `RootChecker.checkRootAccess(reprompt)` and `RootStatus` in `util/RootChecker.kt`;
  `VirtualAPInstaller.deviceArch/bundledPayloadVersion/payloadUpdateAvailable/install` in
  `util/VirtualAPInstaller.kt`; `QrCodeGenerator.wifiPayload/encode` in
  `util/QrCodeGenerator.kt`, the only zxing caller.
- `HotspotTileService` and `HotspotStopReceiver`: the Quick Settings toggle and the
  notification's Stop action. Both hold no state of their own.
- `AppViewModel` owns the welcome flag, root status and install state; `APViewModel` owns the editable config,
  the interface and container lists and the tailed log, and forwards start and stop to
  `Hotspot`. The 3 s status poll lives in `MainScreen`, bound to its lifecycle.
- The global libsu configuration lives in `VirtualAPApplication.kt`, and only there.

Backend:

- `log`, `warn`, `error`: every message. `save_conf`, `load_conf`, `sq()`: `ap.conf`.
  `hostapd_cli` (the function): every call to the binary.
- `detect_auto_table`, `table_oif`, `resolve_iface_table`, `resolve_upstream`: upstream
  table resolution. `sta_channel`, `is_dfs_channel`, `chan_no_ir`, `ensure_5g_ir`: radio
  probes and the NO-IR fix. `phy_idx`, `phy_info`, `phy_supports_vht`, `phy_supports_ht40`,
  `vht_seg0`, `ht40_dir`, `pick_width`: chip capabilities and 40/80 MHz geometry.
  `pick_channel`: band and channel policy. `calculate_subnet`: gateway to pool.
- `check_prerequisites`, `create_ap_iface`, `config_network`, `write_hostapd_conf`,
  `start_hostapd`, `start_services`, `teardown`, `stop_daemons`: routed mode and its mirror.
  `ds_run`, `ds_list`, `ds_container_pid`, `ds_is_openwrt`, `provision_openwrt`,
  `config_network_bridged`, `start_services_bridged`, `teardown_bridged`: managed mode.
- `pid_alive`, `kill_pidfile`, `del_ap_iface`, `del_bridge`, `del_rules`, `state`, `retry`,
  `wpa_block`, `follow_sta`, `dns_lines`: the pieces setup and teardown share.
- `cmd_start/stop/status/interfaces/containers/caps`: the subcommands. The app calls all six.

## Opening a PR

Fork, work in a branch, open the PR against `main`, and stay responsive: an unanswered PR is
closed, and feedback is addressed in the same PR, not a new one. There is no CLA; by
submitting you agree that your contribution may be distributed under the project's licence.

Every PR says:

1. What real-world problem it solves. "I wanted this" is not one; say what breaks, fails or
   is missing for real users on real hardware.
2. Screenshots or terminal output showing it working.
3. The tested environments: for the app, device, Android version and root provider; for the
   backend, device, SoC and Wi-Fi chip, driver, band and width, upstream (mobile data,
   Wi-Fi, VPN), and whether managed mode was exercised.
4. For UI changes, the DESIGN.md rules you followed, and which one you deviated from and
   why. A radius or a colour that disagrees with DESIGN.md without a reason is sent back.

What is merged: fixes with a reproduction and a verified resolution, features validated
across devices, security hardening, measured performance improvements, deletions, and
documentation corrections. What is rejected: a backend change that only parses under bash
or calls a coreutil outside the busybox variables; a prebuilt or dynamically linked binary;
a feature no real user has reported or that only works on one hardware configuration; code
its author cannot explain; an app change that breaks Android 8; a second copy of something
in the inventory; any dynamic value reaching a root shell unquoted; anything that regresses
what worked, however useful the new behaviour.

A merged feature is yours going forward: when a new Android version or driver breaks it, you
are expected to fix it, and a feature whose author is unreachable is removed. A contributor
whose PRs keep being rejected for the same reasons, no real problem, untested, or padding
the codebase, is blocked; there is no strike count, the decision is the maintainer's.

## Security

Anything that lets a hotspot client, or a value typed into the app, reach the root shell
unquoted is a vulnerability. Do not open a public issue; report it privately to
droidcasts@protonmail.com or [t.me/ravindu](https://t.me/ravindu) with a reproduction, the
affected configurations and the device or driver, and hold public disclosure until a fix is
out. Hardening patches are reviewed first.

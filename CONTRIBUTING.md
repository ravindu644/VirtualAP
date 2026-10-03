# Contributing to VirtualAP

## Philosophy

> A feature that doesn't exist is better than a broken implementation.

VirtualAP runs as root on rooted Android phones, on Wi-Fi chips whose drivers disagree about
basic things: whether `iw dev info` reports a channel, whether the regulatory domain can be
set at all, whether 5 GHz can beacon while the station is connected. A patch that works on
your phone and breaks on someone else's is not a contribution, it is a regression. Every
change to the backend must uphold this contract without exception.

The second half of that philosophy is about the codebase itself. Ten lines that do the job
beat a hundred that do the same job. Reuse beats rewriting. The largest cleanup this project
ever needed came from writing new code beside existing code instead of extending it: the main
screen held 1,200 lines of private copies of components Droidspaces already shared. The
[Reuse Inventory](#reuse-inventory) below exists so you never have to guess whether
something already exists.

## Platform Scope

VirtualAP is Android only. The backend runs as root under Android's `/system/bin/sh`, with
the bundled static busybox for coreutils and Android's own `/system/bin/ip` and `iptables`
for routing and firewalling. There is no Linux desktop target, no chroot and no container.
Anything that would only work with GNU coreutils, bash, or a busybox `ip` does not belong in
`start-ap`.

## Wi-Fi Driver Coverage

VirtualAP runs on Qualcomm, Broadcom FullMAC (Samsung), and MediaTek Wi-Fi chips, under
vendor drivers that disagree with mainline nl80211 in small ways. Your patch must be tested
across a representative spread of this landscape before submission.

State explicitly in your PR which devices and drivers you have tested on. Untested claims of
compatibility will be treated as untested.

Patches that address a quirk specific to one chip or vendor driver are acceptable **only if
VirtualAP can adapt to the quirk at runtime**, via detection, a conditional code path, or a
graceful fallback, without regressing behavior on unaffected hardware. If the fix cannot be
generalized in this way, it belongs in a downstream fork, not in core. `sta_channel` reading
the link frequency because Samsung omits the channel line, and `ensure_5g_ir` firing only
when the phy reports NO-IR, are the pattern.

## Android App Changes

The app has a minimum requirement of **Android 8 (API 26)**. Changes to the Android app must
not introduce any dependency, API call, or behavior that breaks on Android 8.

Test on Android 8 before opening a PR. Testing only on a recent Android release is not
sufficient.

## Backend Changes

`backend/start-ap` is strict POSIX sh plus `local`, and must parse under both Android's
`/system/bin/sh` and busybox `sh`. Test on a real phone; there is no emulator for a Wi-Fi
chip. The static binaries come only from `scripts/build-static.sh`; never commit one, never
link one dynamically.

## Building

### Static binaries

From the repository root:

```
./scripts/build-static.sh    # aarch64 + armhf, Alpine 3.23 under QEMU
```

Requires Docker; the script registers the QEMU binfmt handlers itself on first run. It
initialises the `externals/` submodules (our forks of hostapd, iw and dnsmasq), builds each
arch in its own container and stages the result into `backend/<arch>/`, which is gitignored.
Adding a tool means a submodule under `externals/` and a build block in
`scripts/build-in-container.sh`, for both arches.

### Android app

From `Android/`:

```
./build.sh             # debug APK
./build.sh release     # signed release APK
```

Use the script rather than calling gradle directly. It sets up the wrapper, cleans, and
places the APK where the rest of the tooling expects it. The build needs JDK 17; a newer JDK
fails inside gradle 8.2's script compiler, so point `JAVA_HOME` at a 17 or build in an Ubuntu
24.04 container with `openjdk-17-jdk`.

The version string is `VERSION` at the repository root; `versionCode` is derived from it in
`build.gradle.kts`.

### CI

`.github/workflows/ci.yml` builds the static binaries for each arch in its own job on an ARM
runner (`scripts/build-static.sh <arch>`, native, no QEMU), then a second job runs JDK 17 and
`./gradlew assembleRelease` from `Android/`. The binaries are cached and rebuilt from source
only when a submodule under `externals/` or one of the two build scripts changes. A
`workflow_dispatch` with `create_release` cuts a GitHub release from that APK. It does **not**
run a formatter or a linter.

## Code Style

### No em-dashes

Not in code, not in comments, not in commit messages, not in documentation. Use a comma, a
full stop, or rewrite the sentence.

### No ASCII banner comments

No rows of `-----`, no `=====`, no boxed section headers. The `# --- Section ---` headers in
`backend/start-ap` and `scripts/build-in-container.sh` predate this rule and are being
removed separately. Do not add more.

### Comments sound like a person

Say why the code does what it does, or what breaks if it does not. A comment that restates
the line below it is noise.

```sh
# Bad
# bump the priority
PRIO_FROM_AP=7010

# Good
# netd's own rules start at 10000, so 7010 wins before the VPN catch-all
PRIO_FROM_AP=7010
```

The backend already does this well in places. `ensure_5g_ir` in `backend/start-ap` explains
the world-regdomain race, why `iw reg set` cannot fix it, and why it does not wait for the
reply. That is the bar.

### Ten lines beat a hundred

Delete before you add. The smallest change in the right place is the goal. The smallest
change in the wrong place is a second bug.

### Shell conventions

- Coreutils are `$ECHO`, `$GREP`, `$SED` and friends, never bare. The variables are unquoted
  on purpose: each one word-splits to `busybox <applet>`.
- Output is `log`, `warn`, `error`. Never bare `echo`.
- Every value written to `ap.conf` goes through `sq()` inside `save_conf`.
- Subcommands are `cmd_*`, Droidspaces helpers are `ds_*`, radio probes are `phy_*`, policy
  is `pick_*` and `resolve_*`.
- Teardown mirrors setup step for step and every step tolerates absence (`|| true`,
  `2>/dev/null`).
- Each daemon logs to its own file under `logs/` so a crash leaves a trace.

### Kotlin conventions

- State lives in a ViewModel, not in a stateful composable and not in a `util` singleton.
- Lists render with `LazyColumn` and stable keys.
- Colors come from `MaterialTheme.colorScheme`, type from `MaterialTheme.typography`, animation
  timings from `AnimationUtils`. Do not hardcode them. Corner radii, spacing and every other
  visual value come from [DESIGN.md](./DESIGN.md).
- Any value that reaches a root shell goes through `Backend.quote()` or an allow-list
  validator. No exceptions.

## Commit Conventions

- Sign off every commit: `git commit -s`.
- Do not add a `Co-Authored-By:` trailer for an AI agent. Human co-authors are fine.
- Write a subject in the imperative mood, and a body explaining why when the change is not
  self-evident.

| Prefix | Use for |
| --- | --- |
| `app:` | Android app changes, with `app: fix:` and `app: refactor:` for those cases |
| `backend:` | `backend/start-ap` changes |
| `fix:` | backend bug fixes |
| `scripts:` | the static binary build, `scripts/` and `externals/` |
| `ci:` | `.github/workflows/` |
| `docs:` | documentation |
| `fix(security):` | anything security related, either half |
| `VirtualAP: bump vX.Y.Z` | version bumps, nothing else in the commit |

## Reuse Inventory

Grep this list before you write anything new. If something close already exists, extend it
rather than adding a sibling. Android paths are relative to
`Android/app/src/main/java/com/virtualap/app/`, backend symbols live in `backend/start-ap`.

This list answers "what do I call". [DESIGN.md](./DESIGN.md) answers "what should it look like",
for the case where nothing here fits and you have to build something new.

The shared components were ported verbatim from Droidspaces. A few of its components have no
caller here and were deliberately not ported: `ProgressDialog`, `ErrorLogsDialog`,
`SnackbarUtils`, `DsSnackbarHost`, `SettingsRowCard`, `ErrorState`, `TerminalDialog`. When a
caller appears, copy the upstream file rather than writing a new one.

### Android: forms and inputs

| Symbol | Path | Use it when |
| --- | --- | --- |
| `DsDropdown(label, selected, options, displayName, onSelect, ...)` | `ui/component/DsDropdown.kt` | Any select field. Never hand-roll `ExposedDropdownMenuBox` |
| `DsMenuTheme { }` + `Modifier.dsMenuBorder()` | `ui/component/DsMenuTheme.kt` | Any `DropdownMenu` that needs the opaque menu surface. `DsDropdown` already applies it |
| `DsTextFieldDefaults.colors()` / `.surfaceColors()` | `ui/component/DsTextFieldDefaults.kt` | Every `OutlinedTextField`. `colors()` on screens, `surfaceColors()` inside dialogs |
| `FocusUtils`, `rememberClearFocus()`, `ClearFocusOnClickOutside` | `ui/util/FocusUtils.kt` | IME actions and dismissing the keyboard on outside taps |

### Android: dialogs

| Symbol | Path | Use it when |
| --- | --- | --- |
| `DsDialog(onDismiss, modifier, borderColor, scrollableContent, footer) { }` | `ui/component/DsDialog.kt` | Every dialog. Actions go in `footer`, never in the content, or they get squeezed off a short screen. Never set a width, padding or scroll |
| `DialogDismissButton(label, onDismiss)` | `ui/component/DialogFooterRow.kt` | The `footer` of a dialog whose only action is close |
| `DialogCloseButton(onClick, enabled)` | `ui/component/DialogCloseButton.kt` | The 36.dp close square in a header row, for content that closes from the top (the logs sheet) |
| `DialogFooterRow(dismissLabel, confirmLabel, onDismiss, onConfirm, confirmEnabled, destructive)` | `ui/component/DialogFooterRow.kt` | Every dialog's cancel and confirm row. Pass `destructive = true` for a delete or a wipe, never a colour |
| `LogActionRow(logs, isBlocking, onClear)` + `copyLogsToClipboard(context, logs)` | `ui/component/LogActionRow.kt` | The Clear and Copy row above a log console. `onClear = null` gives Copy alone |

The Wi-Fi QR sheet and the logs sheet are private to `ui/screen/MainScreen.kt`, and the About
dialog to `ui/screen/SettingsScreen.kt`. Do not import or copy them.

### Android: bars, scaffolds, feedback

| Symbol | Path | Use it when |
| --- | --- | --- |
| `PrimaryActionBottomBar(label, icon, onClick, ...)` | `ui/component/PrimaryActionBottomBar.kt` | The Start and Stop bar on the main screen and the setup flow's Done and Retry. The overload taking a `content` slot is for buttons that swap their contents |
| `PullToRefreshWrapper(onRefresh) { ... }` | `ui/component/PullToRefreshWrapper.kt` | Any pull to refresh list or screen body |

### Android: cards and list items

| Symbol | Path | Use it when |
| --- | --- | --- |
| `SettingsCard(title, onClick, icon, subtitleContent, trailing, ...)` | `ui/component/SettingsCard.kt` | The base for every settings or option row. Build new variants on top of it |
| `ToggleCard` | `ui/component/ToggleCard.kt` | A switch row inside a card. A thin wrapper over `SettingsCard` |
| `SwitchItem` | `ui/component/SwitchItem.kt` | Flat switch row inside a grouped Surface, used on the Settings screen. See the duplicates note below |
| `EmptyState(icon, title, description)`, `RootUnavailableState` | `ui/component/EmptyState.kt` | Any empty list or missing root state |

### Android: status and indicators

| Symbol | Path | Use it when |
| --- | --- | --- |
| `StatusPill(label, color, busy)` | `ui/component/StatusPill.kt` | Any small status chip or badge |
| `SectionHeader(text)` | `ui/component/SectionHeader.kt` | Any heading above a group of cards. Spacing goes on the modifier |
| `CardContentPadding`, `CardHeaderHeight` | `ui/component/CardMetrics.kt` | Any card with a title-and-pill header. Keeps the dividers aligned between cards, do not retype the values |
| `LoadingIndicator(size, color)` + `LoadingSize` | `ui/util/LoadingIndicator.kt` | Inline spinners. Pick a `LoadingSize`, never a raw `.size(n.dp)` |
| `FullScreenLoading(message)` | `ui/util/LoadingIndicator.kt` | Whole screen loading state |
| `ContainedLoadingIndicator`, `LoadingIndicatorDefaults`, `MaterialShapes` | `ui/util/LoadingIndicator.kt` | Determinate and morphing indicators, and their tokens |
| `TerminalConsole(logs, isProcessing, maxHeight)` | `ui/component/TerminalConsole.kt` | Inline scrolling log view |
| `ShimmerAnimation(enabled) { ... }` | `ui/component/TerminalConsole.kt` | Skeleton loading effect |

### Android: theme

| Symbol | Path | Use it when |
| --- | --- | --- |
| `VirtualAPTheme(darkTheme, dynamicColor, amoledMode, themePalette)` | `ui/theme/Theme.kt` | The single theme root, applied in `MainActivity` |
| `rememberThemeState()` + `ThemeState` | `ui/theme/ThemeStateHolder.kt` | Reading live theme preferences |
| `ThemePalette` | `ui/theme/Color.kt` | Adding an accent palette. Here and nowhere else |
| `MaterialTheme.colorScheme.*` | | All colors. `ui/theme/Color.kt` holds only `AMOLED_BLACK` and the palettes |
| `MaterialTheme.typography.*`, `JetBrainsMono` | `ui/theme/Type.kt` | All text styles, and the mono font for log and code text |
| Corner radii, spacing, type roles | [DESIGN.md](./DESIGN.md) | Every visual value. There is no shape token object, the numbers live in DESIGN.md |
| `AnimationUtils` | `util/AnimationUtils.kt` | Durations, easing, and tween specs. Never a literal `tween(300)` |
| `AccentColorPicker`, `ColorPaletteSwatch` | `ui/component/` | The palette picker in settings |

There is no spacing token object. Padding is written as literal dp, following the existing
conventions: 24.dp for dialog and screen horizontal padding, 16.dp for card inner padding,
8.dp and 12.dp between rows.

### Android: navigation

| Symbol | Path | Use it when |
| --- | --- | --- |
| `Screens` | `ui/navigation/VAPNavigation.kt` | Adding a destination. Never a raw route string |
| The `NavHost` in `MainActivity.kt` | `MainActivity.kt` | The single graph, with the root and install gating effect below it. Register new screens here |

### Android: shell and root execution

| Symbol | Path | Use it when |
| --- | --- | --- |
| `Backend.quote(value)` | `util/Backend.kt` | **Every** dynamic value going into a root command. POSIX single-quote wrap that escapes embedded quotes |
| `Backend.startAp`, `Backend.install(context)` | `util/Backend.kt` | The script's command prefix and its extraction from the APK |
| `APManager.getStatus/start/stop/getContainers/getInterfaces/isInstalled` | `util/APManager.kt` | Driving `start-ap`. The only class that runs it. Do not assemble these by hand |
| `Hotspot.start(cfg)/stop()/refresh()`, `phase`, `status`, `actionLogs` | `util/Hotspot.kt` | Starting or stopping the AP from anywhere, and reading the session state. The only caller of `APManager.start/stop`; also posts the running notification and wakes the tile |
| `APConfig.fromPrefs(prefs)`, `isValid()`, `passwordValid()`, `validChannelForBand` | `util/APConfig.kt` | The saved hotspot config and the one start validation |
| `BackendLogger` / `ViewModelLogger(onLog)` + `classifyLine(line)` | `util/BackendLogger.kt` | The log sink the installer and the AP calls take, and the `[ERROR]`/`[WARN]` level tagging |

The global libsu configuration lives in `VirtualAPApplication.kt`. That is the only place it
should be set.

### Android: data and preferences

| Symbol | Path | Use it when |
| --- | --- | --- |
| `PreferencesManager.getInstance(context)`, `saveApConfig(...)` | `util/PreferencesManager.kt` | All settings persistence |
| `Constants` | `util/Constants.kt` | Every path, preference key, and default. Never re-declare a literal |

### Android: device and platform

| Symbol | Path | Use it when |
| --- | --- | --- |
| `RootChecker.checkRootAccess()` / `RootStatus` | `util/RootChecker.kt` | Root availability |
| `VirtualAPInstaller.deviceArch/bundledPayloadVersion/payloadUpdateAvailable/install` | `util/VirtualAPInstaller.kt` | Deploying the arch's binaries to `/data/local/virtualap` and detecting a new payload after an APK update |
| `QrCodeGenerator.wifiPayload/encode` | `util/QrCodeGenerator.kt` | The Wi-Fi join code. The only zxing caller |
| `HotspotTileService` | `HotspotTileService.kt` | The Quick Settings toggle. Optimistic state like AOSP's HotspotTile; holds no state of its own |
| `HotspotStopReceiver` | `HotspotStopReceiver.kt` | The notification's Stop action |
| `AnsiColorParser.parseAnsi/stripAnsi` | `util/AnsiColorParser.kt` | Rendering or cleaning ANSI output |

### Android: ViewModels

| Symbol | Path | Owns |
| --- | --- | --- |
| `AppViewModel` | `ui/viewmodel/AppViewModel.kt` | Root status and backend install state |
| `APViewModel` | `ui/viewmodel/APViewModel.kt` | Status polling, the editable config, interface and container lists, the tailed log. Start, stop and the session state are forwarded to `Hotspot` |

### Backend: logging and config

| Symbol | Use it when |
| --- | --- |
| `log`, `warn`, `error` | Every message. Stdout is what the app shows live; `logs/ap.log` is the timestamped copy for a shell |
| `save_conf`, `load_conf`, `sq()` | Persisting and escaping `ap.conf` |
| `$CAT` ... `$WC` applet variables | Every coreutil. Unquoted |
| `hostapd_cli` (function) | Every call to the binary. It supplies the `-s` client socket dir Android lacks; the station count in `cmd_status` and `ensure_5g_ir` use it |

### Backend: upstream, radio, channel

| Symbol | Use it when |
| --- | --- |
| `detect_auto_table`, `table_oif`, `resolve_iface_table`, `resolve_upstream` | Upstream table resolution. The only place |
| `sta_channel`, `reg_country`, `is_dfs_channel`, `chan_no_ir`, `ensure_5g_ir` | Radio state probes and the NO-IR country fix |
| `phy_idx`, `phy_info`, `phy_supports_vht`, `phy_supports_ht40`, `vht_seg0`, `ht40_dir`, `pick_width` | Chip capabilities and 40/80 MHz geometry |
| `pick_channel` | Band and channel policy. Nowhere else |
| `calculate_subnet` | Gateway to pool derivation |

### Backend: lifecycle and managed mode

| Symbol | Use it when |
| --- | --- |
| `check_prerequisites`, `create_ap_iface`, `config_network`, `write_hostapd_conf`, `start_hostapd`, `start_services`, `teardown`, `stop_daemons` | Routed mode setup and its mirror |
| `ds_run`, `ds_list`, `ds_container_pid`, `ds_is_openwrt`, `provision_openwrt`, `config_network_bridged`, `start_services_bridged`, `teardown_bridged` | Managed mode |
| `cmd_start/stop/status/leases/interfaces/containers/caps` | Subcommands. The app calls `status`, `start`, `stop`, `interfaces`, `containers` |

## Known duplicates: do not add a third copy

These exist today. They are on the cleanup list. Extend the shared version, do not add
another.

- `ToggleCard` and `SwitchItem` are two shapes of the same switch row.
- `labelFontSize` on `PrimaryActionBottomBar` has no caller here. Either the type scale
  covers it or the parameter should go. Do not add a caller.
- `VirtualAPInstaller` interpolates constant paths into shell strings without quoting
  (`chmod 755 ${Constants.VAP_DIR}/bin/*`, the `rm -f` and `cp` line). They are constants, not
  input, but do not copy the pattern; use `Backend.quote()`.

## Adding something new

1. Search the inventory above, then grep the tree. Something close usually exists.
2. Extend the shared thing. Adding a parameter to one component beats adding a sibling
   component.
3. If you are about to copy a block and change two fields, parameterize it instead.
4. Fix bugs at the choke point every caller routes through, not at the one call site the
   report happens to name.
5. Never add a second way to build a shell command.
6. New state goes in a ViewModel, not in a composable and not in a `util` object.

## PR Requirements

Every feature PR must include:

1. **A clear description of the real-world problem being solved.** "I wanted this" is not a
   problem statement. Explain what breaks, fails, or is missing for real users on real
   hardware.

2. **Screenshots or terminal output** demonstrating the feature working as intended.

3. **Explicit list of tested environments.** For the app: device name, Android version, root
   provider. For the backend: device, SoC and Wi-Fi chip, driver, band and width tested,
   upstream tested (mobile data, Wi-Fi, VPN), and whether managed mode was exercised.

4. **No regressions.** Run the existing behavior through your change. If something that
   worked before no longer works, fix it before opening a PR.

5. **For UI changes, the DESIGN.md rules you followed.** If you deviated from one, say which and
   why. A radius or a colour that disagrees with [DESIGN.md](./DESIGN.md) without a reason gets
   sent back.

## Code Ownership

If your feature is merged, you are responsible for it going forward.

When a new Android version, a new driver quirk, or a platform behavior change breaks your
contribution, you are expected to address it. If a feature you submitted starts causing
issues and you are unreachable or unwilling to maintain it, it will be removed.

Users do not know who wrote a feature. When something breaks, they blame the project.
Understand what your code does before submitting it. If you cannot explain why a specific
implementation choice was made, that choice should not be in production.

## What Gets Merged

- Features that solve a real problem and are validated across multiple devices.
- Bug fixes with a clear reproduction case and a verified resolution.
- Security improvements. These are always welcome.
- Performance improvements with measurable, non-regressing impact.
- Deletions. Removing duplicated code or dead flexibility is a contribution.
- Documentation corrections.

## What Gets Rejected

- A backend change that only parses under bash, or that calls a coreutil outside the busybox
  variables.
- A prebuilt or dynamically linked binary.
- Features that solve a problem no real user has reported or that cannot be reproduced
  outside a narrow hardware configuration.
- Code the author cannot explain or defend under review.
- App changes that break Android 8 compatibility.
- A new component or helper that duplicates one already in the inventory above.
- Any dynamic value reaching a root shell without quoting or an allow-list.
- Anything that introduces a regression, regardless of how useful the new behavior is.

## Repeat Rejections

If a contributor submits multiple PRs that are rejected for the same reasons, features that
solve no real problem, fail universality requirements, or add unnecessary complexity to the
codebase, they will be blocked from contributing further.

There is no fixed strike count. The threshold is pattern recognition: if it is clear that a
contributor is not reading feedback, not testing properly, or is deliberately padding the
codebase, the decision to block is at maintainer discretion and is final.

## Security Vulnerabilities

Security fixes and hardening patches are always welcome and will be reviewed with priority.

If you discover a vulnerability, particularly **anything that lets a hotspot client, or a
value typed into the app, reach the root shell unquoted**, do not open a public issue.

Report it privately:

- **Email:** droidcasts@protonmail.com
- **Telegram:** [t.me/ravindu](https://t.me/ravindu)

Include a reproduction case, affected configurations, and device or driver details if
relevant. Public disclosure should wait until a fix is available.

## Process

1. Fork the repository and work in a dedicated branch.
2. Open a PR against `main` with the information described above.
3. Be responsive during review. Unresponsive PRs will be closed.
4. Address review feedback directly. Do not open a new PR for the same change.

There is no formal CLA. By submitting a PR you agree that your contribution may be
distributed under the project's existing license.

AI agents working in this repository should read [AGENTS.md](./AGENTS.md), which is the short
form of the rules above.

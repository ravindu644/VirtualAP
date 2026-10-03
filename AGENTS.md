# AGENTS.md

Instructions for AI coding agents working in this repository. Humans should read
[CONTRIBUTING.md](./CONTRIBUTING.md), which carries the full inventory of what already
exists here.

## What this is

VirtualAP turns a rooted phone into a Wi-Fi access point. Two halves:

- `backend/start-ap` is the engine, one POSIX sh script that runs as root, plus the fully
  static binaries it drives (`hostapd`, `hostapd_cli`, `iw`, `dnsmasq`, `busybox`). The
  binaries are built from the git submodules under `externals/` by `scripts/build-static.sh`
  into `backend/aarch64/` and `backend/armhf/`, which are gitignored.
- `Android/` is the Compose app that drives that script over a root shell.

The `prepareAssets` gradle task stages the script and both arch trees into
`Android/app/src/main/assets/` at build time, and the app deploys the matching arch to
`/data/local/virtualap` on first run and after every APK update. There is no C backend, no
chroot and no Magisk module.

## Build

Static binaries, from the repository root:

```
./scripts/build-static.sh    # aarch64 + armhf, Alpine 3.23 under QEMU, needs Docker
```

It initialises the `externals/` submodules, builds each arch in its own container and stages
the result into `backend/<arch>/`. Those directories are gitignored; `prepareAssets` fails
loudly when they are empty, so run this once before the first app build and again whenever
`externals/` or `scripts/build-in-container.sh` change.

Android app, from `Android/`:

```
./build.sh             # debug
./build.sh release     # signed release
```

Use the script, not gradle directly. The build needs JDK 17 (`jvmTarget = "17"`); a newer
JDK does not work, so point `JAVA_HOME` at a 17 (on the maintainer's Fedora host that is
`~/.jdks/jdk-17`, with `ANDROID_HOME=~/Android/Sdk`), or build inside an Ubuntu 24.04
container with `openjdk-17-jdk`. CI builds each arch with `scripts/build-static.sh <arch>` on
an ARM runner and then runs `./gradlew assembleRelease`. The binaries never come from the
repo: they are built from source and cached, and rebuilt whenever a submodule under
`externals/` or one of the two build scripts changes. CI does not check formatting or lint
anything.

The version comes from `VERSION` at the repository root. Bump it there, nowhere else.

## Commits

- Sign off every commit: `git commit -s`.
- Never add a `Co-Authored-By:` trailer for an AI agent. Human co-authors are fine.
- Prefixes, matching the existing history:
  - `app:` for the Android app, with `app: fix:` and `app: refactor:` for those cases
  - `backend:` for `backend/start-ap`, `fix:` for a backend bug fix
  - `scripts:` for the static binary build under `scripts/` and `externals/`, `ci:` for
    `.github/workflows/`, `docs:` for documentation
  - `fix(security):` for anything security related, either half
  - `VirtualAP: bump vX.Y.Z` for a version bump, and nothing else in that commit

`ap:`, `build:`, `ui:`, `feat:` and `refactor:` appear in older history. Do not use them for
new commits.

## Style

Four rules. They apply to code, comments, commit messages, and documentation.

**No em-dashes.** Use a comma, a full stop, or rewrite the sentence.

**No ASCII banner comments.** No rows of `-----`, no `=====`, no boxed section headers. The
`# --- Section ---` headers in `backend/start-ap` and `scripts/build-in-container.sh` predate
this rule and are being removed separately, do not add more.

**Comments sound like a person.** Say why the code does something, or what breaks if it
does not. Skip comments that restate the line below them.

```sh
# Bad: bump the priority
PRIO_FROM_AP=7010

# Good: netd's own rules start at 10000, so 7010 wins before the VPN catch-all
PRIO_FROM_AP=7010
```

**Ten lines that work beat a hundred that do the same thing.** Delete before you add. A
smaller diff in the right place is the goal, not a smaller diff anywhere.

## UI changes

Anything visual in `Android/` follows [DESIGN.md](./DESIGN.md). Colours, type, spacing, radii and
the action pill pattern are all specified there, and they were read out of the existing app rather
than invented, so following them is also the smallest diff.

If a rule genuinely does not fit your case, deviate, but leave a comment at the site saying what
the deviation buys and say it in the PR. An undocumented deviation is drift and the next
contributor will "fix" it. DESIGN.md's "Decided exceptions" section lists the deviations that
are decisions, do not "fix" those.

## Before you write it at all

Work down this list and stop at the first answer that holds.

1. Does this need to exist? A flag nobody asked for, a knob for a value that never changes,
   an interface with one implementation: skip it and say so in one line.
2. Does it already exist here? See "Reuse before you write" below. Grep first.
3. Does the platform already do it? busybox applets and POSIX sh on the backend, plus
   Android's own `/system/bin/ip` and `iptables`; the Kotlin and Java stdlib and the Android
   framework on the app side. Mind busybox sh and `minSdk = 26`.
4. Does an installed dependency cover it? libsu, Compose, navigation, lifecycle, zxing on
   the app side. Never add a new one for what a few lines can do.
5. Can it be one line? Then it is one line.
6. Only then write the smallest thing that works.

Two answers work? Take the higher one and move on.

The list shortens the solution, never the reading. Trace the flow the change touches before
you pick a rung. A small diff in the wrong place is a second bug, not a lazy fix. Same for
bug reports: a report names a symptom, so grep every caller before you edit. One guard in
the shared function is smaller than a guard in each caller, and it fixes the siblings the
report did not mention.

Never simplify away input validation at a trust boundary, a fail-closed check, error
handling that loses state, or anything the requester asked for by name.

## Reuse before you write

The app's main screen once held 1,200 lines of private copies of components its sibling
project Droidspaces already shared: dropdowns, a status pill, a settings card, fake buttons,
a bottom bar. The port that replaced them is why `CONTRIBUTING.md` has a Reuse Inventory.
Grep before you write.

These are choke points. Bypassing one is a bug, not a shortcut.

- `Backend.quote()` in `Android/app/src/main/java/com/virtualap/app/util/Backend.kt` wraps
  every dynamic value that reaches a root shell. `APManager` is the only class that runs
  `start-ap`, and `Backend.startAp` is the only way to spell its path.
- `Hotspot` is the only caller of `APManager.start` and `APManager.stop`. It owns the
  starting/stopping phase, the command log, the running notification and the Quick Settings
  tile sync, so the screen and the tile always agree. `APConfig.isValid()` is the only
  start validation; the screen's button and the tile both ask it.
- Coreutils in `backend/start-ap` go through the bundled busybox: `$CAT`, `$CUT`, `$DATE`,
  `$ECHO`, `$GREP`, `$HEAD`, `$ID`, `$KILL`, `$MKDIR`, `$PRINTF`, `$RM`, `$SED`, `$SLEEP`,
  `$TR`, `$WC`. Each one word-splits to `busybox <applet>`, so never quote one and never call
  a bare `grep`. `ip` and `iptables` are Android's own, `$IP` and `$IPT`. `iw`, `hostapd` and
  `dnsmasq` are our static binaries, `$IW`, `$HOSTAPD`, `$DNSMASQ`.
- Logging is `log`, `warn`, `error`. They print to stdout, which the app streams into its
  log sheet, and append a timestamped copy to `logs/ap.log` for reading from a shell. The
  app never reads that file. A bare `echo` is invisible to the user.
- `hostapd_cli` is a shell function in `backend/start-ap`, not the binary. The binary puts
  its reply socket under `/tmp`, which Android does not have, so a direct call fails before
  it connects. The function adds `-s "$RUN_DIR"`; the client count and the NO-IR country
  command both go through it.
- Config is written by `save_conf`, and every value in it passes through `sq()`, the
  single-quote escaper. Never append to `ap.conf` by hand. A new key is a default at the top
  of the script, a line in `save_conf`, and a flag in `cmd_start`.
- `resolve_upstream` is the only place the upstream routing table is resolved.
  `pick_channel`, with `pick_width` and `ensure_5g_ir`, is the only place band and channel
  policy lives: following the station's channel, the DFS fallback, the NO-IR country fix. Do
  not compare a channel number anywhere else.
- `Constants` holds every path and preference key on the app side. `Backend` extracts the
  script; `VirtualAPInstaller` deploys the binaries. Neither is duplicated elsewhere.

## Hard constraints

- The app targets `minSdk = 26`. Test on Android 8 behaviour before assuming an API exists.
- `backend/start-ap` is strict POSIX sh plus `local`. It runs under Android's
  `/system/bin/sh` and must also parse under busybox `sh`. No arrays, no `[[`, no
  `${var//}`, no `function` keyword, no bashisms.
- Binaries are fully static musl builds from the `externals/` submodules, both arches. Never
  a dynamically linked binary, never a prebuilt from elsewhere. A new tool is a new submodule
  plus a block in `scripts/build-in-container.sh`.
- No new dependency when an installed one covers it. The app already has libsu, Compose,
  navigation, lifecycle, and zxing.
- Samsung's Broadcom FullMAC driver is the floor for wireless assumptions.
  `iw dev <iface> info` has no channel line, so the station channel comes from the `freq:`
  line of `iw dev <iface> link` (`sta_channel`). The wiphy is self-managed, so `iw reg set`
  and hostapd's `country_code` are ignored; the only lever is the driver's private `COUNTRY`
  command over the supplicant socket (`ensure_5g_ir`). DFS channels 52 to 144 cannot beacon.
  A quirk fix that is not gated on runtime detection does not go in.

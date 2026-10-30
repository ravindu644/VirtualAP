# AGENTS.md

Instructions for AI coding agents working in this repository. Humans should read
[CONTRIBUTING.md](./CONTRIBUTING.md), which carries the inventory of what already exists
here. How the thing works is in
[Documentation/How-It-Works.md](./Documentation/How-It-Works.md).

## What this is

VirtualAP turns a rooted phone into a Wi-Fi access point. Two halves:

- `backend/start-ap` is the engine, one POSIX sh script that runs as root, plus the fully
  static binaries it drives (`hostapd`, `hostapd_cli`, `iw`, `dnsmasq`, `busybox`). The
  binaries are built from the git submodules under `externals/` by `scripts/build-static.sh`
  into `backend/aarch64/` and `backend/armhf/`, which are gitignored.
- `Android/` is the Compose app that drives that script over a root shell.

The `prepareAssets` gradle task stages the script and both arch trees into a generated
assets root under `Android/app/build/`, and the app deploys the matching arch to
`/data/local/virtualap` on first run and after every APK update. There is no C backend, no
chroot and no Magisk module.

## Build

```
./scripts/build-static.sh    # from the repository root: both arches, needs Docker
./build.sh [release]         # from Android/: the APK, needs JDK 17 or newer and ANDROID_HOME
```

Run the first once before the first app build and again whenever `externals/` or the build
scripts change; `prepareAssets` fails when `backend/<arch>/` is empty. The version comes
from `VERSION` at the repository root. Bump it there, nowhere else. The build and CI
details are in the Building section of How-It-Works.md.

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

**No ASCII banner comments.** No rows of `-----`, no `=====`, no boxed section headers.

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

In the documents, every fact is stated once, on the page where a reader looks for it, and
the other pages link to it. The split is fixed: README.md says what, for whom and the steps;
How-It-Works.md holds every technical fact; this file holds the rules for agents;
CONTRIBUTING.md says how a human gets a change merged and holds the inventory; DESIGN.md
holds the UI values. DESIGN.md's tables stay, they are role maps and sizes that do not work
as prose; elsewhere a list or a paragraph.

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
4. Does an installed dependency cover it? libsu, Compose, lifecycle, zxing on
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

Grep the inventory in CONTRIBUTING.md, then the tree. These are choke points. Bypassing one
is a bug, not a shortcut.

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
  lifecycle, and zxing.
- Samsung's Broadcom FullMAC driver is the floor for wireless assumptions, and MediaTek's
  gen4m the second; what each one lacks is in "The radio" in How-It-Works.md. A quirk fix
  that is not gated on runtime detection does not go in.

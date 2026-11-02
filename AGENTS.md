# AGENTS.md

Instructions for AI coding agents working in this repository. Humans should read
[CONTRIBUTING.md](./CONTRIBUTING.md), which carries the inventory of what already exists
here. How the thing works is in
[Documentation/How-It-Works.md](./Documentation/How-It-Works.md).

## What this is

VirtualAP turns a rooted phone into a Wi-Fi access point. Two halves:

- `src/` is the engine, `virtualap`, a static C binary that runs as root and drives two
  more: `hostapd` and `dnsmasq`, built from the git submodules under `externals/` by
  `scripts/build-static.sh`. `make aarch64 armhf` builds `virtualap`. All three land in
  `backend/aarch64/` and `backend/armhf/`, which are gitignored.
- `Android/` is the Compose app that drives `virtualap` over a root shell.

The `prepareAssets` gradle task stages both arch trees into a generated assets root under
`Android/app/build/`, and the app deploys the matching arch to `/data/local/virtualap` on
first run and after every APK update. There is no chroot and no
Magisk module.

## Build

```
./scripts/build-static.sh    # from the repository root: both arches, needs Docker
make aarch64 armhf           # from the repository root: virtualap, needs musl-cross gcc
./build.sh [release]         # from Android/: the APK, needs JDK 17 or newer and ANDROID_HOME
```

Run the first two before the first app build; after that, the first again whenever
`externals/` or the build scripts change, the second whenever `src/` does. `prepareAssets`
fails when `backend/<arch>/` lacks one of the three. `make check` runs the tests. The version
comes from `VERSION` at the repository root. Bump it there, nowhere else. The build and CI
details are in the Building section of How-It-Works.md.

## Commits

- Sign off every commit: `git commit -s`.
- Never add a `Co-Authored-By:` trailer for an AI agent. Human co-authors are fine.
- Prefixes, matching the existing history:
  - `app:` for the Android app, with `app: fix:` and `app: refactor:` for those cases
  - `backend:` for the engine under `src/`, `fix:` for an engine bug fix
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
3. Does the platform already do it? musl's libc and the kernel's own interfaces (netlink,
   `/proc/sys`) on the engine; the Kotlin and Java stdlib and the Android framework on the
   app side. Mind `minSdk = 26`.
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
  `virtualap`, and `Backend.startAp` is the only way to spell its path.
- `Hotspot` is the only caller of `APManager.start` and `APManager.stop`. It owns the
  starting/stopping phase, the command log, the running notification and the Quick Settings
  tile sync, so the screen and the tile always agree. `APConfig.isValid()` is the only
  start validation; the screen's button and the tile both ask it.
- `src/` keeps one concern per `.c`, its header under `src/include/<layer>/`, everything
  else `static`. A file includes only its own layer and the ones below it: `core`, `cli`,
  `net` and `radio`, `services`, `mode`, `monitor`, `cmd`, `main`. A new concern is a new
  module, never a few hundred lines added to one that has another.
- Logging is `vap_log`, `vap_warn`, `vap_error` (`core/log`). They print to stdout, which
  the app streams into its log sheet with the `[WARN]` and `[ERROR]` tags it reads, and
  append a timestamped copy to `logs/ap.log`. The app never reads that file. A bare
  `printf` is invisible in the log, and anything on stderr is lost.
- Programs are run by `core/proc` (`run_quiet`, `run_capture`, `run_shown`, `run_logged`,
  `run_lines`), never `popen` or `system`: Android has no `/bin/sh`, and each child gets
  `/dev/null` for stdin and a clean signal mask. Pid files go through its `pidfile_*`
  functions, which signal only a process running the expected program.
- The settings live in one table, `vap_keys` in `cli/options.c`: flag letter, `ap.conf`
  key, default, usage text. A new setting is a field in `struct vap_opts` and a row there;
  parsing, `ap.conf` and the usage text follow. Every value goes through `opt_set`, which
  refuses line breaks. `ap.conf` is written only by `conf_save`.
- The kernel is spoken to through `net/` (rtnetlink: `link`, `addr`, `route`, `rule`,
  `neigh`, `netd`; the `ip_tables` engine: `ipt`, `ipt_rules`) and `radio/` (`nl80211`,
  `wpa_ctrl`). No module spawns `ip`, `iw` or `hostapd_cli`; `iptables` is spawned only for
  the TTL fix (`net/ttl`).
- `upstream_resolve` (`net/netd`) is the only place the upstream table is resolved.
  `radio/channel`, with `ensure_5g_ir` in `radio/regdom`, is the only place band and channel
  policy lives: following the station's channel, the DFS fallback, the NO-IR country fix.
  Do not compare a channel number anywhere else. `ipv6_decide` (`mode/ipv6`) is the only
  place the clients' IPv6 is chosen; the start and the monitor both ask it.
- `Constants` holds every path and preference key on the app side, the engine's
  (`Constants.ENGINE`) included. `VirtualAPInstaller` deploys the binaries; it is not
  duplicated elsewhere.

## Hard constraints

- The app targets `minSdk = 26`. Test on Android 8 behaviour before assuming an API exists.
- The engine is C11 (`gnu11`) on musl, single threaded: `setns` moves only the calling
  thread, and a netlink socket stays in the namespace it was opened in. It builds with no
  warnings under the Makefile's gcc set, which CI turns into errors.
- Binaries are fully static musl builds from the `externals/` submodules, both arches. Never
  a dynamically linked binary, never a prebuilt from elsewhere. A new tool is a new submodule
  plus a block in `scripts/build-in-container.sh`; `virtualap` is the one binary built from
  this repository's own `src/`, with musl-cross gcc and the root `Makefile`.
- No new dependency when an installed one covers it. The app already has libsu, Compose,
  lifecycle, and zxing.
- Samsung's Broadcom FullMAC driver is the floor for wireless assumptions, and MediaTek's
  gen4m the second; what each one lacks is in "The radio" in How-It-Works.md. A quirk fix
  that is not gated on runtime detection does not go in.

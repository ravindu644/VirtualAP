# DESIGN.md

The visual language of the VirtualAP Android app, which is the Droidspaces design language
carried over with the components that implement it. The backend is a shell script with no
UI, so none of this applies to `backend/`.

Components come first, values second: the reuse inventory in
[CONTRIBUTING.md](./CONTRIBUTING.md) lists what exists, and this file is for the case where
nothing there covers what you need. Never hardcode a hex colour, an sp size, or a radius you
picked by eye.

## What the app looks like, and why

Nothing in this app is elevated. Depth is expressed by stepping the surface colour up one
level and drawing a 1dp border; if you reach for a shadow, you have left the design.
`Surface` is the primitive: buttons are clickable Surfaces, not Material `Button`s, and
dialogs are `Dialog { Surface { } }`, not `AlertDialog`. Material components are not banned,
but they arrive with their own elevation and radius defaults and have to be argued for.

Colour is derived, never literal. `ui/theme/Theme.kt` blends a full scheme from three
palette colours, or takes a Material You scheme whole. Dynamic colour is on by default,
fallback palettes ship for older devices and for users who turn it off, and AMOLED collapses
every surface to black, so a hardcoded `Color(0xFF...)` breaks all four paths at once.

Borders and tint carry state, fills stay quiet. A running access point is not a green card;
it is a normal card with a tinted button and an accent border.

## Colour

Roles come from `MaterialTheme.colorScheme`. The scheme itself is built in `ui/theme/Theme.kt`
from the palettes in `ui/theme/Color.kt`, the only two files that name a colour literally.

| Surface | Role |
| --- | --- |
| Screen background | `background` |
| Card sitting on the background | `surfaceContainer` |
| Card or pill nested inside another surface, and interactive settings rows | `surfaceContainerHigh` |
| Dialog and bottom sheet shell | `surfaceContainer` |

| Text | Role |
| --- | --- |
| Primary | `onSurface` |
| Secondary, supporting, metadata | `onSurfaceVariant` at alpha `0.7f` |
| Tertiary, deliberately quiet | `onSurfaceVariant` at alpha `0.6f` |
| Disabled content | `onSurface` at alpha `0.38f` |
| Disabled container fill | `onSurface` at alpha `0.12f` |

Borders are always 1dp. The alpha is what changes:

| Border | Colour |
| --- | --- |
| Card | `outlineVariant` at `0.35f` |
| Dialog shell | `outlineVariant` at `0.4f` |
| Action pill wrapper and other nested surfaces | `outlineVariant` at `0.2f` |
| Divider (`HorizontalDivider`) | `outlineVariant` at `0.3f` |
| Text field, unfocused | `outlineVariant` at `0.5f` |
| Text field, focused | `primary` at `0.8f` |
| Tinted button, matching its accent | the accent role at `0.2f` |

### What the colours mean

The access point lifecycle is consistent across the app and new states should join it rather
than invent a parallel vocabulary.

| State | Accent |
| --- | --- |
| Running, healthy, start action | `primary` |
| Stopped, failed, destructive, stop action | `error` |
| Restart action | `secondary` |
| In progress, restarting, downloading | `tertiary` |
| Idle, unknown, nothing to report | `onSurfaceVariant` at `0.6f` |
| Disabled | fill `onSurface` at `0.08f`, accent `outlineVariant` |

A tinted button takes its fill from the matching container role at alpha `0.4f`, for example
`primaryContainer.copy(alpha = 0.4f)` for start and `errorContainer.copy(alpha = 0.4f)` for stop,
with a 1dp border of the accent at `0.2f`.

The console colours a `[WARN]` line `tertiary` and an `[ERROR]` line `error`, from the level
`classifyLine` reads off the tag; the backend emits plain text, no escape codes.

## Typography

The scale is Material 3's own, `Typography()` in `ui/theme/Type.kt`. Use the named styles and
pass the weight at the site: a title is `titleLarge` with `FontWeight.Bold`, a card heading
`titleMedium` SemiBold. There is no `fontSize` override anywhere in the app.

UI text is `FontFamily.Default`. Anything the machine produced, a log line, a config value, a
path, is JetBrains Mono, imported from `ui/theme/Type.kt`. Do not declare a local font
family.

| Element | Style |
| --- | --- |
| Screen title in a `TopAppBar` | `titleLarge`, Bold |
| Dialog title | `titleLarge`, Bold, left aligned |
| Card title | `titleMedium`, SemiBold |
| Section header inside a screen | `titleSmall`, Bold, `primary` |
| Button label | `labelLarge`, SemiBold |
| Body copy in a dialog | `bodyMedium` |
| Helper text, metadata, subtitles | `bodySmall` with `onSurfaceVariant` at `0.7f` |
| Status pill label | `labelSmall`, Black, `letterSpacing = 0.5.sp`, uppercase |
| Machine output | `bodySmall` with the JetBrains Mono family |

## Spacing

The scale is **4, 8, 12, 16, 20, 24, 32**. Nothing else. If a gap feels like it wants 14, it
wants 12 or 16.

| Gap | Value |
| --- | --- |
| Screen edge, lists and dashboards | 16 horizontal |
| Screen edge, wizard and setup flows | 24 horizontal |
| Between cards in a list | 16 |
| Between blocks inside a card | 12 |
| Card inner padding | 16 |
| Dialog inner padding | 24 |
| Icon to its label, chip to chip | 8 |
| Tight metadata rows | 4 |

There is no tab bar. The main screen sits above `PrimaryActionBottomBar` inside a `Scaffold`,
so the bar's measured height arrives as the Scaffold's inner padding and the list clears it
without anyone typing a clearance number. Keep it that way: a bar that is not the Scaffold's
`bottomBar` needs a literal, and the literal drifts.

### Card headers line up

The active network card has no title: its first row is the network name on the left and the
status pill on the right, above a divider, at a minimum height of `CardHeaderHeight` and
inside `CardContentPadding`, both from `ui/component/CardMetrics.kt`. The height is 48dp
because the row holds the Wi-Fi QR icon button, and 48dp is the touch floor.

A card that does get a title-and-pill header takes the same two values, so its divider and
pill hold the same line as this one. If you change them, you are moving every card header in
the app, which is the intended way round.

## Shape

| Element | Radius |
| --- | --- |
| Card | 20 |
| Dialog shell | 24 |
| Bottom sheet, top corners only | 24 |
| Full width primary button | 20 |
| Dropdown menu | 20 |
| Action button | 16 |
| Text field | 16 |
| Action pill wrapper | 12 |
| Status pill, small badge | 8 |

## The action pill

A row of related actions sits inside a tinted wrapper:

```kotlin
Surface(
    shape = RoundedCornerShape(12.dp),                    // wrapper
    color = MaterialTheme.colorScheme.surfaceContainerHigh,
    border = BorderStroke(1.dp, MaterialTheme.colorScheme.outlineVariant.copy(alpha = 0.2f))
) {
    Row(Modifier.padding(4.dp), horizontalArrangement = Arrangement.spacedBy(4.dp)) {
        Surface(
            modifier = Modifier.weight(1f).height(48.dp),
            shape = RoundedCornerShape(16.dp),            // button, larger than the wrapper
            color = accentContainer.copy(alpha = 0.4f),
            border = BorderStroke(1.dp, accent.copy(alpha = 0.2f))
        ) { /* icon + labelLarge SemiBold */ }
    }
}
```

The buttons are rounder than the box they sit in, and that is the design: the 4dp inset plus
the larger inner radius makes the buttons read as sitting proud of the wrapper rather than
nesting flush inside it. Matching the two radii flattens the effect.

The Interface or Container switch on the main screen (`SegmentedSelector` in
`ui/screen/MainScreen.kt`) is this geometry used as a selector: the selected segment takes
the tinted fill and accent border, the other stays transparent. The Clear and Copy row above
the log console (`LogActionRow`) is not that: it is a bare row of two 38dp buttons with no
wrapper.

## Dialogs

Every dialog is `DsDialog`, which owns the layout as well as the frame: a full-width `Surface`
inside the 24dp gutters, bounded to the screen height minus them, and a 24dp padded column
whose body scrolls. The bound comes from `LocalConfiguration`, not the window, so a dialog open
through a rotation measures against the live configuration. Callers do not set a width, their
own padding, or their own scroll, and never pass `wrapContentHeight()`; what is per-dialog rides
on the `modifier`, `imePadding()` for a text field or a `heightIn(max = ...)` cap.

The title is `titleLarge` Bold, left aligned, and is the first thing in the content.

## Dialog actions

The one dialog today, About, closes from its header with `DialogCloseButton`. The first dialog
with a choice copies `DialogFooterRow` back from Droidspaces: two equal-weight 48dp buttons at
radius 16, a quiet tonal dismiss and a filled confirm, `error` for a destructive one, one-line
labels that ellipsize. It goes in a `footer` slot on `DsDialog`, measured before the scrolling
body so landscape cannot squeeze it; the slot comes back with it.

## Icons and touch targets

| Use | Size |
| --- | --- |
| Leading icon on a card or list row | 20 |
| Toolbar action, icon inside a button | 18 |
| Inline with `bodySmall` text | 16 |
| Dialog header | 24 |
| Setup hero glyph | 96, inside the 240 hero |
| Status dot | 6 |

Anything tappable is at least **48dp**. That is an accessibility floor, not a style preference,
and it is the one rule in this file that a design argument does not get to override.

## Alignment

Rows are `Alignment.CenterVertically`.

Card titles and dialog titles are left aligned, always. A card row that pairs a title with a
control uses `Arrangement.SpaceBetween`.

Centred text is reserved for full screen states: empty lists, loading, setup heroes. If content
is centred inside a card, it is probably wrong.

## Motion and loading

Spinners come from `LoadingIndicator` with a `LoadingSize`, never a raw `.size(n.dp)` on a
progress indicator. Whole screen loading is `FullScreenLoading`; `LoadingSize.Hero` (144) is
the one setup-flow hero loader. Animation timings come from `AnimationUtils`; hero state changes
use `MaterialTheme.motionScheme`, which the theme sets to `MotionScheme.expressive()`.

### The setup flow

The welcome pager, the root check and the backend installer share one geometry, which lives
in `ui/component/SetupHero.kt`. `SetupPage` is the skeleton under the Scaffold: a 40dp row
above (the page counter on the welcome pager, empty elsewhere), the frame slot, which centres
its content or scrolls when the window is too short, and a 44dp row below (the page dots).
Every screen keeps both rows so the frame and the button never move between screens.
`SetupFrame` is the frame: a 240dp hero box, then the title and the copy in a fixed text
block. `SetupHero` is the hero of a screen that does work, a 240dp
`MaterialShapes.Cookie12Sided` in `primaryContainer` or `errorContainer` holding the loader or
a 96dp glyph, and only its colour and content change between states. The welcome pager's hero
is its own `Shape` instead, a soft burst that settles into a cookie, a clover and a sun as the
pages slide over it. `SetupActionBar` is the one button. Build a new setup screen out of these
and the hero, the title and the button land on the same rows as the others.

## When the rule does not fit

Sometimes it will not, and the answer is not to quietly use a different number.

Say it in the PR, and leave a comment at the site explaining what the deviation buys, the way
the decided exceptions below do. An undocumented deviation is drift and the next contributor
will "fix" it. A documented one is a decision, and it survives.

## Decided exceptions

Looked at, deliberately left alone. Do not re-open these without a reason the original one
misses.

- **The QR plate is `Color.White`** in the Wi-Fi QR sheet, and `QrCodeGenerator.encode` draws
  black modules on white. A scanner needs the contrast the spec assumes; a themed plate under a
  dark palette reads as a broken code.
- **The status card answers a tap** (`ActiveNetworkCard` in `ui/screen/MainScreen.kt`) although
  it opens nothing. It is the card a user prods to check the app is alive, so it ripples and drops
  field focus instead of sitting dead under a finger. The cost is that TalkBack reads it as one
  actionable node.
- **The Wi-Fi QR sheet and the logs sheet** in `ui/screen/MainScreen.kt` stay Material
  `ModalBottomSheet`s rather than `Dialog { Surface { } }`. Both are drag-dismiss content that
  belongs at the bottom edge, and the logs sheet blocks gesture dismissal while a command is
  running so the sheet cannot leave a touch-eating scrim behind. Their shell is
  `surfaceContainer` with 24dp top corners, matching the shape table.
- **The setup flow's button** (`SetupActionBar` in `ui/component/SetupHero.kt`) is a full pill
  with no bar surface and no divider, not the 20dp bar button. It sits alone on the background
  under a hero rather than over scrolling content. Its disabled colours stay equal to its enabled
  colours: it is disabled only while a check or an install is in flight, and greying it out for
  that moment reads as a flicker.
- **Faint fill alphas of 0.03 and 0.06** in `LogActionRow` stay unshared. They are invisible
  in isolation, and snapping them together would mean inventing a token for something nobody
  can see.

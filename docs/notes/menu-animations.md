# Camp menu animation speed (#577)

Debug Menu → Cheats → Quick tools → **Menu animations** selects Normal (1×),
Fast (2×) or Faster (4×). Normal is the default; `menu_animation_rate` stores
the choice independently of graphics previews and saves.

## Native update boundary

The option scales only the delta passed to two visual functions:

| Guest function | Evidence in the original PPC |
| --- | --- |
| `822A4950` | Saves f1 in f29 at `822A496C`; advances UI startup delay (`+148`, limit `+152`) and position/opacity/scale interpolation through vtable+20. |
| `82328F90` | Saves f1 in f31 at `82328FA8`, calls the base tween, then advances the tile panel timer at `+196` and fade queues at `+200/+212/+224`. |

The second function needs the scaled delta throughout its tile timeline. A
thread-local guard prevents its nested base/child updates from multiplying it
again. Each original update runs once. Accelerated calls restore the incoming
f1 afterwards; Normal forwards the original call without register changes.

The native task still receives its original delta. Only visual calls inside
these task scopes opt in:

| Update | Object | Condition |
| --- | --- | --- |
| Camp `822E0E10` | `8326DAA8` | Exact camp object; shares its existing wrapper with Save Anywhere. |
| Formation `822EBAA0` | `832AEB04` | Camp main state (`8326DAA8+0`) is 7. |
| Save `822ED938` | `832688E8` | Camp main state is 16. |
| Dreams selector `822ECEE8` | `832BE15C` | Camp main state is 14 and selector state (`+4`) is 2 (opening) or 4 (closing). |

Formation and Save retain their parent states until the child has finished
closing (`822E2764..279C`, `822E2680..26D8`). Dreams leaves its interactive
state 3 and the subsequent story presentation at their original speed. The
host Settings task (`822F19B0`, object `832AD2B8`) ticks independently and keeps
its own presentation timing.

Using task scopes also covers heap list cells. For example, `8233D600..6A0`
updates 236-byte visual cells from a list's `+68/+72` array. A whitelist of
static camp objects would miss their fades.

Focus cursors keep their original tween delta: camp `83291CE4`, Formation
`832BA9F0`, Save `83269F60`, and Dreams `832BF114`. Their outer updates retarget
them to the selected row each frame (`822EB4FC..B564`, `822EBAA0`,
`822F1768..17E0`, `822ECF9C..ED004`). List scrolling, input repeat and derived
blink timers retain the unscaled outer delta.

## Entry and delta evidence

Camp opening follows `82A18080` (action r4=1, open r5=1) → `8285FF58` →
`8287F6C0`. That helper configures the four sliding panels at camp offsets
14348/14584/14820/15056 and sets main state 2. `8287A188` sets their closing
tweens and main state 3. The camp task waits for their completed UI state 8.

Engine loop `82290008` samples time through `82290888`, converts it to seconds
using `83313630`, subtracts the prior time, and stores the frame delta at
`83315ED0` (`822901B8`). Its fixed-step branch instead uses approximately 1/30
second from `82003AB0`. The delta travels in f1 through GEngine vtable+`108`
(`8229029C`) → `82290B60` → `82290D28` → `82290DD8` → `82299818` →
`822DBD68`. The last wrapper reads the UI manager at `832652F0` and dispatches
vtable+`300` to `822DBD90` (`82017E30+300 = 82018130`).

UI manager `822DBD90` saves its input f1 in f31 at `822DBDA4`; at
`822DC02C..034` it forwards that delta to the camp task, which saves it in f23
at `822E0E2C` and forwards it to visual updates. No engine/global clock is
written. Sound requests remain in the unscaled task's selection/return
branches (`82B5D230`/`82B617B8`); neither visual updater advances audio.

The base visual completion callbacks (`828ACB20`, `82899640`) clear tween or
renderer flags through `8232AC78`. They do not perform input repeat or audio
mixing. The derived updater `822DC478` retains its own original f31 for blink
timing after calling the base tween.

These findings come from generated PPC and the four decrypted disc images.
The two visual function bodies and camp task have identical machine bytes
across all four images. The fetched semantic-recovery ledger at `498e6787`
has no recovered entries for these functions; their semantics were traced
for this change.

## Validation boundary

The call-adapter unit checks Normal, 2×/4×, task/phase gates, heap cells, focus
cursors, nested panel suppression, register restoration and thread isolation.
Debug menu interaction/raster checks cover both debug languages and help
widths. The production config fixture covers fresh-process restoration,
malformed values, preview isolation and failed writes.

Static RE and these tests justify the scoped hook. No new game run or runtime
frame capture was used for this change; this is not visual or playthrough
acceptance.

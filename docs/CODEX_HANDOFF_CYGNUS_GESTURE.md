# Codex Handoff: Cygnus-S Keymap / Host-Aware AutoMouse Gesture Work

Last updated: 2026-05-31 JST  
Repository: `Ryota-Mikuni/Cygnus-S`  
Current HEAD observed in this session: `162cb49670aa6d2664d823cb24d47b42f8c6bad3`  
Latest HEAD message observed: `Enable ZMK pointing on right half for gesture processors`

This document is intended as a continuation brief for Codex. It explains the design goal, current implementation state, likely reasons for current GitHub Actions failure, and a prioritized plan to get the build green before continuing feature work.

---

## 1. Product/design goal

The goal is to redesign the Cygnus-S keymap around these principles.

### 1.1 moNa2-inspired principles

The user believes moNa2’s design is more polished and wants Cygnus-S to borrow its philosophy, not copy it literally.

Adopted principles:

- Keep the number of user-facing layers small.
- Put scrolling on the horizontal rotary encoder, not on a dedicated trackball scroll layer.
- Keep the trackball primarily for pointer movement.
- Keep AutoMouse as a mouse/gesture mode, not a text-entry layer.
- Keep BT/system functions isolated.
- Avoid accidental BT clear / bootloader operations.
- Avoid unnecessary gesture layers visible to the user.

### 1.2 Host-aware shortcut principle

The user wants the same physical operation to have the same semantic meaning across Mac and Windows.

BT profile mapping:

| BT profile | Host type |
|---|---|
| BT0 | Mac |
| BT1 | Mac |
| BT2 | Windows |
| BT3 | Windows |
| BT4 | Windows |

Modifier meaning:

| Semantic role | Mac output | Windows output |
|---|---|---|
| Primary shortcut | Command / `LGUI` | Control / `LCTRL` |
| Alt / Option | `LALT` | `LALT` |
| OS/Sub | Control / `LCTRL` | Windows / `LGUI` |

The design should preserve shortcut fingering between Mac and Windows.

### 1.3 Base screenshot / Escape rule

Base layer rightmost key is not Escape anymore.

| Host | Base rightmost key |
|---|---|
| Mac | Screenshot UI: `Shift + Command + 5` |
| Windows | Snipping UI: `Win + Shift + S` |

Escape is moved to Fn/Nav/AutoMouse.

### 1.4 Text return rule

AutoMouse is not for text entry. When returning from mouse mode to typing, the user wants an explicit action:

| Key | AutoMouse meaning |
|---|---|
| かな | Return to text / Japanese input |
| 英数 | Return to text / English input |

This is meant to avoid confusion between pointer mode and typing mode.

---

## 2. Gesture goal

The user wants MX Ergo S-like gestures, but without adding many memorized layers.

Original desired intuitive behavior:

- Hold a comfortable right-hand key.
- Move the trackball with the thumb.
- The direction of trackball movement determines the action.

Important ergonomic observation from the user:

- Holding a thumb key while moving the thumb-operated trackball is awkward.
- Therefore gesture trigger keys should be at physical positions corresponding to Base `G/D/M` on the right hand, not on the thumb cluster.
- Thumb stays on the trackball; fingers press `G/D/M`.

Intended mapping:

### D position: Back / Clipboard

| Operation | Meaning | Mac | Windows |
|---|---|---|---|
| Tap / no movement | Back | MB4 | MB4 |
| Hold + ball left | Copy | Cmd+C | Ctrl+C |
| Hold + ball right | Paste | Cmd+V | Ctrl+V |
| Hold + ball up | Undo | Cmd+Z | Ctrl+Z |
| Hold + ball down | Redo | Shift+Cmd+Z | Ctrl+Y |

### M position: Forward / Navigation

| Operation | Meaning | Mac | Windows |
|---|---|---|---|
| Tap / no movement | Forward | MB5 | MB5 |
| Hold + ball left | Previous tab | Ctrl+Shift+Tab currently | Ctrl+Shift+Tab |
| Hold + ball right | Next tab | Ctrl+Tab currently | Ctrl+Tab |
| Hold + ball up | Overview | Ctrl+Up currently | Win+Tab |
| Hold + ball down | Desktop | F11 currently | Win+D |

Mac mapping for Mission Control / Desktop is not final. It may require macOS shortcut settings, Rectangle, Raycast, BetterTouchTool, or another helper app.

### G position: Middle / Pan

| Operation | Meaning |
|---|---|
| Tap / no movement | Middle click / MB3 |
| Hold + ball movement | Pan / XY-to-scroll style movement |

---

## 3. Current implementation state

The current repository is not in the original custom `&cy_gesture` behavior architecture anymore. It was rolled back to a more conservative architecture using:

- native ZMK hold-tap behavior, custom-named as `&mg`
- internal gesture layers
- existing `src/cygnus_gesture_processor.c`
- input listener layer-specific overrides

This is important: the current implementation does **not** implement release-time one-shot direction detection. It implements a layer-specific threshold processor that fires when accumulated movement crosses a threshold while the layer is active.

### 3.1 Current keymap layer IDs

Current `config/Cygnus.keymap` defines:

```dts
#define BASE 0
#define WIN 1
#define NUM 2
#define NAV_MAC 3
#define AMOUSE 4
#define NAV_WIN 5
#define FN 6
#define SYS 7
#define WIN_AMOUSE 8
#define D_MAC 9
#define M_MAC 10
#define G_PAN 11
#define D_WIN 12
#define M_WIN 13
```

Meaning:

| Layer | Purpose |
|---|---|
| 0 `BASE` | Mac/common base |
| 1 `WIN` | Windows overlay |
| 2 `NUM` | Number/symbol |
| 3 `NAV_MAC` | Mac nav/edit |
| 4 `AMOUSE` | AutoMouse |
| 5 `NAV_WIN` | Windows nav/edit |
| 6 `FN` | Function/System access |
| 7 `SYS` | Bluetooth/system |
| 8 `WIN_AMOUSE` | Windows AutoMouse override |
| 9 `D_MAC` | Internal D gesture layer, Mac |
| 10 `M_MAC` | Internal M gesture layer, Mac |
| 11 `G_PAN` | Internal pan layer |
| 12 `D_WIN` | Internal D gesture layer, Windows |
| 13 `M_WIN` | Internal M gesture layer, Windows |

### 3.2 Current `&mg` behavior

Current `config/Cygnus.keymap` defines:

```dts
mg: mouse_gesture {
    compatible = "zmk,behavior-hold-tap";
    label = "MOUSE_GESTURE";
    #binding-cells = <2>;
    flavor = "balanced";
    tapping-term-ms = <200>;
    quick-tap-ms = <0>;
    bindings = <&mo>, <&mkp>;
};
```

This means:

```dts
&mg SOME_LAYER SOME_MOUSE_BUTTON
```

Tap sends mouse button. Hold activates `SOME_LAYER`.

Current use in AutoMouse:

```dts
&mg G_PAN MB3
&mg D_MAC MB4
&mg M_MAC MB5
```

Windows overlay layer overrides these with:

```dts
&mg G_PAN MB3
&mg D_WIN MB4
&mg M_WIN MB5
```

### 3.3 Current AutoMouse idea

AutoMouse is Layer 4 and is still the PMW3610 AutoMouse target.

AutoMouse behavior:

- Text keys are mostly `&none`.
- Mouse buttons remain on right-hand cluster.
- G/D/M physical positions are gesture triggers.
- かな/英数 are explicit text return keys.
- Escape exists in AutoMouse.

### 3.4 Current right overlay gesture processor setup

Current `config/boards/shields/Test/Cygnus_R.overlay` includes:

```dts
#include <input/processors.dtsi>
```

It defines four custom input processor instances:

```dts
cyg_d_mac
cyg_d_win
cyg_m_mac
cyg_m_win
```

Each is compatible with:

```dts
compatible = "cygnus,input-processor-gesture";
```

Example current D Mac mapping:

```dts
cyg_d_mac: cyg_d_mac {
    compatible = "cygnus,input-processor-gesture";
    #input-processor-cells = <0>;
    tick = <18>;
    wait-ms = <20>;
    tap-ms = <20>;
    // Order required by src/cygnus_gesture_processor.c: right, left, up, down.
    bindings = <&kp LG(V) &kp LG(C) &kp LG(Z) &kp LS(LG(Z))>;
};
```

Important: order is **right, left, up, down**, not left/right/up/down.

Layer-specific listener overrides:

```dts
trackball_listener {
    compatible = "zmk,input-listener";
    device = <&trackball>;

    d_mac_gesture {
        layers = <9>;
        input-processors = <&cyg_d_mac>;
    };

    m_mac_gesture {
        layers = <10>;
        input-processors = <&cyg_m_mac>;
    };

    g_pan_gesture {
        layers = <11>;
        input-processors = <&zip_xy_to_scroll_mapper>;
    };

    d_win_gesture {
        layers = <12>;
        input-processors = <&cyg_d_win>;
    };

    m_win_gesture {
        layers = <13>;
        input-processors = <&cyg_m_win>;
    };
};
```

### 3.5 Current C module

The current active module is the pre-existing `src/cygnus_gesture_processor.c`.

Root `CMakeLists.txt` currently:

```cmake
zephyr_library()

zephyr_library_sources_ifdef(CONFIG_CYGNUS_GESTURE_PROCESSOR src/cygnus_gesture_processor.c)
```

Root `Kconfig` currently:

```kconfig
config CYGNUS_GESTURE_PROCESSOR
    bool "Cygnus trackball gesture input processor"
    default y
    depends on ZMK_MOUSE
    help
      Enables the Cygnus layer-specific trackball gesture input processor.

config CYGNUS_GESTURE
    bool "Cygnus D/M/G trackball gesture support"
    default y
    depends on ZMK_MOUSE
    select CYGNUS_GESTURE_PROCESSOR
    help
      Compatibility switch used by Cygnus_L.conf and Cygnus_R.conf.
      Selects the layer-specific gesture input processor.
```

The processor:

- listens to relative X/Y events
- accumulates `x` and `y`
- on sync event, if threshold crossed, chooses dominant axis
- fires one of four configured bindings
- resets accumulated x/y
- applies `wait-ms` debounce

This is a continuous threshold-triggering model, not release-based.

---

## 4. Current GitHub Actions status

The user reports the build is still failing.

In this ChatGPT environment, latest workflow runs for current commits were not available through the connector. Older run `26354623657` is from commit `95ec9cad...` and was successful; do not use it as evidence for the current state. That old log checks out commit `95ec9cad1f27dd724c6deb13eaecc7e408d3ab94`, not current HEAD.

Codex should first inspect the current failing Actions log directly.

Recommended commands:

```bash
gh run list --repo Ryota-Mikuni/Cygnus-S --limit 20
gh run view <RUN_ID> --repo Ryota-Mikuni/Cygnus-S --log-failed
```

If log output is too large:

```bash
gh run view <RUN_ID> --repo Ryota-Mikuni/Cygnus-S --json jobs
gh run view <RUN_ID> --repo Ryota-Mikuni/Cygnus-S --job <JOB_ID> --log
```

Focus on the first actual compile/devicetree error, not on follow-on failures.

---

## 5. Known implementation history / what was attempted

This history matters because there are still remnants from older attempts.

### 5.1 Initial custom behavior attempt

A custom behavior named:

```dts
compatible = "zmk,behavior-cygnus-gesture";
```

and an input processor named:

```dts
compatible = "zmk,input-processor-cygnus-gesture";
```

were attempted.

Files from that attempt were created under `config/`:

```text
config/include/cygnus_gesture.h
config/include/dt-bindings/zmk/cygnus_gesture.h
config/src/cygnus_gesture_state.c
config/src/behavior_cygnus_gesture.c
config/src/input_processor_cygnus_gesture.c
config/dts/bindings/input_processors/zmk,input-processor-cygnus-gesture.yaml
```

The keymap no longer references `&cy_gesture`, and the right overlay no longer references `zmk,input-processor-cygnus-gesture`.

Recommendation: once the build is stable, remove unused `config/src/*` and `config/include/*` remnants from the abandoned custom behavior attempt. Do not remove them until the current failure is understood, unless the build log explicitly says they are causing issues.

### 5.2 Current architecture is the fallback architecture

The current implementation is intended to be more buildable:

- native `zmk,behavior-hold-tap`
- internal layers
- existing `cygnus,input-processor-gesture`
- no new custom key behavior

This is not as semantically perfect as the original design, but should be much easier to build.

---

## 6. Most likely current failure causes

The exact error log is required. However, based on current files, these are the most likely causes.

### 6.1 Undefined behavior labels in overlay: `&kp`

`config/boards/shields/Test/Cygnus_R.overlay` uses `&kp` inside input processor bindings:

```dts
bindings = <&kp LG(V) &kp LG(C) &kp LG(Z) &kp LS(LG(Z))>;
```

The overlay includes:

```dts
#include "Cygnus.dtsi"
#include <input/processors.dtsi>
```

It does **not** include `<behaviors.dtsi>`.

Potential error:

```text
undefined node label 'kp'
```

If that is the failure, there are two possible fixes:

Option A: add this include near the top of `Cygnus_R.overlay`:

```dts
#include <behaviors.dtsi>
```

Potential risk: behavior labels may already be defined by the keymap include path; double inclusion might be okay or might create duplicate nodes depending on include flow.

Option B, safer: move the `cyg_d_mac`, `cyg_d_win`, `cyg_m_mac`, `cyg_m_win` processor node definitions from `Cygnus_R.overlay` into `config/Cygnus.keymap`, where `<behaviors.dtsi>` is already included. Keep only the `trackball_listener` child overrides in `Cygnus_R.overlay`. But if the listener references `&cyg_d_mac`, cross-file labels should still resolve if all are in the final devicetree.

### 6.2 `CONFIG_CYGNUS_GESTURE_PROCESSOR` not enabled

Current `Kconfig` says `CYGNUS_GESTURE` selects `CYGNUS_GESTURE_PROCESSOR`, and both depend on `ZMK_MOUSE`.

Current right and left confs explicitly set:

```conf
CONFIG_ZMK_MOUSE=y
CONFIG_ZMK_POINTING=y
CONFIG_CYGNUS_GESTURE=y
```

If the log says `CONFIG_CYGNUS_GESTURE` assigned y but got n, inspect Kconfig dependency names. It may be safer to remove the dependency temporarily:

```kconfig
config CYGNUS_GESTURE_PROCESSOR
    bool "Cygnus trackball gesture input processor"
    default y

config CYGNUS_GESTURE
    bool "Cygnus D/M/G trackball gesture support"
    default y
    select CYGNUS_GESTURE_PROCESSOR
```

Do this only if Kconfig complains.

### 6.3 `ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL`

`src/cygnus_gesture_processor.c` now includes:

```c
#include <zmk/events/position_state_changed.h>
```

This was added because the code uses:

```c
.source = ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL
```

If log still complains here, verify ZMK version and include path.

### 6.4 Binding schema problem

`dts/bindings/input_processors/cygnus,input-processor-gesture.yaml` currently manually defines:

```yaml
"#input-processor-cells":
  type: int
  required: true
  const: 0
track-remainders:
  type: boolean
```

ZMK’s built-in input processor bindings often include `ip_zero_param.yaml`, which in turn includes common properties. If schema errors occur, rewrite the binding as:

```yaml
description: Cygnus trackball gesture input processor

compatible: "cygnus,input-processor-gesture"

include: ip_zero_param.yaml

properties:
  tick:
    type: int
    default: 18
  wait-ms:
    type: int
    default: 20
  tap-ms:
    type: int
    default: 20
  bindings:
    type: phandle-array
    required: true
```

This may be more consistent with ZMK v0.3.

### 6.5 `&mg` hold-tap syntax

Current custom `mg` is:

```dts
mg: mouse_gesture {
    compatible = "zmk,behavior-hold-tap";
    label = "MOUSE_GESTURE";
    #binding-cells = <2>;
    flavor = "balanced";
    tapping-term-ms = <200>;
    quick-tap-ms = <0>;
    bindings = <&mo>, <&mkp>;
};
```

Use in keymap:

```dts
&mg G_PAN MB3
&mg D_MAC MB4
&mg M_MAC MB5
```

If the log complains about hold-tap parameters, compare this against ZMK hold-tap examples. The intent is:

- hold: `&mo <layer>`
- tap: `&mkp <mouse_button>`

This should be valid in principle.

### 6.6 Too many layers / ZMK Studio limits

Current keymap now has layers up to 13. If the log complains about max layers or Studio, the internal gesture layers may be too many.

Possible response:

- Disable `CONFIG_ZMK_STUDIO` temporarily.
- Or reduce internal layers:
  - use D/M Mac only first
  - defer Windows gesture layers
  - or remove internal layer display names

Current right conf has:

```conf
CONFIG_ZMK_STUDIO=y
CONFIG_ZMK_STUDIO_LOCKING=n
```

If Studio layer limit causes failure, this is a prime suspect.

### 6.7 `settings_reset` build interaction

The build matrix includes `settings_reset`.

If `settings_reset` build fails due keymap/custom layers, isolate:

- Does settings_reset parse `config/Cygnus.keymap`?
- Does it include this module’s root `Kconfig/CMakeLists`?
- Does it fail because `CONFIG_ZMK_MOUSE` is off?

If settings_reset is the only failure, do not overfit the Cygnus_R implementation. Gate module sources or Kconfig more carefully.

---

## 7. Recommended immediate Codex workflow

### Step 1: Get the actual failing log

Run:

```bash
gh run list --repo Ryota-Mikuni/Cygnus-S --limit 20
gh run view <latest_failed_run_id> --repo Ryota-Mikuni/Cygnus-S --log-failed
```

Identify which matrix job failed:

- `Cygnus_R rgbled_adapter, studio-rpc-usb-uart`
- `Cygnus_L rgbled_adapter`
- `settings_reset`
- `Fetch Build Matrix`

Do not modify anything until the exact failing job and first error are known.

### Step 2: Classify the first error

Use this table.

| Error contains | Likely fix |
|---|---|
| `undefined node label 'kp'` | Add `<behaviors.dtsi>` to overlay or move processor nodes to keymap |
| `compatible ... has no binding` | Check `dts_root`, binding path, compatible spelling |
| `CONFIG_CYGNUS_GESTURE ... got n` | Fix Kconfig dependencies |
| `ZMK_POSITION_STATE_CHANGE_SOURCE_LOCAL` | Header/include issue |
| `DT_INST_PROP_LEN` or `ZMK_KEYMAP_EXTRACT_BINDING` | C macro extraction issue |
| `ZMK Studio` / layer limit | Disable Studio or reduce internal layers |
| `settings_reset` only | Gate C sources/Kconfig or avoid keymap parse in settings_reset |

### Step 3: Prefer the smallest build-green fix

The current priority is build stability. Do not implement more gesture features until Actions is green.

### Step 4: After build green, test semantics

Check on device:

- Encoder scroll works on Base/Num/AutoMouse.
- Trackball movement enters AutoMouse.
- かな returns to text / Japanese input.
- 英数 returns to text / English input.
- D tap sends Back.
- M tap sends Forward.
- G tap sends Middle.
- D hold + ball right sends Paste.
- D hold + ball left sends Copy.
- D hold + ball up sends Undo.
- D hold + ball down sends Redo.
- M hold + ball right sends Next Tab.
- M hold + ball left sends Previous Tab.
- G hold + ball movement pans/scrolls.

---

## 8. Important semantic caveat

The current processor is not release-based.

Original ideal:

```text
Press D
Move ball left
Release D
=> Copy once
```

Current implementation:

```text
Hold D
D_MAC layer activates
Move ball left
Processor fires once when threshold is crossed
If movement continues after wait-ms, it may fire again
Release D
Layer exits
```

Because current `wait-ms = <20>`, repeated firing is likely if the user continues moving the ball.

Recommended after build green:

Set `wait-ms` higher, for example:

```dts
wait-ms = <500>;
```

or implement true one-shot behavior in `src/cygnus_gesture_processor.c`.

True one-shot implementation idea:

- Add `bool triggered;`
- Once a direction fires, ignore further movement until layer deactivates.
- Detect layer deactivation is not trivial from the current input processor alone.
- Alternative: rely on high `wait-ms` first.

---

## 9. Current files to inspect

### Primary

```text
config/Cygnus.keymap
config/boards/shields/Test/Cygnus_R.overlay
src/cygnus_gesture_processor.c
dts/bindings/input_processors/cygnus,input-processor-gesture.yaml
CMakeLists.txt
Kconfig
config/boards/shields/Test/Cygnus_R.conf
config/boards/shields/Test/Cygnus_L.conf
build.yaml
```

### Legacy / cleanup candidates

These are remnants from the abandoned custom `&cy_gesture` behavior attempt:

```text
config/include/cygnus_gesture.h
config/include/dt-bindings/zmk/cygnus_gesture.h
config/src/cygnus_gesture_state.c
config/src/behavior_cygnus_gesture.c
config/src/input_processor_cygnus_gesture.c
config/dts/bindings/input_processors/zmk,input-processor-cygnus-gesture.yaml
```

Do not assume they are active. Current root `CMakeLists.txt` does not build them. After build is stable, delete them to reduce confusion.

---

## 10. Suggested cleanup after build green

1. Delete abandoned custom behavior files under `config/src`, `config/include`, and `config/dts/bindings/input_processors/zmk,input-processor-cygnus-gesture.yaml`.
2. Rename visualizer from v6/v7 wording to v8 if keeping the current layer-specific implementation.
3. Update docs visualizer text:
   - It should no longer say “custom behavior + input processor”.
   - It should say “hold-tap activates internal gesture layers; layer-specific input processors fire direction shortcuts.”
4. Tune `tick` and `wait-ms`.
5. Decide whether Mac Mission Control / Desktop mappings should be:
   - native macOS shortcuts,
   - app-specific shortcuts,
   - Rectangle/Raycast/BetterTouchTool shortcuts.

---

## 11. Emergency rollback plan

If Codex needs to get Actions green quickly:

1. In `config/Cygnus.keymap`, replace:

```dts
&mg G_PAN MB3  &mg D_MAC MB4  &mg M_MAC MB5
```

with:

```dts
&mkp MB3  &mkp MB4  &mkp MB5
```

and similarly in `WIN_AMOUSE`.

2. Remove or ignore internal gesture layers 9-13.
3. In `config/boards/shields/Test/Cygnus_R.overlay`, remove gesture processors and keep only:

```dts
trackball_listener {
    compatible = "zmk,input-listener";
    device = <&trackball>;
};
```

4. Set `CONFIG_CYGNUS_GESTURE=n` or remove the custom processor Kconfig/CMake usage.

This reverts to stable AutoMouse buttons without ball gestures.

---

## 12. What not to change unless explicitly requested

Do not change:

- `CONFIG_PMW3610_AUTOMOUSE_TIMEOUT_MS=10000`
- BT0/BT1 Mac, BT2-BT4 Windows policy
- Base rightmost key as screenshot
- Escape moved off Base
- Kana/英数 explicit text return principle
- Encoder-as-scroll principle

These were deliberate user decisions.

---

## 13. Suggested first Codex prompt

Use this prompt in Codex:

```text
We are working in Ryota-Mikuni/Cygnus-S. Read docs/CODEX_HANDOFF_CYGNUS_GESTURE.md first. The current branch is failing GitHub Actions. First, inspect the latest failed GitHub Actions log using gh. Do not guess. Identify the first failing job and the first compiler/devicetree/Kconfig error. Then make the smallest code change to get the build green. Preserve the design decisions: encoder scroll, AutoMouse L4, BT0/BT1 Mac, BT2-BT4 Windows, Base screenshot key, Escape off Base, Kana/英数 text return, D/M/G gesture triggers. After each change, summarize the exact error fixed and the file touched.
```

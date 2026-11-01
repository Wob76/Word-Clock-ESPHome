# Word Clock for ESPHome

ESPHome firmware for a 12×11 WS2812 word clock (ESP32-S3 SuperMini), controlled entirely from Home Assistant.
It is an ESPHome port of [johniak/word-clock](https://github.com/johniak/word-clock) (ESP32 build), merged with the animations,
weather, presence, groups, slideshow and time-of-day brightness of the
[Crossbow78/word_clock](https://github.com/Crossbow78/word_clock) Pi controller.

**You only need one small YAML file.** The C++ (clock engine, animations) is fetched from this repo at build time.

## Install

### Option A – minimal file (recommended)

1. Copy [`wordclock-device.yaml`](wordclock-device.yaml) into your ESPHome config folder (e.g. as `wordclock.yaml`).
2. Add the entries from [`secrets.example.yaml`](secrets.example.yaml) to your `secrets.yaml`.
3. Edit the `substitutions:` at the top (name, timezone, your weather entity, the four presence entities).
4. First flash over USB (`esphome run wordclock.yaml`, or *Install* in the ESPHome dashboard). OTA after that.
   Home Assistant discovers the device automatically.

The whole config is pulled with ESPHome `packages`; anything you put in your file (substitutions, or any extra YAML) overrides the repo's defaults.
The package uses substitutions for credentials because ESPHome remote packages cannot resolve `!secret` themselves; the local file maps those values to your `secrets.yaml`.
ESPHome fetches `wordclock.yaml` and `components/wordclock` from the repository; it does not fetch or unpack `Word-Clock-ESPHome.zip`.

### Option B – one full file

Copy [`wordclock.yaml`](wordclock.yaml) instead and edit it directly. It still fetches the C++ from this repo through `external_components`.
Because it is also the remotely fetched package, its credentials are substitutions rather than `!secret` lookups; when using it as a standalone
config, add `api_key`, `ota_password`, `wifi_ssid`, `wifi_password` and `ap_password` under `substitutions:` and map them to `api_encryption_key`,
`ota_password`, `wifi_ssid`, `wifi_password` and `fallback_ap_password` from your `secrets.yaml` with `!secret`.

Both configs select `variant: esp32s3` and ESP-IDF, which is the recommended ESPHome configuration for the ESP32-S3 SuperMini. The external LED data
pin remains GPIO13; change `led_pin` in your local substitutions if your wiring uses another pin.

### Updating and pinning

ESPHome re-checks GitHub once a day (`refresh: 1d`); run an install to pick up new changes. To freeze a version, replace `ref: main` with a
tag or commit. In Option A change it in your file; in Option B change it in the `external_components:` block.

## Using it

The **Display** light has an effect list: *Word Clock*, *Weather*, *Presence*, and the animations. Everything below shows up as entities on the *Word Clock* device.

| Entity | Notes |
|---|---|
| **Display** (light) | On/off, brightness, colour, effect list. Brightness multiplies with Auto Brightness |
| **Time Colour Mode** | Random per word (ESP32 original) / Shuffled palette (Pi, no repeated colours) / Light colour / Rainbow |
| **Hourly Display**, **Hourly Display Time**, **Play Hourly Display** | What is shown on the hour (Off / Random from pool / any animation / Weather / Presence) and for how long |
| **Transition Animation**, **Transition Animation Time**, **Play Transition Animation** | Optional animation between time updates: Off (default) / Random / Sequential, and its length |
| **Transition: \<animation\>** (13 switches, config category) | Which animations are in the pool |
| **Presence: Dot 1–4** (switches, config category) | Enable/disable individual presence dots; disabled dots are fully dark |
| **Effect Speed** | 5–100; 50 = exactly the Pi's pace |
| **Group**, **Next Mode**, **Next Group** | Choose Info or Animations, or navigate to the next mode/group |
| **Slideshow** + **Slideshow Interval** | Advances within the current group (default 30 s) |
| **Auto Brightness** | Pi schedule: night 23–05 → 15 %, evening 19–08 → 40 %, day 100 % |
| **Night/Evening Brightness / Starts / Ends** | Every number of that schedule is adjustable (config category) |
| **Transition Fade** | Fade in/out around animations and mode changes |
| Diagnostics | WiFi signal, uptime, IP, restart |

Settings are restored after a reboot.

### Modes

* **Info**: Word Clock, Weather, Presence
* **Animations**: Matrix Rain, Ripple, Fire, Plasma, Sand, Lava Lamp, Bouncing Balls, Game of Life, Flow Field, Spiral, Flags, Lightning, Heart
* **Games** (Tetris, Snake, Othello, Chess, Connect Four, Lights Out): *not ported yet*

### Animations on the clock

Both are driven by the Word Clock effect, so they only play while it is the active mode.

* **Transition animation (opt-in, off by default).** Set **Transition Animation** to *Random* or *Sequential*. From then on, every time the words
  change (every 5 minutes) an animation plays first, then the new time fades in. Choose which animations may play with the
  **Transition: …** switches, and how long with **Transition Animation Time**. Random never repeats the same animation twice in a row;
  Sequential goes through the enabled ones in order. A new time appears about *Transition Animation Time* seconds after the 5-minute mark.
* **Hourly display.** At :00, **Hourly Display** decides what is shown for **Hourly Display Time** seconds, *instead of* the transition animation:
  Off, Random from pool (the enabled transition switches), or one fixed choice: any animation (**Heart** is the default, as in the original
  firmware), **Weather** or **Presence**.
* The **Play …** buttons preview both. Selecting the Word Clock never triggers a transition by itself, and an hour is only announced once.

### Weather and Presence (fed by Home Assistant)

* **Weather** uses the `weather_entity` (plus `sun.sun` for day/night icons) instead of the Open-Meteo call of the Pi code. Temperature must be in °C.
* **Presence** replaces the Pi's ping thread: a member is present when its entity (`person.*`, `device_tracker.*`, `binary_sensor.*`) is `home`/`on`.
  The four dot positions and colours are in `components/wordclock/wc_effects.h` (`PRESENCE[]`, the Pi's `FAMILY_MEMBERS`). Toggle **Presence: Dot 1–4**
  in Home Assistant to disable unused dots; all four are enabled by default and the setting is restored after reboot.

## Customising the code

* **Different heart / hourly GIF:** `python3 tools/gif_to_header.py my.gif -o components/wordclock/heart_anim.h --fixed-delay 100`
  and commit the result (`--fixed-delay 100` is what the Pi does: every frame 100 ms). Resized to 12×11 if needed.
* **Presence dots or new animations:** edit `components/wordclock/` and push. For quick local testing, point `external_components` at a local
  folder instead: `source: {type: local, path: components}`.
* A new animation = write `fx_xxx()` in `wc_effects.h`, add it to `G_ANIMS`/`run_item()` (and bump the counts), add an effect and a
  `Transition:` switch in `wordclock.yaml`. The effect names in the YAML must match the names in `G_ANIMS`.

## Repo layout

```
wordclock.yaml                    full device config (light, effects, HA entities)
wordclock-device.yaml             minimal per-device file using packages
secrets.example.yaml
components/wordclock/             header-only ESPHome component (fetched at build time)
  __init__.py  wordclock.h  wc_effects.h  heart_anim.h
tools/gif_to_header.py, heart.gif
```

## Differences from the Pi / original ESP32

* `gamma_correct: 1.0` is set on purpose: the Pi writes raw RGB (Fire applies its own gamma), so ESPHome's default 2.8 would double-darken.
* The hourly display happens when the hour changes (as in the ESP32 firmware), not at boot as on the Pi. The heart is now one animation among the others.
* Perlin noise (Fire, Flow Field) is an equivalent improved-Perlin implementation instead of the `noise` package: same amplitude and octave maths, not bit-identical.
* The Pi's web UI/API is replaced by Home Assistant. The physical button is optional (commented block at the end of `wordclock.yaml`: single = next mode, double = slideshow, long = next group).

## Not ported yet: the games

They are not interactive: all six are AI-vs-AI demos. Tetris, Snake and Lights Out are small. Othello, Chess and Connect Four think for 1.5–2 s per move,
which would block ESPHome's main loop, so they need to run on the ESP32's second core.

## Verification

Tested on a PC against the original Python, **not yet on hardware or with the real ESPHome compiler** (please open an issue with the output if `esphome compile` complains):

* Exact match with the Pi code: LED mapping (all 132 pixels), word selection (all 1440 minutes), all 14 flags, the weather temperature overlay, the brightness schedule.
* Within 1/255 of the Pi code frame by frame: Plasma, Spiral, Fire (same Perlin algorithm on both sides). The random animations were compared side by side.
* Transition/hourly scheduling, pools, fades and buttons have their own tests, plus a 24-hour soak. Headers compile warning-free and run under AddressSanitizer/UBSan.
* The YAML parses, effect names and pool switches match the C++, and all 31 embedded lambdas compile against stubs.

## Credits and licence

Based on [johniak/word-clock](https://github.com/johniak/word-clock) (word table, LED layout, ESP32 firmware) and
[Crossbow78/word_clock](https://github.com/Crossbow78/word_clock) (animations, weather, presence, groups, `heart.gif`).
The animations are ports of that Python code. **Before you publish, check the licences of both projects and add a `LICENSE` file here that is compatible with them.**

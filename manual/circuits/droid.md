---
circuit: droid
title: General DROID controls
obsolete: false
ram_bytes: 88
manual_pages: [205, 206, 207]
category: utility
tags: [configuration, led-brightness, motor-faders, m4, calibration, firmware-upgrade, low-pass-filter, smoothing, clear, factory-reset]
see_also: [algoquencer]
impl_difficulty: moderate
controller_binding: controller-enhanced
verification: requires-human
spec_gap: true
difficulty_note: A grab-bag of global settings — the output-smoothing bits (maxslope, lpfilter) and clear/clearall/uislowdown are emulatable, while calibration, firmware-upgrade and statusdump are hardware maintenance with no VCV analog and become no-ops.
verification_note: "Requires-human: the maxslope/lpfilter output smoothing could be sampled headlessly, but the exact threshold and digital-LPF coefficient mapping are only described qualitatively so matching hardware needs reference data; ledbrightness and the M4 motor/touch parameters are visual/haptic and need human check on the affected controllers."
---

# droid — General DROID controls

This circuit gives access to some general configuration settings. It does not
make sense to create more than one instance of this.

The `droid` circuit gives you access to miscellaneous functions that affect the
system as a whole. The most commonly used functions are that for lowering the
brightness of the LEDs on the master, G8 and X7 via `ledbrightness` and that of
reducing the force feedback power of virtual notches of the motor faders of the
M4. This is done with `m4notchpower`.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `ledbrightness` (`l`) | CV | `1.0` | Let's you dim all of the 24 LEDs of the master and the G8. This is mainly for those who think they are too bright. But since this parameter can be CV-controlled, you could of course also do funny things with it. Beware: if you set this to zero, the LEDs will be completely dark. This also includes possible error messages. |
| `maxslope1 … maxslope8` (`s`) | CV | `0.25` | Sets a threshold for a voltage change between two samples until the internal logic of the DROID outputs assumes that this step is intentional and should not be smoothed out. A typical case where you do not want smoothing is the pitch output of a sequencer. The default value is `0.25`. A value of `0.0` turns off smoothing altogether since the slightest voltage change is considered an intentional jump. |
| `lpfilter1 … lpfilter8` (`lf`) | `0..1` | `0.25` | Configures a digital low pass filter on the output in order to smooth out digital noise resulting from the DROID's main loop. This loop is running somewhere between 3 and 6 kHz — depending on the number of circuits you use. Per default this filter is set to `0.25` — which means a mild filtering — thus still allowing fast and snappy envelopes and other rapidly changing signals while filtering away most of the digital artefacts. If you use an output for a slow envelope that is combined with an audio path in a way that you hear digital artifacts then increase that value. This is e.g. the case if you modulate a VCA that in turn modulates a very low pitched audio wave with very few harmonics (such as a sine or triangle wave). The maximum value of `1.0` leads to a very strong filtering — i.e. removing all fast transients. Snappy envelopes will be smoothed out heavily. Square wave LFOs will be converted into lower level almost sine waves. |
| `m4faderspeed` (`m4f`) | `0..1` | | Set the force / speed of the motor faders. Faster speeds need more electrical power and might wear off the faders faster. Too slow speeds might lead to poor operation. This value goes from 0.0 (slowest possible speed) to 1.0 (maximum speed). If you don't use this parameter, some reasonable default is used that depends on the firmware of the M4 module. |
| `m4touchgain1 … m4touchgain8` (`m4t`) | `0..1` | | Set the sensitivity of the touch buttons of the M4s. The range of this values is 0…1. Set a small value if your touch buttons are too sensitive (e.g. "touch" themselves when a fader moves). Set a large value if your buttons are not sensitive enough. When you mix M4s of different hardware revisions you might need to set different values for different M4s. So there are eight inputs for up to eight M4s. The idea behind this: The touch buttons of the M4 operate on the change of a minimal current flowing through a very large resistor. This resistor is changed by your finger touching the metal surface. The amount of change depends on the moisture of your finger, the grounding of your case and also the hardware revision of the M4. |
| `statusdump` (`sd`) | trigger | | A trigger here does the same as a double "click" on the master's button: it creates a status dump file on the SD card. This trigger allows you automated control with a precise timing. Each dump needs a couple of milli seconds to write to the SD card. So better do not spam this input with a high frequency of triggers. |
| `m4notchpower` (`m4n`) | `0..1` | | Set the force feedback power of the M4 motor fader units when they operate with virtual notches. The range is from 0 (minimum notch power) to 1 (maximum notch power). Note: 0 does not turn the notches off, there is still some minimal feedback. If you don't use this parameter, the notch force feedback operates at some default power, which is dependent on the M4 firmware version. |
| `calibrate` (`c`) | trigger | | Immediately enter the calibration procedure, that's contained in the maintainance menu (only MASTER). Skips the menu. After calibration is done, resets. |
| `startcontrollerupgrade` (`u`) | trigger | | Immediately starts the firmware upgrade procedure for controllers like M4 and E4. After one succesfull upgrade resets the master. |
| `startx7upgrade` (`x7`) | trigger | | Immediately starts the X7 firmware upgrade procedure (which is located at position 8 of the maintainance menu). After the upgrade of the X7 resets the master. |
| `clear` (`cl`) | trigger | | A trigger here sends a trigger to the `clear` input of all circuits that support this. That brings the state of those circuits to their start state. Circuits that have presets do keep those presets untouched. Just the current state is affected. That trigger is not sent to circuits whose `clear` input is patched. Note: Just that part of the state is affected that is saved to the SD card. For example the [`algoquencer`](algoquencer.md) does not reset to the first step, it just clears it's current pattern. |
| `clearall` (`ca`) | trigger | | A trigger here sends a trigger to the `clearall` input of all circuits that support this. That's like a global factory reset for all of your circuits. Everything is set to its starting state, including all presets of those circuits. That trigger is not sent to circuits whose `clearall` input is patched. Note: Just that part of the state is affected that is saved to the SD card. For example the [`algoquencer`](algoquencer.md) does not reset to the first step, it just clears it's current pattern. |
| `uislowdown` (`us`) | gate | `1` | Since blue-6 circuits that are ment to build a user interface are executed at just one eighth of the normal speed. They are just executed every eighth loop cycle. This saves considerable loop time and speeds up all the other more musical circuits. This breaks the rule that all circuits are executed in the order of their appearance in the patch, however. So if your patch really depends on that and your buttons, buttongroups and pots seem to behave weird, try switching off the UI optimization by setting this parameter to `0`. |

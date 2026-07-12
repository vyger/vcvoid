---
circuit: outputcalibrator
title: Tune the calibration of your CV outputs
obsolete: false
ram_bytes: 48
manual_pages: [358, 359]
category: utility
tags: [calibration, master18, cv-output, voltmeter, tuning, nudge, reference-voltage, db8e, display]
see_also: [calibrator]
impl_difficulty: not-feasible
controller_binding: controller-required
difficulty_note: Tunes the physical DAC calibration of a real master's CV outputs (nudge to a voltmeter, save to flash, DB8E display); in Rack the outputs are already numerically exact, so the entire calibration procedure has no analog and no purpose.
---

# outputcalibrator — Tune the calibration of your CV outputs

This circuit can be used for tuning the output precision of the CV outputs of
your master. Its main purpose is to provide a calibration procedure for the
MASTER18.

The eight CV outputs of the master need to be exactly calibrated so that if you
write e.g. 5 V in your DROID patch, the output actually outputs 5.000 V and not
something else. This is due to the slight production tolerances in electronic
parts. For the MASTER there is a semi-automatic calibration procedure available
in the maintainance menu.

The MASTER18 does not have CV inputs, however, so the calibration of the CV
outputs has to be done manually with a precise voltmeter. Since the MASTER18 does
not have a maintainance menu either (it does not have LEDs on the front panel),
this circuit has been introduced to give you access to the calibration settings.

You *can* use this circuit with the MASTER, as well. One use case would be to
adapt an output to some non-standard tracking which is not 1V/Octave. This is a
bad idea however, since this permanently destroys your correct calibration.
Better is to use the circuit [`calibrator`](calibrator.md) or apply some simple
math at the outputs, if that is sufficient.

In the example patches of your firmware packager you find an example patch that
uses an E4 controller for the calibration procedure.

## Display

On a MASTER18, if you have attached and configured a DB8E controller (see the
manual, [hardware](../hardware.md)), its display shows the current calibration
status as soon as you nudge a calibration value up or down, change the
calibration channel or reference point or do some other operation that changes
the state.

The two rows of boxes indicate, where the calibration values of the individual
outputs and reference voltages differ from the default calibration. One box is
marked with a dot – the output and voltage that you have currently selected.

The number in the bottom shows the deviation of the selected calibration point
from its default and the word "CHANGED" indicates that your current calibration
is not yet saved to the flash.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `output` (`o`) | integer | `1` | Select the output to calibrate. This is a number from 1 to 8. |
| `referencepoint` (`r`) | integer | `0` | For each output, two voltages need to be adjusted: 0 V and 5 V. Select either 0 for 0 V or 1 for 5 V. |
| `nudgeup` (`nu`) | gate | | Increase the currently selected output voltage by one minimal step, to match the reference voltage. |
| `nudgedown` (`nd`) | gate | | Decrease the currently selected output voltage by one minimal step, to match the reference voltage. |
| `save` (`sv`) | trigger | | A trigger here saves the changed calibration values to the internal flash of the master and the SD card. |
| `cancel` (`c`) | trigger | | A trigger here restores the previous calibration values from the internal flash. |
| `loaddefaults` (`ld`) | trigger | | A trigger here loads the default calibration values, which are not very precise, but a good starting point if you got totally lost. |
| `display` (`dy`) | integer | `1` | This parameter selects the number of the DB8E display controller, where any change of this circuit's state will show up. Per default, the first DB8E available is used. Set this to 2 if you have a second DB8E and want to use that. Note: any other controller is not being counted here, it's really just the number of the DB8E as counted from the master module. Set this to 0 to suppress any display output from this circuit. |
| `header` (`hr`) | text | ☞ smart | This text is displayed at the top of the DB8E display whenever this circuit shows up in the display. If you omit this, an automatic title is used. For example if the output of this circuit is fed into an internal patch cable, the name of this cable is displayed. Setting `header = ""` or `header = 0` removes the header and just displays the actual value. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `dirty` (`d`) | gate | Outputs 1 if the current calibration has been changed and needs to be saved. |
| `calibration` (`cl`) | CV | Shows the difference between the current calibration of the selected output and reference voltage and its default calibration value. |
| `uncalibrated` (`u`) | 0..1 | Shows you the percentage of uncalibrated outputs. If all eight outputs are calibrated (differ from the default calibration value) this outputs 0. |

## See also

- [`calibrator`](calibrator.md) — the preferred way to adapt output tracking
  without destroying the factory calibration.

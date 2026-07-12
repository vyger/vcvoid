---
circuit: encoquencer
title: Performance sequencer using E4 encoders
obsolete: false
ram_bytes: 1352
manual_pages: [223, 224, 225, 226, 227, 228, 229, 230, 231, 232, 233, 234, 235, 236, 237, 238, 239, 240, 241, 242]
category: sequencer
tags: [sequencer, encoder, e4, performance, motoquencer, ratchets, repeats, gate-pattern, quantize, scale, keyboard, record, lucky, accumulator, form, ledpreview, zorder]
see_also: [motoquencer, buttongroup, sinfonionlink]
impl_difficulty: hard
controller_binding: controller-required
verification: requires-human
spec_gap: false
difficulty_note: Full motoquencer feature set (sequencing, forms, lucky ops, scales, ratchets, presets) re-skinned onto E4 encoders with 32-LED ring visualization.
verification_note: "Sequencer/quantize/lucky logic is deterministic and headless-testable against computed references, but the E4 encoder-ring LED semantics and push-button editing require human visual inspection; spec is complete (defers to motoquencer)."
---

# encoquencer — Performance sequencer using E4 encoders

This circuit is an exact replica of the [`motoquencer`](motoquencer.md) circuit,
but it uses encoders of the E4 controller instead of the motorfaders of an M4
controller.

Here is a minimal example:

```droid
[e4]

[encoquencer]
    clock = I1
    cv = O1
    gate = O2
```

The manual section of the [`motoquencer`](motoquencer.md) circuit is long and
deep and we don't want to duplicate it here. Please go to
[`motoquencer`](motoquencer.md) to learn about all features and see examples.
Here are the only differences between the encoquencer and the motoquencer:

- The circuit's name is `encoquencer` and uses encoders.
- For setting and removing gates, it uses the push buttons in the encoders.
- It uses the LED rings around the encoders to visualize what's going on.
- The encoders do not give haptic feedback, of course.

Please don't get confused by the fact that many parameters have the word *fader*
in their name or description. This is because we chose to use the same names as
in the motoquencer. This allows you to use all patch examples for the
motoquencer. The only thing you have to do is to replace the circuit name
`motoquencer` by `encoquencer` everywhere in these patches. When you read
"fader" or "M4", think of "encoder" and "E4".

## LED visualization

Unlike a fader, an encoder has no visible *position*. The E4 uses a ring (or
rather square) of LEDs, instead. Read the LEDs as follows:

Every step of the sequence is represented by one encoder. The middle three LEDs
below each encoder have the same function as the touch button's LED in the M4:
They reflect the current `buttonmode`. Usually they show active gates (greenish
blue). If the button mode is set to "start / end", the three LEDs are green on
the first used step and a red LED on the last one.

The two LEDs left and right of those for the gates are unused. The remaining 25
LEDs visualise the current setting of the sequencer step according to the
current `fadermode`:

- **Pitch / CV**: In quantized mode every scale note is represented by one
  colored LED. The color reflects the chord function of that note. For example
  root notes are blue, fifths are green, thirds are red. That makes it easier to
  set the right note. In unquantized mode you just get a blue LED gauge starting
  from the bottom left.
- **Randomize CV**: A light green LED gauge shows you the amount of
  randomization.
- **Gate probabilities**: The eight possible settings are represented by eight
  positions in the ring. The LEDs are blue for the various standard random
  positions of 100%, 50%, 25% and 12%. The positions for playing a note every
  2nd or 4th turn are light green. And the special position 1 for playing a note
  when the last random was positive is magenta.
- **Repeats**: The LEDs are purple and white. Purple means an odd number (1, 3,
  5, 7, 9, 11, 13, or 15). White means an even number (2, 4, 6, 8, 10, 12, 14,
  16). This funny scheme makes it easier to spot certain numbers.
- **Gate pattern**: The four possible settings are represented by one colored
  corner of the ring, each. The order is cyan (just play the first repetition),
  magenta (play all repetitions), orange (play one long note), yellow (tie this
  note with the next one).
- **Ratchets**: The number of ratchets are symbolized by purple LEDs for odd
  numbers (1, 3, 5, 7) and white LEDs for the even numbers (2, 4, 6, 8).
- **Gate**: If the fader mode is set to 6 (gate), all LEDs are light blue for
  enabled gates.
- **Skip**: If the fader mode is set to 7 (skip), all LEDs are violet for
  skipped notes.

And the encoquencer has two inputs the [`motoquencer`](motoquencer.md) does not
have: `ledpreview` and `zorder`. When you set `ledpreview = 1`, the LED ring
always shows all possible values in dimmed colors. Try it out! And `zorder` swaps
how the steps of the sequence are distributed on the encoders from "vertical
first" to "horizontal first".

## DB8E / Display

If you have a DB8E display controller (see the manual
([hardware](../hardware.md))), the encoquencer circuit automatically uses the
display whenever you edit something in the sequencer. For example the note value
of a display is displayed when you change it.

Note: The encoder of the DB8E *cannot* be used for the encoquencer. Please have
this in mind before you order 16 DB8Es. You will be disappointed. Sorry.

The input `firstfader` (which sets the first encoder to use) does *not* take into
account any DB8Es. They are simply ignored.

## Inputs

| Jack | Type | Default | Description |
|------|------|---------|-------------|
| `zorder` (`z`) | integer | `0` | Changes the order of the encoders in the sequence. The natural order (at the default value of `zorder = 0`) assigns the sequence steps to the encoders in their order of appearance in your controllers. The step counter moves downwards the four encoders of one E4, then jumps to the first encoder of the next E4 and so on. There are four different choices. The choices 2 and 3 are for situations where you mount the master at the *right* of your controllers. `0` = sequence step moves downwards, E4 by E4; `1` = sequence step moves left to right, row by row; `2` = like 0, but start with last E4; `3` = right to left, row by row. The name `zorder` resembles the fact that if you draw the order of the encoders with a pen on a paper, the setting `zorder = 1` looks like three times the letter *Z*. |
| `nume4s` (`n4`) | integer | ☞ smart | Define the number of E4 modules the sequencer should occupy if `zorder` is 1 or 3. If you don't use this parameter, the number is set to `numfaders` / 4. If you have eight E4 and want to create a sequencer with just the first row, set `numfaders = 8` and `zorder = 1` and `nume4s = 8`. For using the first two rows, do the same with `numfaders = 16`. By choosing a specific encoder to be the first, with `firstfader`, you can move this rectangle of encoders to a different position in your E4 matrix. |
| `ledpreview` (`pv`) | gate | `0` | Set this to 1 if you want the inactive states (or possible settings) to be illuminated in the LED ring. |
| `firstfader` (`f`) | integer | `1` | First M4 fader of the sequencer (starting with 1). If you omit this, it starts at the first fader of your first M4. |
| `numfaders` (`n`) | integer | | Number of faders to use for your sequencer. The typical numbers are 4, 8, 16 and 32. 32 is the maximum (eight M4 units). If you omit this, all of your M4 faders will be used. |
| `numsteps` (`ns`) | integer | | Number of steps your sequence consists of (at maximum). The number of steps can be greater than the number of faders. In that case use `page` for paging your faders so that you can edit all of the steps. Having the number of steps less than the faders makes no sense — it's just a waste of faders. The maximum number of steps is 32. If you don't set this parameter, the number of steps will be set to the number of faders. Note: changing this setting dynamically can provoke various surprising behaviours. For example the number of pages (see parameter `page`) might be reduced. Or the end marker is forcibly moved around. If you want to change the length of the sequence via CV, better use `endstep` or `autoreset`. Another note: Setting `numsteps` will *not* restrict the number of faders. If you set `numsteps = 4` but have eight faders available, the circuit will use all these, even if faders 5, 6, 7 and 8 will be useless. You need to set `numfaders = 4` in this situation. |
| `page` (`p`) | integer | `0` | Use this parameter if you have less faders than steps. The first page is `0`, not `1`. For example if you have 4 faders but 16 steps, you can select between the four "pages" of four faders each, by setting this to 0, 1, 2 or 3. The output of a [`buttongroup`](buttongroup.md) with one button per page is a good match for this parameter. |
| `clock` (`c`) | trigger | | Patch an input clock here. If you want to use ratcheting, that clock needs to be stable and regular, because the sequencer needs to interpolate the clock and create evenly distributed new beats within two clock ticks. If you don't use ratcheting, you can use any rhythm you like here — may it be shuffled, euklidean, the output from another sequencer or whatever you like. Each clock tick will advance the sequence to the next step (or to the next repetition of the current step). |
| `taptempo` (`tt`) | trigger | | If your clock is not steady but might be stopped and restarted, you should patch a steady clock here. This avoids problems with the computation of the gate length right after starting the clock. The tap tempo computation needs a series of at least two clock pulses so the gate length of the first step is wrong after the clock has stopped for a while. |
| `reset` (`r`) | trigger | | A trigger here resets the sequencer to its start step. The next clock tick (or a tick that is roughly at the same time as the reset) will play step 1. Note: If there is a reset *without* a clock tick at the same time, the sequencer will go to "step 0", which is a special state where it waits for the clock to advance to the first step. Without that fancy logic, a reset plus clock would skip step 1 and start with step 2. |
| `run` (`ru`) | gate | `1` | If you set this input to 0, the sequencer will ignore all incoming clock ticks. It stops. The default of 1 is normal operation, where it runs. This input is a better way to temporarily stop the sequencer than to stop the clock. The reason: for computing the gate length and ratchets a steady input clock is needed. If you stop the clock, the next gate length and ratchets right after the next start will have the wrong duration since at least two clock ticks are necessary for computing its speed. Note: This input is not a replacement for `mute`, since a muted sequencer leaves the clock running and advances steps. It just mutes the gate output. |
| `composemode` (`cm`) | gate | | Enabling "compose mode" makes it easier to find the right note in a step, when creating more complex melodies. When `composemode` is set to 1, the sequencer stops clocking. Instead — every time you change the CV of a step, it immediately jumps to that step, outputs the changed CV and opens the gate for a short time, so you can listen to the changed note. |
| `mute` (`m`) | gate | | If you set this to 1, the gate output of the sequencer is muted (will always be 0). Any changes of the CV output still happen. |
| `cvbase` (`cb`) | CV | `0.0` | Here you set the voltage the sequencer will output if the CV fader is at the bottom position. The allowed range is −1 … 1 (which is the same as the range from −10 V to +10 V, if you output the CV directly to an output). |
| `cvrange` (`cr`) | CV | `0.2` | CV range of the faders. So the resulting CV lies somewhere between `cvbase` and `cvbase` + `cvrange`. The CV range cannot be negative and is capped at 1. If you need a greater range, consider multiplying the output value of the `cv` output of the sequencer. |
| `invert` (`iv`) | gate | `0` | Inverts the CV or pitch output. This is like mirroring the fader position. Now if the fader is up, the output is low and vice versa. In scale quantized mode, the melody still stays within the scale. |
| `quantize` (`q`) | integer | `2` | Switches on quantization in two levels. At 0, the faders run freely and output a continuous CV. At 1, the output is quantized to semitones, which is 1⁄12 V steps. Also the faders will get artificial notches — one for each semitone. That is, unless your range is too large. The maximum number of notches with force feedback is 25, so if your range exceeds two octaves (0.2), the notches are turned off. At 2, the output is quantized to the scale that `root` and `degree` define. Furthermore the individual scale notes can be switched on or off with the parameters `select1`, `select3` and so on. Note: the `root` input does not select the lowest note of the CV range. That is still set with `cvbase`. It is just used for selecting the scale. `0` = no quantization; `1` = quantize to semitones (1/12V steps); `2` = quantize to the scale set by root and degree. |
| `cvnotches` (`cn`) | integer | `0` | Usually the CVs of the steps are meant to be note pitches (when `quantize` is 1 or 2), or just free CVs (`quantize = 0`). There is an alternative mode, however, that allows you to assign integer values like 0, 1, 2 and so on to each step. To do this set `cvnotches` to a value of 2 or greater. This defines the number of discrete values (and hence notches) for each step and puts CVs of the sequence into *notched mode*. 1 makes no sense, of course, since in this "pitch bend mode" the faders would always return to the neutral position. In notched mode the `cv` output does not output a pitch but a notch number starting from 0. `cvbase`, `cvrange` and `quantize` are ignored. The maximum number of notches is 127, but the haptic force feedback of the motor faders is disabled starting at 26. |
| `shiftsteps` (`sh`) | integer | `0` | Shifts all your steps by that number to the left (negative numbers shift to the right). So if `shiftsteps` is 1, right after a reset, the sequencer will not play step 1, but step 2. The shifting wraps around at the end of your sequence, so if you have 24 steps and shift is 1, the sequencer will play step 1 instead of step 24. Note: Other things like `startstep`, `endstep`, `playmode`, `from` and `autoreset` take place *after* shifting. |
| `startstep` (`ss`) | integer | `1` | Sets the first step to be used. This means that after a reset or when the sequencer comes to the end of the sequence, it will begin at this step. There is also a way for setting start and end with buttons (see below at `buttonmode`). If you use the interactive mode, the `startstep` and `endstep` settings will be overridden. They are reactivated if you `clear` everything. Note: `startstep` and `endstep` take place after applying `shiftsteps`. |
| `endstep` (`es`) | integer | | Defines the last of the steps to be played. The default is to play all steps. After playing the end step, the sequencer moves on to the start step at the next clock tick. There is also a way for setting start and end with buttons (see below at `buttonmode`). If you use the interactive mode, the `startstep` and `endstep` settings will be overridden. They are reactivated if you `clear` everything or trigger `clearstartend`. If `startstep` is equal to `endstep`, the sequence just consists of one single step. Setting `startstep` larger than `endstep` is allowed and reverses the playing order. |
| `setendstep` (`ses`) | integer | | This input is similar to `endstep` but avoids the problem of `endstep` that its value is overridden as soon as you manually change the end step with the button (see `buttonmode`). This input acts as a number input and trigger at the same time. As soon as you send a different number than 0, the end step is set to that value *as if you had manually changed it* with the step buttons in button mode 1. The effect always happens when you change the input number. E.g. when it changes from 3 to 5, the end step is set to five *once*. This allows you to directly patch the output of a [`buttongroup`](buttongroup.md) to this input. The input value 0 does not change the end step. |
| `form` (`fo`) | integer | `0` | This is an advanced feature that allows you to slice your steps into two or three parts and create musical song forms like AAAB or ABAC. Each of the parts A, B or C are then played according to the playmode. The form AAAB, for example, creates a 32 step form from just 16 steps, by playing the first 8 steps three times and then the second 8 steps once. Available forms: `0` = A (forms are basically deactivated); `1` = AAAB; `2` = AABB; `3` = ABAC; `4` = AAABAAAC; `5` = AB; `6` = AAB. Notes: The splitting of the steps into parts takes place *after* accounting for `startstep` and `endstep`. Forms with A, B and C split the pattern into three parts. These parts can only be of equal size if the number of steps is dividable by 3, of course. The pattern AB is really not the same as A, e.g. when `direction` is set 1 (reverse). In that case each of the parts is played backwards, but the parts themselves move forwards on your steps. |
| `direction` (`d`) | gate | `0` | Sets the general direction in which the sequencer moves through the steps. 0 means forwards and 1 means backwards. |
| `pingpong` (`pp`) | gate | `0` | If set to 1, the sequencer will change the direction every time it reaches the start or end of the sequence. |
| `pattern` (`pt`) | integer | `0` | Selects one of a list of movement patterns. That way, the sequence steps are not played in linear order but in a more sophisticated movement. Available patterns: `0` = go step by step to the sequence (normal) `→`; `1` = two steps forward, one step backward `→→←`; `2` = double step forward, one step backward `⇒←`; `3` = double step forward, double step backward, single step forward `⇒⇐→`; `4` = double step forward, single step forward, double step backward, single step forward `⇒→⇐→`; `5` = random single step forward or backward `↔`; `6` = go forward by a small random number of steps `→×?`; `7` = random jump to any allowed (other) note `⇕`. |
| `repeatshift` (`rs`) | integer | `0` | This is a number in the range −24 … +24. If you set this to non-zero, each repetition of a step shifts the played note by that many notes within the selected scale notes. This only has effect on steps where the number of repeats is greater than 1. Also it is only applied when the `quantize = 2`. |
| `ratchetshift` (`ras`) | integer | `0` | This is a number in the range −24 … +24. If you set this to non-zero, each ratchet of a step shifts the played note by that many notes within the selected scale notes. This only has effect on steps where the number of ratchets is greater than 1. Also it is only applied when the `quantize = 2`. If you combine `ratchetshift` with `repeatshift`, both shifts are added together. And the ratchet shifting is reset for each repetition of the step. |
| `accumulatorrange` (`ac`) | integer | ☞ smart | Using this parameter enables the pitch accumulator. The value sets the maximum value the accumulator can get. The maximum is 16. If you set this to 0, the fader mode for pitch randomization still is in the special mode where the upper four positions control the impact of the accumulator. Please consult the manual of [`motoquencer`](motoquencer.md) for details on the pitch accumulator. |
| `constantlength` (`co`) | gate | `0` | This input enables a feature that (tries to) keep the actual length of the sequence constant. There are two levels. If `constantlength = 1`, every change in the *repeats* of a step is compensated by changing the repeats in the following steps. E.g. if you increase the number of repeats from 4 to 5 in step 3 (by moving the fader in the appropriate fader mode), the repeats in step 4 are reduced by 1. If they are already 1, step 5 is tried and so on, until it wraps around to step 1. The second level is `constantlength = 2`. Here also the *skip* setting of steps is honored and modified in order to keep the length constant. A skipped step essentially has the length 0 (or 0 repeats). The compensation is now done not only when the repeats are changed but also when skip is switched on or off on a step. All the compensation is only active with the range that is set with the start and end step. |
| `autoreset` (`ar`) | integer | `0` | If set to non-zero, automatically issues a reset (just like a trigger to `reset`) every N clock ticks. The single difference to a reset is that the pitch accumulator is not reset but advanced. While this sounds like a bug it is musically more useful and is what you expect intuitively (believe me). |
| `metricsaver` (`ms`) | gate | `1` | The Metric Saver ™ helps you to reliably come back to your original metric and time after playing around with all sorts of parameters that change the played number of steps in the sequence. These are: `startstep`, `endstep` (also when changed interactively), `form`, `direction`, `pingpong`, `pattern`, `autoreset` and repeats and skips of individual steps. Therefore it counts the actual number of clock cycles since the last external reset (or system start). And when *all* of these features are deactivated, it snaps back the clock to the position it *would* have been by now if you never had played around with all the funny stuff. That way, during a live performance, you can safely play around with all this polymetric and otherwise time disrupting stuff and as soon as you clean up your mess — voila: you are back on track and in sync with the rest of the "band". The metric saver is turned on by default. But you can disable it by setting the parameter to 0. |
| `fadermode` (`fm`) | integer | `0` | Switches the current meaning of the motor faders. You probably want to connect the output of a [`buttongroup`](buttongroup.md) here. Possible modes: `0` = pitch / CV; `1` = randomize CV; `2` = gate probability; `3` = repeats (up to 16); `4` = gate pattern; `5` = ratchets (up to 8); `6` = gate; `7` = skip. |
| `buttonmode` (`bm`) | integer | `0` | Switches the current meaning of the touch buttons below the faders. You probably want to connect the output of a [`buttongroup`](buttongroup.md) here. Possible modes: `0` = gates; `1` = start / end; `2` = gate pattern; `3` = skip. |
| `holdcv` (`hc`) | gate | `1` | This setting determines whether the CV output changes every time the sequencer moves to the next step or just when that step is active (a gate is being played). The latter is the default. But if you set this to 0, the CV values of steps without gates will also influence the output CV. Note: regardless of this setting, the CV will never change inbetween. Any change of the CV faders, the `cvbase` and `cvrange` and so on will only take effect when the next step is played. This also ensures that repeats or ratchets are always in the same pitch. |
| `defaultcv` (`dc`) | CV | `0.0` | Set the CV the steps should be set to on a trigger to `clear`. That value must be within the range set by `cvbase` and `cvrange`, or it will be truncated to that range. If you have set `cvnotches`, however, the value is expected to be an integer in the range 0 … `cvnotches` − 1. |
| `defaultgate` (`dfg`) | gate | `1` | Here you set to which state (on / off) the gates should be set on a trigger to `clear`. |
| `clearskips` (`cs`) | trigger | | A trigger here removes the "skip" setting from all steps. |
| `clearrepeats` (`crp`) | trigger | | A trigger here resets the number of repeats to 1 for each step. |
| `clearstartend` (`cse`) | trigger | | A trigger here clears the manual settings of the start and end step. So the sequence will be played in its full length (again). |
| `gatelength` (`gl`) | CV | `0.5` | The gate length in input clock cycles. A value of 0.5 thus means half a clock cycle. A steady input clock is needed for this to work. Please note that if the gate length is >= 1.0, two succeeding notes will get a steady gate, which essentially means legato. If you don't use a steady clock, set this parameter to 0. This will output a minimal gate length of about 10 ms (basically just a trigger). |
| `keyboardmode` (`km`) | integer | `1` | This input sets how a keyboard, that is hooked to `keyboardcv` and `keyboardgate`, should be used for directly playing notes. You can set it to 0, 1 or 2. `0` = ignore the keyboard inputs; `1` = keyboard and sequencer play together, keyboard has precedence; `2` = mute sequencer, just play keyboard. |
| `keyboardcv` (`kc`) | pitch | | The pitch input of a keyboard. This is used for playing along with the `keyboardmode` or recording with `recordmode`. |
| `keyboardgate` (`kg`) | gate | | The gate input of a keyboard. A positive gate enables play along (see `keyboardmode`) and also recording, if `recordmode` is set accordingly. |
| `recordmode` (`rm`) | integer | `0` | Use this input to record melodies played with a keyboard (namely `keyboardcv` and `keyboardgate`) into the sequencer. There are three possible settings: `0` = don't record; `1` = record, notes longer than one step will automatically tie steps via the gate pattern; `2` = record, don't tie notes. Ignore the length of the input note. |
| `recordsilence` (`rsi`) | gate | `0` | When this input is set to 1 while recording, silence will be recorded while `keyboardgate` is off. Otherwise you can just add notes to the sequence. |
| `copy` (`cy`) | trigger | | A trigger here copies the current page of the sequence to an internal clipboard. The clipboard is not part of any preset and is also not saved to the SD card. It can be used later for pasting its data to another preset or another page of a sequence. |
| `paste` (`pa`) | trigger | | A trigger here copies the steps from the clipboard to the current page. |
| `pastefaders` (`pf`) | trigger | | This is like `paste`, but just the values of the faders of the current `fadermode` are copied. |
| `pastebuttons` (`pb`) | trigger | | This is like `paste`, but just the values of the faders of the current `buttonmode` are copied. Note: the button mode "start / end" is not supported by copy and paste. |
| `stepcopy` (`sc`) | gate | | Wire this input to a push button to enable copying of individual sequencer steps. Hold the button to start copying. Then touch one of the step buttons. This step is copied into the clipboard. When you press further step buttons while you still hold the button the copied step's settings overwrite the pressed step. |
| `doublerange` (`dr`) | trigger | | A trigger here doubles the current playing range and copies the contents of the previous range to the second half of the new range. This only works if the playing range (start/stop) is not at maximum. |
| `bulkedit` (`be`) | gate | | If you set this to 1, then if you move one fader (or encoder) or touch/press one button, all other faders (or encoders) *at the right of the modified step* will move along to the same value — even in the steps that are currently on another page. The application is to wire this input to some button. While you hold the button you can bring a certain parameter to the same value for many steps super fast. Do this on step 1 to modify the whole track. |
| `linktonext` (`ln`) | gate | `0` | This setting allows you to create motoquencer tracks that have more than one CV or gate output for each step. If you set this to 1, the next motoquencer circuit in your patch will be synchronized to this one. This means that it always plays the same step number — including all fancy operating like `shiftsteps`, `startstep`, `endstep`, `form`, `pattern` and `autoreset`. All those inputs and also `clock` and `reset` are *ignored* by the next motoquencer. The same holds for the "repeats" and "skip" setting of the steps. `fadermode` and `buttonmode` are extended to the next motoquencers by adding 10 for each motoquencer to follow. So `fadermode = 10` will show the CV of next motoquencer in the faders. `fadermode = 11` the CV randomization of the next motoquencer. `fadermode = 20` show the CV of the third linked motoquencer and so on. Don't set `fadermode` or `buttonmode` on the linked motoquencers. They will be ignored there. Note: The `linktonext` setting cannot be dynamically changed. It needs to be fixed 0 or 1. You cannot use any button or internal cable or other methods to change it while the patch is running. |
| `luckychance` (`lc`) | 0..1 | `1.0` | Sets the chance for a step to be affected by the next "lucky" operation (see triggers below). |
| `luckyscope` (`ls`) | integer | `0` | Determines which part of the sequence is affected by the "lucky" operations. Depending on this setting the following steps are affected: `0` = all steps between the current start and end step; `1` = all steps; `2` = all steps between start and end on the current page; `3` = all steps on the current page. |
| `luckyamount` (`la`) | 0..1 | `1.0` | Sets the amount of change that a "lucky" operation does to a step. The meaning depends on the operation. See the parameters below. |
| `luckycvbase` (`lv`) | 0..1 | `0.0` | This parameter affects only the operation `luckycvs` and `luckyfaders` when the fader mode is set to 0. It adds a fixed amount of CV to the random result, so that lies in the range `luckycvbase` … (`luckycvbase` + `luckyamount`). |
| `luckyfaders` (`lf`) | trigger | | Moves the currently selected faders (according to `fadermode`) to new random positions. `luckyamount` sets the maximum value of the fader, where 1 allows the maximum. |
| `luckybuttons` (`lb`) | trigger | | Randomly toggles the currently selected buttons (according to `buttonmode`). `luckyamount` only has an effect when the gate patterns are selected, since here, four different values are possible. `luckyamount` restricts them if it is lower than 1. |
| `luckycvs` (`lcv`) | trigger | | Replaces the affected steps' CVs with new random CVs. The lowest possible CV is `cvbase`. If `luckyamount` is 1, the highest possible CV is `cvbase` + `cvrange`, otherwise it is `cvbase` + `luckyamount` × `cvrange`. |
| `luckycvdrift` (`ld`) | trigger | | Modifies the affected steps' CV randomly up or down. They will stay in the CV range set by `cvbase` and `cvrange`. `luckyamount` controls the amount of change. |
| `luckyspread` (`lr`) | trigger | | First computes the average CV of all steps. Then changes the CV values of the affected steps such that their distance to the average increases or decreases. If `luckyamount` is greater than 0.5, the distance is increased. Otherwise it is decreased. |
| `luckyinvert` (`li`) | trigger | | Inverts the CVs of the affected steps within the allowed CV range. `luckyamount` has no influence. |
| `luckyrandomizecv` (`lrc`) | trigger | | Sets the "randomize CV" values of the affected steps to random values (yes, this is double randomization). The `luckyamount` sets the maximum randomization value that will be set. |
| `luckygates` (`lg`) | trigger | | Sets the gates of the affected steps randomly to on or off. The chance for on is determined by `luckyamount`. So with `luckyamount = 0` you clear all gates and with `luckyamount = 1` you set all gates. |
| `luckyskips` (`lk`) | trigger | | Sets the "skip this step" setting of the affected steps randomly to skip or normal. The chance for skip is determined by `luckyamount`. |
| `luckyties` (`lt`) | trigger | | Sets the "tie this step to the next" setting of the affected steps randomly to tie or normal. This is the same as setting the gate pattern to the upper most position. The chance for tie is determined by `luckyamount`. |
| `luckygatepattern` (`lgp`) | trigger | | Randomizes the gate pattern of the selected steps (there are four different values: once, all, hold and tie). Use `luckyamount` to reduce that set. |
| `luckygateprob` (`lga`) | trigger | | Sets the "randomize gate" values of the affected steps to random values (yes, this is double randomization). The `luckyamount` sets the minimum randomization value that will be set (yes, this is inverted). So with `luckyamount = 1` you disable randomization and make the gates play always. With `luckyamount = 0` you set the gate probability to the lowest possible value (still not 0). |
| `luckyrepeats` (`lrp`) | trigger | | Randomly sets the number of repeats of the affected steps to something between 1 and 16 (the maximum). The `luckyamount` determines the maximum repetition number, where 1 stands for a maximum of 16 repetitions. |
| `luckyratchets` (`lrt`) | trigger | | Randomly sets the number of ratchets of the affected steps to something between 1 and 8 (the maximum). The `luckyamount` determines the maximum ratchet number, where 1 stands for a maximum of 8 ratchets. |
| `luckyshuffle` (`lsh`) | trigger | | Randomly swaps all affected steps (their playing order) together with all their attributes. `luckyamount` has no influence. |
| `buttoncolor` (`bc`) | CV | `0.1` | Set a user defined color for the gate buttons. This color is only used when `buttonmode = 0`. The main purpose of this option is to allow a separate color for the gate button in a linked sequencer, that does something else than gates. |
| `cvname` (`ce`) | CV | | If you use a DB8E display controller (see the manual ([hardware](../hardware.md))), here you can set the title of the display when changing the step's CV value. Without this parameter it's just "CV" or "Number". |
| `gatename` (`gn`) | CV | | If you use a DB8E display controller (see the manual ([hardware](../hardware.md))), here you can set the title of the display when changing the step's gate. Without this parameter it's "Gate" and the main text is either "silent" or "play". When you use this parameter, you select a custom title and the text changes to "on" and "off". |
| `display` (`dy`) | integer | `1` | This parameter selects the number of the DB8E display controller, where any change of this sequencer's state will show up. Per default, the first DB8E available is used. Set this to 2 if you have a second DB8E and want to use that. Note: any other controller is not being counted here, it's really just the number of the DB8E as counted from the master module. Set this to 0 to suppress any display output from this circuit. |
| `luckyreverse` (`lrv`) | trigger | | Reverses the playing order of the affected steps. `luckyamount` has no influence. |
| `root` (`ro`) | integer | `0` | Set the root note here. 0 means C, 1 means C♯, 2 means D and so on. If you multiply the value of an input like `I1` with 120, then you can use a 1V/Oct input for selecting the root note via a sequencer, MIDI keyboard or the like. Also then you are compatible with the ROOT CV input of the Sinfonion. `0` = C; `1` = C♯; `2` = D; `3` = D♯; `4` = E; `5` = F; `6` = F♯; `7` = G; `8` = G♯; `9` = A; `10` = A♯; `11` = B; `12` = C. |
| `degree` (`dg`) | integer | `0` | Set the musical scale. This is a number from 0 to 107. The first 12 and most important scales: `0` = lyd — Lydian major scale (it has a ♯4); `1` = maj — Normal major scale (ionian); `2` = X7 — Mixolydian (dominant seven chords); `3` = sus — mixolydian with 3rd/4th swapped; `4` = alt — Altered scale; `5` = hm5 — Harmonic minor scale from the 5th; `6` = dor — Dorian minor (minor with ♯13); `7` = min — Natural minor (aeolian); `8` = hm — Harmonic minor (♭6 but ♯7); `9` = phr — Phrygian minor scale (with ♭9); `10` = dim — Diminished scale (whole/half tone); `11` = aug — Augmented scale (just whole tones). Note: Altogether there are 108 scales. Please see the manual ([scales](../scales.md)) for a complete list. |
| `select1` (`s1`) | gate | ☞ smart | Gate input for selecting the *root* note as being an allowed interval. When you want to create a playing interface for live operation you can patch the output of a toggle button (made with the circuit `[button]`) here. Note: When all `select` and `selectfill` inputs are 0, automatically all seven scale notes are selected, i.e. `select1` … `select13` will be set to one. |
| `select3` (`s3`) | gate | ☞ smart | Gate input for selecting the 3rd. |
| `select5` (`s5`) | gate | ☞ smart | Gate input for selecting the 5th. |
| `select7` (`s7`) | gate | ☞ smart | Gate input for selecting the 7th. |
| `select9` (`s9`) | gate | ☞ smart | Gate input for selecting the 9th (which is the same as the 2nd). |
| `select11` (`s11`) | gate | ☞ smart | Gate input for selecting the 11th (which is the same as the 4th). |
| `select13` (`s13`) | gate | ☞ smart | Gate input for selecting the 13th (which is the same as the 6th). |
| `selectfill1` (`sf1`) | gate | `off` | Selects the alternative 9th (i.e. the 9th that is *not* in the scale). |
| `selectfill2` (`sf2`) | gate | `off` | Selects the alternative 3rd (i.e. the 3rd that is *not* in the scale). |
| `selectfill3` (`sf3`) | gate | `off` | Selects the alternative 4th or 5th. In most cases this is the diminished 5th. |
| `selectfill4` (`sf4`) | gate | `off` | Selects the alternative 13th (i.e. the 13th that is *not* in the scale). |
| `selectfill5` (`sf5`) | gate | `off` | Selects the alternative 7th (i.e. the 7th that is *not* in the scale). |
| `harmonicshift` (`has`) | integer | `0` | This input can reduce harmonic complexity by disabling some of the scale or non-scale notes. It is an idea first found in the Sinfonion and also provided by the circuit [`sinfonionlink`](sinfonionlink.md). `harmonicshift` is staged after the `select...` inputs and further filters out (disables) notes based on their relation to the current scale. This means that first the 12 `select...` inputs select a subset of the 12 possible notes. After that `harmonicshift` can reduce this set further (it will never add notes). If `harmonicshift` is not zero, depending on its value some or more of the scale notes are disabled, even if they would be allowed by `select...`. Or in other words: the harmonic material is reduced. You also can use negative values. These create rather strange sounds by removing the *simple* chord functions instead of the complex ones first. Possible values: `0` = off — all selected notes are allowed; `1` = disable all fill notes (non-scale notes); `2` = disable fills and 11th; `3` = disable fills, 11th and 13th; `4` = disable fills, 11th, 13th and 9th; `5` = disable fills, 11th, 13th, 9th and 7th; `6` = disable fills, 11th, 13th, 9th, 7th and 3rd; `7` = disable fills, 11th, 13th, 9th, 7th, 3rd and 5th; `−1` = disable the root note; `−2` = disable the root note and the 5th; `−3` = disable root, 3rd, 5th; `−4` = disable root, 3rd, 5th, 7th; `−5` = disable root, 3rd, 5th, 7th, 9th; `−6` = disable root, 3rd, 5th, 7th, 9th and 13th; `−7` = disable all scale notes (fill notes untouched). |
| `noteshift` (`nos`) | integer | `0` | Shifts the resulting output note(s) by this number of *scale* notes up or down (if negative). So the output note still is part of the scale but may be a note that is none of the selected ones. The maximum shift range is limited to −24 … +24. |
| `selectnoteshift` (`sns`) | integer | `0` | Shifts the output note by this number of *selected* scale notes up or down (if negative). If you use `noteshift` at the same time, first `selectnoteshift` is applied, then `noteshift`. The maximum shift range is limited to −24 … +24. |
| `tuningmode` (`tm`) | gate | `off` | While this is 1, the circuit will output the value set by `tuningpitch` instead of the actual pitch. This is meant to be a help for tuning your VCOs. |
| `tuningpitch` (`tp`) | pitch | `0V` | This pitch CV will be output while the tuning mode is active. |
| `transpose` (`tr`) | pitch | `0V` | This value is being added to the output pitch when not in tuning mode. It can be used for musical transposition or adding a vibrato. |
| `select` (`s`) | integer | ☞ smart | The select input allows you to overlay buttons and LEDs with multiple functions. If you use this input, the circuit will process the buttons and LEDs just as if `select` has a positive gate signal (usually you will select this to 1). Otherwise it won't touch them so they can be used by another circuit. Note: even if the circuit is currently not selected, it will nevertheless work and process all its other inputs and its outputs (those that do not deal with buttons or LEDs) in a normal way. |
| `selectat` (`sa`) | integer | ☞ smart | This input makes the `select` input more flexible. Here you specify at which value `select` should select this circuit. E.g. if `selectat` is 0, the circuit will be active if `select` is exactly 0 instead of a positive gate signal. In some cases this is more convenient. |
| `preset` (`pr`) | integer | ☞ smart | This is the preset number to save or to load. Note: the first preset has the number 0, not 1! For the whole story on presets please refer to the manual ([basics](../basics.md)). This circuit has 4 presets, so this number ranges from 0 to 3. |
| `loadpreset` (`lp`) | trigger | | A trigger here loads a preset. As a speciality you can use the trigger for selecting a preset at the same time. |
| `savepreset` (`sp`) | trigger | | A trigger here saves a preset. |
| `clear` (`cl`) | trigger | | A trigger here loads the default start state into the circuit. The presets are not affected, unless you use direct preset switching with the `preset` input and without triggers. In that case the current preset is also cleared. |
| `clearall` (`ca`) | trigger | | A trigger here loads the default start state into the circuit and into all of its presets. |
| `dontsave` (`dos`) | gate | `0` | If you set this to 1, the state of the circuit will not be saved to the SD card and not loaded from the SD card when the Droid starts. |

## Outputs

| Jack | Type | Description |
|------|------|-------------|
| `cv` () | CV | The CV output (or pitch output, if you use `quantize`). |
| `gate` (`g`) | gate | The gate output. |
| `startofsequence` (`ssq`) | trigger | Outputs a trigger whenever the sequencer starts playing from the beginning. This can be used for synchronizing with other sequencers. An external `reset` will also cause this output to trigger. |
| `startofpart` (`spa`) | trigger | Outputs a trigger whenever a form part starts again. This is only interesting when you use `form`. |
| `startstepout` (`sso`) | integer | Outputs the current start step. This is useful in case it has been interactively set with the buttons and you need that information for another circuit. |
| `endstepout` (`eso`) | integer | Outputs the current end step. This is useful in case it has been interactively set with the buttons and you need that information for another circuit. |
| `currentstep` (`cst`) | integer | Outputs the number of the step that is currently being played (starting from 0). |
| `currentpage` (`cpg`) | integer | Outputs the number of the fader page that is currently played, i.e. the value you would have to feed into `page` in order to see the currently being played step. |
| `accumulator` (`acc`) | integer | The current value of the pitch accumulator (an integer number in the range 0 … 16). |

## See also

- [`motoquencer`](motoquencer.md) — the fader-based (M4) version of this circuit; its manual section documents all shared features and examples in depth.
- [`buttongroup`](buttongroup.md) — a natural match for driving `fadermode`, `buttonmode`, `page` and `setendstep`.
- [`sinfonionlink`](sinfonionlink.md) — provides the same harmonic-shift concept found in `harmonicshift`.

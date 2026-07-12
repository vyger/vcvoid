# DROID Manual — Hardware (Chapters 6–14)

Controllers, expanders (G8, X7), the MASTER18, the R2M/R2C bridge, internals, firmware upgrades, calibration/maintenance, and hardware specifications. For patching basics see [basics.md](basics.md); for the per-circuit reference see [circuits/index.md](circuits/index.md).

## 6. Controllers

### 6.1 Installing the controllers

Controllers are easy to install and use. The picture below shows the back of the P2B8 controller, but the other controllers look similar.

Each controller has two 6-pin connectors that are *mounted in boxes* (shrouded). They are labelled "LINK OUT" (left) and "LINK IN" (right). These connectors are for building a chain of controllers. Don't mix this up with the 6-pin header that is labelled "Debug", which doesn't have a box!

With your controller you got a 6-pin ribbon cable. Connect one end of it to the shrouded 6-pin controller connector of your master and the other end to the "LINK IN" of your first controller.

Take another 6-pin cable and wire the "LINK OUT" of your first controller to the "LINK IN" of your second controller. Continue until all controllers are chained together.

**Finally**: Every controller also has a three-pin header with the labels "LAST" and "PARK". When you get the module there is a small connector ("jumper") between the two pins that are labelled "LAST". This jumper is crucial for making the chain work. Here is the rule:

- On the *last* controller, the jumper must be in the position "LAST".
- On *all other* controllers, the jumper must be in the "PARK" position or removed (the park position is just for your convenience so that you don't lose the jumper).

This is how a setup with two P2B8s on a master looks like:

If you switch on your system after connecting the controllers, those with LEDs should make a short power up animation. This does *not* mean that they are wired and jumpered correctly, though. To make a real test, you need to prepare a `droid` patch, as you will see below.

Note: The M4 controller (see [page 75](hardware.md)) needs an additional power connector to your Eurorack system. The other controllers are powered by the master.

### 6.2 How to use controllers in your patch

#### Working with the Forge

Before you can use the controllers in your patch, you need to declare them in your patch. If you are working with the Forge, that's super easy. Double click on the top area with the modules, or use the menu entry *Edit / New controller…*. This brings up a collection of controllers:

Double click a controller to add it to your patch. Make sure that the controllers are in the same order as you have wired it to the master – from left to right. In case you have mounted your master on the right side and the controllers from right to left, you can switch how Forge displays your patch with *View / Show master on the right side*.

Now if you want to use one of the controls, bring the cursor in your patch to the cell that shall "receive" the value of the pot or button and click on this control in the rack view. The Forge then inserts something like *Button B2.7* into this cell. This means *Button 7 on controller number 2*.

Working with the motor faders in the M4 is a bit more complex. Please have a look into the chapter about the M4 (see [page 75](hardware.md)).

#### Working with a text editor

If you write your patch with a text editor, just write one line with the content `[p2b8]`, `[p10]`, `[b32]`, `[p4b2]`, `[s10]`, `[m4]` or `[p8s8]` for each of your controllers at the top of your patch. The order of these declarations must match the order of your controllers in the chain, beginning with the one that is directly connected to the master. Here is an example with two P2B8s followed by one P10:

```droid
[p2b8]
[p2b8]
[p10]
```

Now you can use the pots, buttons and LEDs by indicating these special registers in your patch as follows:

| Register | Meaning |
|----------|---------|
| `Px.y`   | potentiometers |
| `Ex.y`   | encoders |
| `Bx.y`   | buttons |
| `Lx.y`   | LEDs in buttons |
| `Sx.y`   | switches (S10 and P8S8) |

Replace *x* with the number of the controller and *y* with the number of the pot, button, LED or switch on that controller. Examples:

- `P1.2` is the *second* pot on the *first* controller
- `B3.8` is the *eighth* button on the *third* controller
- `L3.8` is the LED in that button

Here is a schematics of the numbering of three P2B8 controllers:

```
   P2B8            P2B8            P2B8

  ( P1.1 )       ( P2.1 )       ( P3.1 )

  ( P1.2 )       ( P2.2 )       ( P3.2 )

 [B1.1][B1.2]   [B2.1][B2.2]   [B3.1][B3.2]
 [B1.3][B1.4]   [B2.3][B2.4]   [B3.3][B3.4]
 [B1.5][B1.6]   [B2.5][B2.6]   [B3.5][B3.6]
 [B1.7][B1.8]   [B2.7][B2.8]   [B3.7][B3.8]
```

Look at the following example. Here we have three controllers attached to the master: One P2B8, then one P10 and finally one more P2B8. Then we use some of the pots of the P10 for controlling the timing of an envelope circuit:

```droid
[p2b8]
[p10]
[p2b8]

[contour]
    trigger = G1
    output  = O1
    attack  = P2.5
    release = P2.6
```

##### Details on the potentiometers

The potentiometers of the P2B8 and P10 output a number in the range 0.0 … 1.0. This corresponds to a voltage from 0.0 V to 10.0 V. Wherever there is a CV parameter in a circuit (labelled ∿ in the table of inputs) you can set a pot here. An example would be an envelope generator:

```droid
[p10]

[contour]
    gate    = G1
    output  = O1
    attack  = P1.3
    decay   = P1.4
    sustain = P1.5
    release = P1.6
```

If you do not like the range of the pot you can easily change it by attenuation and offsetting as described on [page 57](basics.md). Let's make attack just go from 0.0 to 0.3:

```droid
[p10]

[contour]
    gate    = G1
    output  = O1
    attack  = P1.3 * 0.3
    decay   = P1.4
    sustain = P1.5
    release = P1.6
```

Of course you could use the *same* pot for more than one input. The following example use one single pot for attack, decay and release – with different scaling, however!

```droid
[p10]

[contour]
    gate    = G1
    output  = O1
    attack  = P1.3 * 0.3
    decay   = P1.3 * 0.5
    sustain = P1.4
    release = P1.3 * 0.7
```

Sometimes you want to use a potentiometer in a *bipolar* way – e.g. with a range from -1.0 to 1.0. This can be achieved by multiplication with 2 and subtracting 1:

```droid
[p2b8]

[copy]
    input  = P1.1 * 2 - 1
    output = O1
```

For more complicated tasks about pots there is the circuit [`pot`](circuits/pot.md) (see [page 362](circuits/pot.md)). Here are some of its features:

- Make it easy to exactly dial in 0.5 by creating an artificial notch.
- Overlay the same pot with several independent virtual values.
- Easily create a bipolar pot with access to the left and right half of the values.
- Use the master's 16 LEDs for highlighting the current pot value.

##### Details on the buttons

The buttons like on the P2B8, B32 and so on yield a value of `1.0` while pressed *and hold* and `0.0` otherwise. While this is sufficient for using them as trigger, in most cases you want the button to toggle its state between on and off each time you press it.

Here the circuit [`button`](circuits/button.md) helps (see [page 154](circuits/button.md)). It converts a push button into an on/off switch. The following example uses `B1.1` in order to switch an LFO between unipolar and bipolar:

```droid
[p2b8]

[button]
    button = B1.1
    led    = L1.1

[lfo]
    bipolar = L1.1
    sine    = O1
```

Please note, how the LED `L1.1` is set by the button, so that you have visual feedback of the current state. And since that register contains `0` or `1` depending on the button's state it can directly be used for the input `bipolar` of the LFO.

The [`button`](circuits/button.md) circuit can do much more interesting things, for example:

- Create buttons with three or four toggle states
- Combining more buttons into a group, similar to "radio buttons".
- Overlay one button with several independent functions
- Detect double clicks and long presses

See [page 154](circuits/button.md) for all the details.

### 6.3 Troubleshooting

Here are the most common reasons why controllers don't work as expected. If you have trouble with the controllers, please try the following before you reach out to our community or us. We have a production error rate of less than 1 in 1000 modules so far. So the chances are huge that you can fix your problem yourself.

**Jumpers**: If your LAST/PARK jumpers are not set correctly, the controllers will powerup anyways. The LEDs will show their boot up animation. A patch might even be able to use the LEDs in the buttons. But you won't get button presses or pot positions back to your master. That's because the jumpers organize the transportation of the output data of the whole chain back to the master.

**IN/OUT swapped**: If you mix up the two connectors, the LEDs on the module will still light up on boot time. But no communication works. It happens to me all the time, since it's easy to get confused by the fact that left/right changes when you turn the module around.

**Wrong declaration of controllers in your patch**: The controllers need to be added in their correct order to your `droid` patch. Make sure that you have setup the controllers in the FORGE in their correct order from left to right. If you mix them up, they get garbled data from the master that they cannot interprete.

**Bad cables**: This happened, even if it's super rare. If you are unsure and you have more than one cable, make a setup with just one controller on the master. Make a simple patch that uses that single controller. Now try your other 6-pin cables. If one cable works and another doesn't, an almost 100% indication that you have detected a broken cable.

**M4 blinking in rainbow colors**: If the four LEDs of an M4 controller (see [page 75](hardware.md)) flash in the alternating four colors red, green, yellow and blue, it indicates that it does not have a communication with the master. It does this in any of the upper situations. So checkout the hints. If it slowly "pumps" in one color (starting with red, then yellow, then green), it's currently charging its super capacitors and needs some time to get ready to work (1-2 minutes at most).

**Bad module**: If you really got a bad module, we apologize! You just won a 1 in 1000 price for bad luck. But you still get a chance to get things to work. There is a chance that the problem just appears if the module is *the last* in the chain, or if its *not the last* in the chain. If you have a suspicious module, try both situations. There was one defective module where just the "PARK" position was broken. The solution was to put that module as last or simply remove the jumper from PARK.

If your thing your module is defective, please contact our community on Discord. It's still a good chance that you can fix it yourself. Sometimes people are blind. You module *really* looks broken, anyway: Please contact your dealer or us directly.

And here is last hint: If you have correctly declared your controllers in your `droid` patch, the LEDs in the buttons should be lit as long as you hold the button (this is the default behaviour until you use the button in our patch). If this works, that the communication with the master is working fine.

### 6.4 The P2B8 controller

The P2B8 controller was the first available controller and is still the most popular one, since it has a balanced number of pots and buttons and is very flexible. It is good choice if you have just one or two controllers.

On the first P2B8…

- the two pots are addressed with `P1.1` and `P1.2`.
- the buttons range from `B1.1` to `B1.8`.
- the LEDs in these buttons are `L1.1` to `L1.8`.

### 6.5 The P4B2 controller

The P4B2 controller give your four nice pots and still two buttons. Otherwise it's very similar to the P2B8. The P4B2 is a good choice if you like to work with a larger number of big pots.

On the first P4B2…

- the four pots are addressed with `P1.1` through `P1.4`.
- the two buttons are `B1.1` and `B1.2`.
- the LEDs in these buttons are `L1.1` and `L1.2`.

### 6.6 The P10 controller

The P10 controller has two big pots (the same as the P2B8 controller) and eight small pots. That makes a total of 10 pots, which are all behaving in the same way. They are numbered from `1` to `10`, so if your P10 would be the first in the chain, these pots are adressed in a `droid` patch by `P1.1`, `P1.2`, `P1.3` … `P1.10`.

The P10 is handy if you need to control lots of continuous values. The small pots are not as easy to operate as the big ones but they are very space efficient.

### 6.7 The S10 controller

The S10 controller has ten switches. They have the register abbreviation `S`. The first two are rotary switches and have eight positions. They output the discrete numbers `0`, `1`, … `7`. The small switches just have three positions: `0` (down), `1` (center) and `2` (up).

In many cases the output values of the switches can be used directly for controlling something. In other stations you might want to use the [`switch`](circuits/switch.md) circuit. It's a perfect solution for having the switch select one of a list of values. Here is an example:

```droid
[switch]
    offset  = S1.1
    input1  = 0
    input2  = 2
    input3  = 3
    input4  = 5
    input5  = 6
    input6  = 10
    input7  = 11
    input8  = 100
    output1 = _FADERMODE
```

Here the switch 1 (`S1.1`) sets on offset to a [`switch`](circuits/switch.md) circuit and sends one of the values `0`, `2`, `3`, `5`, `6`, `10`, `11` and `100` into the cable `FADERMODE`.

As always: inputs can be CVs. So you can also have dynamic inputs into the switch circuit. Here we use one of the small three-way switches to select one of three waveforms of an LFO:

```droid
[lfo]
    hz     = 3
    sine   = _SINE
    saw    = _SAW
    square = _SQUARE

[switch]
    offset  = S1.3
    input1  = _SINE
    input2  = _SAW
    input3  = _SQUARE
    output1 = O1
```

The switches are programmed in a way that if you move them fast, intermediate values will not be seen by the Droid circuits. So for example if you move one of the small switches directly from down (`0`) to up (`2`), the intermediate middle position with the value `1` will not get "visible", not even for a short time.

### 6.8 The P8S8 controller

The P8S8 controller is for those who love those little sliders. The P8S8 has eight Alpha sliders with a range of 20 mm. They behave like the normal pots and are adressed with `P1.1` through `P1.8`. The bottom position is 0, at the top position their value is 1.

As a speciality the sliders contain LEDs that can be controlled and used for any purpose. Use the registers `L1.1` through `L1.8` for these. As long as you don't use the LED registers, the brightness of the LEDs reflect the current slider positions.

At the bottom the P8S8 has eight toggle switches – just the same as in the S10 (see [page 71](hardware.md)). These switches have three positions: `0` (down), `1` (center) and `2` (up). You access them with the registers `S1.1` through `S1.8`.

### 6.9 The B32 controller

You can never have too many buttons! And the B32 gives your not less than 32 of them. The B32 is a perfect companion for the M4 motor fader controller as the M4 provides lots of virtual "pots" and the B32 is handy for switching between these.

Of course the B32 is also a good play ground for trigger sequencers based on the [`algoquencer`](circuits/algoquencer.md) (see [page 127](circuits/algoquencer.md)).

The buttons are numbered `B1.1` through `1.32` (as labelled on the face plate) and the LEDs accordingly `L1.1` through `L1.32`.

They LEDs have one restriction: The just support four brighness levels: off, low, medium and full. This is a design decision for the sake of fast data transfer and low latency.

You will notice that the B32 is super fast in detecting button presses. You can slide with one finger through a column of eight buttons as fast as you like, but you will never make the B32 be too slow to detect one of the presses.

### 6.10 The E4 encoder controller

The E4 is a `droid` controller with four *encoders*. These are rotary knobs that can be turned endlessly in either direction. Instead of giving a certain position they report *movement*. They do this by sending digital signals when they are turned to the left or to the right. The encoders of the E4 have a resolution of 96 steps on one full turn.

Each of the encoders is surrounded by a square of 32 multicolor LEDs. This "ring" is used to indicate the current *logical* or *virtual* value of the encoder. Encoders are ideal for mapping multiple virtual values or presets to one knob and then switch between these. The LEDs will help you to orient yourself "where you are" and which value is currently being set.

Each encoder contains a push button, which can be used just like those on the P2B8 or on the B32.

#### Installing the E4

You install the E4 just as all the other controllers (for more about controllers see [page 63](hardware.md)): Connect the `IN` connector to your master with the 6-pin ribbon cable that came with your module. Make sure that you always use the *shrouded* pin headers (there is an additional 3×2 connector at the top, which is just for the intial programming of the hardware).

The E4 has a green jumper on its back. If the E4 is the last controller in your chain, set this jumper to *Last*. If other controllers follow, connect the next one to the `OUT` connector and remove the jumper or set it to *Park*.

The E4 also needs a connection to the power of your Eurorack modular case. It will not take the power from the master as most other controllers do. This is because the master is not able to power all the 128 LEDs on each E4 via the controller cable.

#### Using the E4 in your patches

The encoders have special registers with the letter `E`. For example `E2.3` is the third encoder on your second controller (which is presumably an E4). There is something special about these registers. Because of the nature of the encoders that do not have a current *position*, you cannot directly use them. Instead you always have to use a circuit for accessing an encoder. Currently there are three circuits for this:

- The circuit [`encoder`](circuits/encoder.md) (see [page 213](circuits/encoder.md)) gives you access to one encoder.
- The circuit [`encoderbank`](circuits/encoderbank.md) (see [page 208](circuits/encoderbank.md)) gives you access to up to eight encoders at once.
- The circuit [`encoquencer`](circuits/encoquencer.md) (see [page 223](circuits/encoquencer.md)) is a performance sequencer that can be controlled with encoders and has the same features as the [`motoquencer`](circuits/motoquencer.md) (see [page 315](circuits/motoquencer.md)).

Please read the chapter about the [`encoder`](circuits/encoder.md) circuit (see [page 213](circuits/encoder.md)) for all details on how to use the E4. There are lots of examples.

You can access the push buttons in the encoders with a `B` register, e.g. `B1.1` for the first button if the E4 is your first controller.

There is also an `L` register for each encoder (e.g. `L1.1`). This allows you to use the whole LED ring around the encoder as one big white LED – nicely overlaying with any actual animation from the encoder itself.

#### Software update for the E4

Because the E4 has a more complex firmware than for example the P2B8, it has built in a method for a software update. This makes it future proof. It is not very likely that you need to do an update any soon, but here is the procedure anyway.

Basically you do exactly the same as for the X7 (see [page 97](hardware.md)) with the following additional notes:

- The firmware file on the SD card must have the name `e4.fw`. You find the firmware file with a different name in the `droid` firmware release ZIP file in the sub directory `firmwares`.
- In the master's maintainance menu the upgrade of the E4 is on position `5` (not 8 as the X7). And its color is orange (not green).
- The E4 that you want to upgrade must be **the only module that is attached to the master!**
- The green jumper on the back of the E4 must be set to "Last".
- The firmware upgrade does not work reliably over a R2M/R2C bridge.

### 6.11 The M4 motor fader controller

#### Quick start

Here is how you get started with your M4 as fast as possible:

1. Wire the M4 to your master just as the P2B8 or any other controller. If the M4 is your last controller, set the green jumper to "Last", just as usual.
2. Connect the M4 to the bus power of your Eurorack case. It is the only `droid` controller that needs its own power connection.
3. Add the M4s to your patch with the Forge or declare them in your patch with `[m4]`.
4. Use the circuits [`motorfader`](circuits/motorfader.md) (see [page 343](circuits/motorfader.md)), [`faderbank`](circuits/faderbank.md) (see [page 247](circuits/faderbank.md)), [`fadermatrix`](circuits/fadermatrix.md) (see [page 250](circuits/fadermatrix.md)) and [`motoquencer`](circuits/motoquencer.md) (see [page 315](circuits/motoquencer.md)) for using the M4 in your patches.

**Note**: When you switch on the power, your M4 unit needs some time for charging their internal power system. That can last 60 - 90 seconds. While they are charging, here LEDs show a colored animation and go from red through yellow to green and finally off.

#### Installing the M4

You install the M4 just as all the other controllers (for more about controllers read about the P2B8 on [page 68](hardware.md)): Connect the `IN` connector to your master with the 6-pin ribbon cable that came with your module. Make sure that you always use the *shrouded* pin headers (there is an additional 3×2 connector at the bottom which is just for debugging the hardware).

If the M4 is the last controller in your chain, set the left jumper to *Last*. If other controllers follow, connect the next one to the `OUT` connector and remove the jumper or set it to `PARK`.

The M4 also needs a connection to the power of your Eurorack modular case. It will not take the power from the master (as the other controllers do). The reason is obvious: motor faders need a decent amount of power.

There are two more jumpers, labelled `+150mA` and `+100mA`. These jumpers configure the power management. Read below for details and then decide which position you want to use. If you are unsure, put both jumpers into the right position (`+0mA`). In that setting each M4 needs up to 350 mA from your 12 V rail.

After you switch on your rack you will see an LED animation on the M4. It starts with red, then gets yellow, then green and finally the LEDs go off. This animation shows you that the power management of the M4 is charging its gigantic capacitors in order to provide the full strength to the motors later. During this charging phase the M4 will not respond to anything that happens in your patch.

Similar – when you turn off your rack – the M4 needs to discharge the capacitors for safety reasons. It does this by running all motors at full speed down and also doing an LED animation in white and blue. Just before the end the LEDs just glim red, because the green and blue part of the LEDs need a higher voltage and go off first.

**Before unmounting the M4, switch of the rack and wait until this animation has stopped completely. Then it is save to remove and put away the M4.**

#### Using the faders in your patches

The traditional way of using motor faders is that you have several *presets*. Every preset holds a certain fader position. With some other control, e.g. a button, you can switch between presets and the new setting of the fader becomes active immediately. This is the classical application for mixing desks, where you can use presets for different mixes that you have prepared for different musical situations. You find general information about presets on [page 21](basics.md).

There is a second even more interesting application, however: You can assign multiple *overlayed functions* to one fader. For example one single fader could control attack, decay, sustain and release of an envelope. So just in order to save rack space and money you use one input device for controlling several parameters. In this application switching between the different functions does *not* alter any value. It just gives you access to control another parameter. And – as opposed to encoders – the motor faders act as a display for showing you the current values of the parameters.

The `droid` motor faders are designed to do both applications: presets, overlayed functions and even both at the same time, because it make absolutely sense.

A speciality of the M4 – however – are its capabilities for *force feedback*. With the help of the motors it can simulate artifical *notches* or dents and thus convert a fader into a linear switch with a specific number of fixed positions. You can really feel these notches and that way easily switch between clock divisions, notes of a musical scale and whatever else you like – without the need of any display. It can also simulate something similar to a pitch bend wheel, where the fader always wants to move back into the center.

The most basic and elementary way to use faders in your patch is using the [`motorfader`](circuits/motorfader.md) (see [page 343](circuits/motorfader.md)) circuit. When you are creating patches with many faders, please also have a look at [`faderbank`](circuits/faderbank.md) (see [page 247](circuits/faderbank.md)) and [`fadermatrix`](circuits/fadermatrix.md) (see [page 250](circuits/fadermatrix.md)). Those circuits manage a collection of faders with a single circuit and make your patches simpler.

In addition there is the [`motoquencer`](circuits/motoquencer.md) (see [page 315](circuits/motoquencer.md)) circuit which is a building block for simple and complex performance sequencers based on motor faders and the experimental specialised [`firefacecontrol`](circuits/firefacecontrol.md) (see [page 256](circuits/firefacecontrol.md)), which turns an RME Fireface audio interface into a motorized mixing desk.

As a starting point for further reading I suggest starting with the circuit [`motorfader`](circuits/motorfader.md) (see [page 343](circuits/motorfader.md)).

#### The touch plates

Below each fader the M4 has one touch plate with an integrated RGB LED. The touch plates are usable as buttons in your patch. Whenever a finger is touching the plate, the respective button register `B` outputs `1`, otherwise `0`. In addition, the circuit [`motoquencer`](circuits/motoquencer.md) (see [page 315](circuits/motoquencer.md)) makes implicit use of the touch plates (and maybe some future circuits, too).

Unfortunately, however, touch plates don't have two definite metal contacts like in the buttons of the P2B8, B4B2 and B32, but work by measuring the time an internal capacitor needs to load.

When you lay your finger on a touch plate this time increases as some of the current is deviated into your finger and thus the loading time increases. This implies some inherent fuzziness since environmental effects such as the moisture of your finger or how well you are electrically grounded play a role.

If you experience your touch plates not to react properly to your finger, you can adapt the sensitivity by using the circuit [`droid`](circuits/droid.md) (see [page 205](circuits/droid.md)) with the parameters `m4touchgain1`, `m4touchgain2`, …. There are up to eight such parameters because you can have different settings for up to eight M4s. The default gain is `0.7`. Using a larger value makes it more sensitive (`1.0` is the maximum).

The following example makes the touch buttons of the first two M4s more sensitive:

```droid
[droid]
    m4touchgain1 = 0.9
    m4touchgain2 = 0.9
```

For checking out different values you might want to attach a pot until you have found a good value:

```droid
[droid]
    m4touchgain1 = P1.1
    m4touchgain2 = P1.2
```

After you have dialled in good values, you can make a status dump (see [page 108](hardware.md)) to learn the actual values – or use the library mode of your DB8E display (see [page 80](hardware.md)).

#### The LEDs

The LED below the touch plates can be accessed with an `L` register – just like in the P2B8. In addition, there is a `R` register that controls the color of the LED, similar to those on the master. If you just use the `R` registers, the LED will light in full brightness. If you just use the `L` register, the LED lights white in the brightness specified by the value you feed into that register. Using both `R` and `L` at the same time gives you control over brightness and color.

#### Registers

Here is the summary of all M4 registers, assuming that `[m4]` is your first declaration in your patch:

| Register | Meaning |
|----------|---------|
| `B1.1 … B1.4` | Touch plates |
| `L1.1 … L1.4` | LED brightness |
| `R1.1 … R1.4` | LED color |
| `P1.1 … P1.4` | Current physical fader positions |

Note: The `P1.1 … P1.4` registers show the physical position of the faders in a range from 0.000 to 1.000 – regardless of which overloaded function they currently have. But while they are configured to have *notches* (see below), these P-registers behave differently. Now they show the notch number the fader currently rests in – as an integer like `0`, `1`, `2`, ….

#### The motor faders

The `droid` M4 has four industrial class motorized faders with 60 mm action range from ALPS. They are a combination of normal linear potentiometer with an electrical motor that can move that potentiometer. The motor is not a step motor but runs continously. The M4 software determines the current position of the fader by reading out the value of the potentiometer and controls the motor to move to the desired position.

The motor control is done via pulse width modulation (PWM), whose frequency is way beyond the audible range.

#### Adapting the fader power

Using the circuit [`droid`](circuits/droid.md) (see [page 205](circuits/droid.md)) you can adapt the motor power of the faders. There are two settings. One is for the normal movement power (and hence speed). The other one is for tuning the power of the haptic feedback when you work with notches. Try mapping both parameters to pots and you can test their influence:

```droid
[droid]
    m4faderspeed = P1.1
    m4notchpower = P1.2
```

#### The power management

Motor faders are nice but need lots of power. As a matter of fact, one fader could use up to 800 mA from your 12 V rail when the motor is running at full power - if you would run it directly from the Eurorack power supply. So even a single M4 would need 3.2 A for full operation. That's a lot more than a typical power supply provides. And it's just one module! That's probably the main reason why we haven't see flying faders in Eurorack sooner.

We have solved the issue in the M4 by means of modern supercapacitors (supercaps). Those little miracles can store up to 100 times more energy per volume than electrolytic capacitors and can accept and deliver charge much faster than batteries. They also tolerate many more charge and discharge cycles than rechargeable batteries. The four supercaps of the M4 can deliver 3.2 A for the faders with ease - of course with the limitation of doing it just for a short time. That's not an issue in a normal usage pattern of the faders, since they move super fast and just for fractions of seconds.

When you power up your M4, you will notice that it takes some time to become operational. That is because it needs to load the supercaps before the show can begin. That time is somewhere in the range of 60 to 90 seconds. The current loading state is indicated by an LED travelling from left to right again and again. The colors starts red, goes yellow and gets green just before the module is powered up.

Note: when you work with the faders and let them jump back and forth very fast very often, it can be the case that the supercaps run out of power. In that case the fader motors will go off for a couple of seconds, the supercaps recharge and the powerup LED animation is visible (with green LEDs).

The M4 has an intelligent charging mechanism that manages the power of the supercaps and makes sure that there is enough power for fader movements while not exceeding a *limit* of current that is drawn from your Eurorack +12 V power rail. With two jumpers on the back of the module you can set the maximum charging current of the M4:

- The minimum charging limit of the M4 is 350 mA.
- With the left jumper you can raise that by 150 mA.
- With the right jumper you can raise that by 100 mA.

That way you can choose between 350 mA, 450 mA, 500 mA and 600 mA. The more power your allow the M4, the faster it charges up and the more fader movements per second it can do.

If you allow the M4 to draw too much current, your Eurorack power supply can overload. That might lead to various problems:

- It could overheat.
- It could blow its fuse.
- It could trigger its short circuit detection and switch off itself.
- The voltage of the 12 V rail could drop too much.

Please make sure that you use the M4 in a way that is within the specification of your power supply.

The good new for last: once the M4 is charged up and when you use the fader in a reasonable way, the power consumption of the M4 is much lower than the maximum limit. This is an important difference from modules like those with vacuum tubes that need their heating power all the time.

#### Discharging

When you switch power off, the M4 still has lots of energy stored in its supercaps. For safety reasons, it will discharge the supercaps as fast as possible as soon as it detects main power off. Discharge is done by constantly moving all fader motors downwards and lighting the LEDs in which with the maximum brightness - with one blue LED wandering from left to right.

At some point in time the voltage is not suffient to drive the motors anymore. The LEDs are still animated. Later they will get red and slowly fader out.

**Do not unmount the M4 from the rack until all LEDs are off!**

This is important to avoid short circuits by accidentally connecting the supercaps with metal of the case or the like.

#### Software update for the M4

If you install a new software on your master, please check the release notes if there is also a new firmware for the M4. These updates happen rarely and are not strictly necessary, since old versions of the M4 are always compatible. But the new firmware might improve the performance of the M4 or fix some bugs that you have encountered.

First check the version of the firmware that's running on the M4. When you power on the module, the four LEDs flash a couple of times in some color. This color and the number LEDs together form the version. If three LEDs are flashing orange - voila, you have version orange-3.

Now find the new firmware file for the M4 in your `droid`'s software release. You find this in the ZIP file in the sub folder `firmware`. It's called something like `m4-orange-4.fw`, where `orange-4` is the firmware's version.

- If that's already installed on your M4, do nothing.
- If the new version is different, proceed with the update.

This is how to upgrade your M4 firmware:

1. Switch off the power of your rack.
2. Remove all controllers and the X7 from your master (the G8 does not harm, leave it there if you like).
3. Attach the M4 as the first and only controller.
4. Set the jumper on the M4 to LAST.
5. Copy the firmware file for the M4 from the `firmware` directory of the ZIP file to the SD card and rename it to `m4.fw`. Insert the SD card back into the master.
6. Switch on your rack.

If you have a **MASTER18**, the update happens automatically. The four LEDs on the M4 light up in yellow one after another and act as a progress bar.

Now make sure that you delete the `m4.fw` file from the SD card before you reassemble your modules and continue making music.

The procedure on the **MASTER** is different.

1. Enter the maintainance menu with a long press of the button on the MASTER. The maintenance menu should show a yellow menu item at position 6 (if not, the SD card or the file `m4.fw` on it is missing):

2. Select menu item 6 (yellow) by pressing the button shortly five times. (short presses move the cursor, long presses select an item).
3. When the blinking cursor is on item 6, do a long press. This will start the upgrade procedure. Now the M4's LEDs light yellow one after another.
4. After the update select menu item 1 (white) to leave the menu of simply switch off your rack.

Important notes:

- The firmware upgrade does not work over a R2M/R2C bridge. You need to attach the module directly.
- If you have a MASTER18, delete the `m4.fw` file from the SD card after you have finished. Otherwise your master will not start anymore but stay in the firmware upgrade mode forever.
- Don't forget to put the M4's jumper back to PARK if it is not the last controller in you chain.

### 6.12 The DB8E display controller

The DB8E is a controller with a little display (`D`), eight push buttons (`B8`) and one rotary encoder (`E`) with an LED square made of 32 tiny multicolor LEDs. The encoder and it's LEDs are the same as on the E4 (see [page 74](hardware.md)).

If you just want to add a display to your patch, install and wire the DB8E just like any other controller. Don't forget to add it as a controller in the Forge or add `[db8e]` to your patch. That's all. The display will automatically show useful information.

The buttons and the encoder are used to control the contents of the display. In addition, you can use them as normal controls in your patch. All details will be explained in this section.

**Important**: For the DB8E to work, you need at least the firmware **blue-7** on your master.

#### Installation

Before your begin, make sure your MASTER or MASTER18 has at least the firmware **blue-7**!

The DB8E is wired just as any other controller.

- Wire the connector labelled IN with the shipped 6-pin controller cable to your MASTER or MASTER18 or to whatever other controller is previous in the chain of controllers.
- Wire the OUT connector of the DB8E to the next controller and set the jumper to PARK if you have more controllers after the DB8E in the chain.
- Set the jumper to LAST if this is the last controller.
- The DB8E needs to connected to your rack's power supply with the shipped 10 pin cable.

Don't forget to add the DB8E to your patch. This is done in the Forge or by adding `[db8e]` to your patch manually.

Since the DB8E has a display, it can help you a bit if you have done something wrong. If there is no working communication with the master module, the display shows `not connected`.

If there *is* a communication with the master but the DB8E is not listed in your patch, the display shows `not used by patch`.

If the modules listed in your patch do not match the actual wiring in your rack – and the DB8E gets mixed up with some other controller – the display shows you a configuration error: `configuration error`.

#### Stickers

The DB8E comes with a small sheet of stickers for its eight buttons. You might want to apply these as shown on the left. The eight buttons are used to control the display, unless you remap them to your own functions in which case the stickers might get in your way.

#### The hardware of the DB8E

Aside from a 128 × 64 pixel OLED display the DB8E is a normal controller with eight push buttons - just the same as the P2B8 has (see [page 68](hardware.md)) - and one encoder with an LED ring like the ones in the E4 (see [page 74](hardware.md)).

The buttons and the encoder control the display but you can use them in your patch (see below how).

The DB8E is 6 HP wide, has the same STM32F030 micro controller as all the other controllers and has a typical power consumption of 40 mA on the +12 V rail. If you make all LEDs white, including those around the encoder, and as many white pixels on the display as possible, the power consumption maxes out at about 103 mA.

#### Using the display

Using the display is easy and most times simply works without change in your patch other than declaring the DB8E as a controller. Here is when and what is being displayed:

**1.** When you operate any pot, slider, fader or switch, the display immediately shows the new value of this control.

**2.** Circuits that accept user interaction, like [`pot`](circuits/pot.md), [`encoder`](circuits/encoder.md), [`motorfader`](circuits/motorfader.md) or [`motoquencer`](circuits/motoquencer.md), use the display to show their changed state when you interact with them.

**3.** You can use the circuit [`display`](circuits/display.md) (see [page 199](circuits/display.md)) to show a custom value or text in the display.

**4.** When you press button 5 on the DB8E, you enter the *Library mode*. In this mode you can interactively select any of the registers of your master to be displayed.

When more if these events happen at the same time, the latter one in the upper list takes precedence and "gets" the display. So the order is:

Library Mode > [display] > Circuit > Control

An example of this priotorisation is when you turn a pot that is used by a [`pot`](circuits/pot.md) circuit. Both a control is operated and the state of a circuit with user interaction is changed. The latter one has precedence so here the [`pot`](circuits/pot.md) circuit owns the display. If you want to see the value of the pot anyway, use the library mode.

Now let's have a look at these four display "sources" in detail:

##### Showing controls

Switches have discrete numbers starting from `0`. When you change the position of a switch, it's new setting is shown as an integer number:

When you move a pot, fader or slider, its new value is shown as a *continous* value. In the following example it is shown as a gauge:

Whenever such a continous value is displayed, you can use the buttons 1, 2, 3, 4 and 6 on the DB8E to tune *how* it is formatted. This not only works for pots but also for things like inputs and outputs in the library mode.

**Button 1** (NUM) selects a number display. Pressing it cycles through three versions of a number display. It starts with a fraction, e.g. `0.278`. Pressing button 1 again switches to a *voltage* display, e.g. `2.78V` (a value of 0.278 is the same as 2.78 V in the Droid ecosystem). Press button 1 once more to bring up a percentage display, e.g. `27.8%`.

In all of these three number formats you can use button 6 (the magnifier) to cycle through three levels of *precision*. This affects the number of shown decimal digits, e.g. `27.79%` at the highest precision, `28%` at the lowest.

If you deal with values between 0 and 1 (such as the outputs of pots), you might prefer a graphical display.

**Button 2** (the gauge icon) toggles between two types of gauges. You start with the simple gauge (a horizontal bar). Another press on button 2 switches to a more "fancy" gauge, that displays the percentage below the gauge (honoring the precision setting of button 6). If the value is *exactly* 0.5, the pointer of the gauge "nocks in". It moves a bit down.

**Button 3** (the note icon) shows the displayed value by interpreting it as a 1V/oct pitch control voltage. Voltages without a fraction are shown as the note *C*, and the octave matches the voltage, so 2.0 V is shown as C 2. If the voltage does not exactly match the 1/12 V raster, a detuning information is shown in cents, where 100 cent correspond to 1/12 V. For example the value 0.154 corresponds to 1.54 V, which is displayed as a F♯ in the first octave with a detune of 47 cents sharp (`F♯1 +47`).

**Button 4** (the LFO icon) is useful if you want to watch a value that is changing over time, such as the output of an LFO. Press it to select *sparkline* mode. Pressing the button several times toggles the displayed range between 0 … 1 (unipolar mode) and -1 … 1 (bipolar mode). You can change the speed of the progression by *holding* button 4 *while turning* the encoder knob.

Note: This display is not the same as an oscilloscope since it lacks a means for synchronizing to a waveform. And the maximum speed is limited since the displayed data needs to be transferred through the controller bus with all the information about knobs, buttons and LEDs of all of your other controllers.

##### Circuits with user interaction

The second type of event that activates the display is a state change of a circuit with user interaction. The idea is this: When you operate a control that changes a circuit's state, you rather want to see that state and not the raw value of the control.

As an example let's see the circuit [`pot`](circuits/pot.md) (see [page 362](circuits/pot.md)). A common use of [`pot`](circuits/pot.md) is to overlay a physical pot with several different virtual values. Here is a sample patch snippet:

```droid
[pot]
    select = _SEL
    pot    = P2.1
    output = O1
```

When `_SEL` is `1`, the [`pot`](circuits/pot.md) circuit comes into action and any movement of `P2.1` will change the output value. Since you can move the pot while the circuit is not selected, the output value differs from the physical pot value.

Here the display of the DB8E shows the modified virtual value rather than the pot, e.g. `4.34V` for Output O1.

Again, you can change the number display format with the buttons 1, 2, 3, 4 and 6. In this example the header "Output O1" has automatically been derived from where the parameter `output` points to. But you can use your own header by using the input `header`:

```droid
[pot]
    select = _SEL
    pot    = P2.1
    output = O1
    header = "Pitch bass"
```

The same principle works for many other circuits. Some circuit like [`motoquencer`](circuits/motoquencer.md) (see [page 315](circuits/motoquencer.md)), [`calibrator`](circuits/calibrator.md) (see [page 164](circuits/calibrator.md)) or [`vcotuner`](circuits/vcotuner.md) (see [page 407](circuits/vcotuner.md)) even have a more fancy custom display layout. They do not simply display a *value* but something more complex. Here is how the [`calibrator`](circuits/calibrator.md) circuit looks like (a small envelope-like graph with the numbers 1, 2, 3).

In such a case the buttons 1, 2, 3, 4 and 6 don't have any function.

If you *don't want* a circuit to display something, look for the parameter `display`. You can set `display = 0` to suppress any output.

##### Displaying your own stuff

The third way to bring the display to life is by using the circuit [`display`](circuits/display.md) (see [page 199](circuits/display.md)). This allows you to show custom values or texts. The following patch snippet shows the value of the internal cable `_SPEED`:

```droid
[copy]
    input  = P2.1 * 120
    output = _SPEED

[display]
    value = _SPEED
```

Here is the display when you turn the knob (the circuit only jumps into action whenever the input value changes), e.g. `95.03`.

You can add a header by using the parameter `header`. It accepts a text value - something enclosed in double quotes:

```droid
[display]
    value  = _SPEED
    header = "Speed (BPM)"
```

You can also show texts, for example to indicate the current setting of a [`buttongroup`](circuits/buttongroup.md).

The [`display`](circuits/display.md) circuit has many tuning options. Please refer to the [page 199](circuits/display.md) for all of the details.

##### The library mode

The fourth and last usage of the display is the *library mode*. You can turn it on and off with button 5 (the book icon). While library mode is enabled, you manually select which value to display, regardless of what's going on in your patch. That way you have access to interesting information that's otherwise not accessible.

While in library mode, with the buttons 7 and 8 you select the *category* of register to show. The current category is shown in a footer at the bottom of the screen, for example the internal patch cables (e.g. `VOICE_1_PITCH` reading `0.1486`, category "Cables").

Within the category use the encoder to select one of the available registers.

There are eleven categories, most of which correspond to registers like `I` for input. The following table shows these register characters in the last column.

| # | Category | Register |
|---|----------|----------|
| 1 | Inputs | `I` |
| 2 | Input normalizations | `N` |
| 3 | Outputs | `O` |
| 4 | Gates | `G` |
| 5 | Pots & faders | `P` |
| 6 | Buttons | `B` |
| 7 | Switches | `S` |
| 8 | LEDs in buttons | `L` |
| 9 | RGB LED colors | `R` |
| 10 | Cables | |
| 11 | Special | |

Category 10 does not show registers but the current value of all internal patch cables.

And the last category – 11 – called *Special*, shows things that are not registers but rather internal information about your `droid` system. These are also available in status dump files (see [page 108](hardware.md)). Here, for example, you can see the timing in your controller bus. This is the typical latency of a button press in your controllers, which depends on the number and type of your controllers (e.g. "Controller cycle" reading `14.6 ms`, category "Special").

##### Using the DB8E as voltmeter

You can turn your `droid` master into a voltmeter using library mode. Just select one of the inputs (first category) and attach some signal to the master. You might want to select *Volts* as display format via button 1 and select a precision with button 6 (e.g. `5.330V` for Input I1).

##### Using more than one DB8E

You are allowed to attach more than one DB8E to your master. Even if you don't need more than one display, the DB8E is still a good controller for its eight buttons and the encoder. But making use of several displays at once is still possible and can be useful. Here is how it works:

- Automatic display of moved pots, sliders and faders is always done on the first display.
- Each DB8E has its own button for a library mode. You can use one of the displays in library mode to watch a specific value while the other display operates in automatic mode.
- Circuits that automatically display their state (such as [`pot`](circuits/pot.md), [`encoder`](circuits/encoder.md), [`motorfader`](circuits/motorfader.md), etc.) have an input called `display`. Setting `display = 2` tells the circuit to use the second DB8E in your patch for displaying it's state. If you don't use `display`, they use the first display.

So unless you set `display = 2` at some of your circuits, the second display is only usable in library mode.

##### The screensaver

The little screen of the DB8E is an OLED display. The advantage is its high contrast and brightness. A disadvantage of OLED displays is that pixels can burn in if the are lit all the time. In order to avoid this, the DB8E has a builtin screen saver after some time of inactivity.

As soon as you move any control on *any* of your controllers, the screen jumps back to live.

You can change the screensaver timeout with the parameter `screensaver` of the [`display`](circuits/display.md) circuit. This example sets it to one minute:

```droid
[display]
    screensaver = 60
```

##### Using the controls

Even if it wouldn't be for the display, the DB8E makes an interesting controller because of it's rotary encoder and the eight buttons. You can use these controls in your patch if you tell the DB8E not to use them for the display. This is done by adding a [`display`](circuits/display.md) circuit to your patch and use `takeovercontrols`:

```droid
[display]
    takeovercontrols = 1
```

Now you can use `B1.1 … B1.8` and `L1.1 … L1.8` in your patch for making use of the buttons (assuming that the DB8E is the first in the chain, of course).

The push button in the encoder can be accessed with `B1.9`. It is never used by the display and even works without `takeovercontrols`! So you can use it for switching the controls between the display and you patch. Just add a button circuit. Then pushing the encoder alternates the two usages of the controls:

```droid
[button]
    button = B1.9
    output = _TAKEOVER

[display]
    takeovercontrols = _TAKEOVER
```

You can also use the encoder. All details about it are in the chapter about the E4 (see [page 74](hardware.md)). The only difference is that you have just one of these, not four. Here is a complete patch example for using the encoder to control the voltage of the output `O1`:

```droid
[db8e]

[display]
    takeovercontrols = 1

[encoder]
    encoder = 1
    output  = O1
```

There is also an `L1.9` register. If you send a `1` to it, the LED ring will light up all white. See the chapter about the E4 for details.

#### Software update for the DB8E

The firmware of the DB8E is more complex than those of most other controllers. In particular it contains software for display layouts that support individual circuits like [`vcotuner`](circuits/vcotuner.md) (see [page 407](circuits/vcotuner.md)), [`motoquencer`](circuits/motoquencer.md) (see [page 315](circuits/motoquencer.md)), [`calibrator`](circuits/calibrator.md) (see [page 164](circuits/calibrator.md)) and many others.

This means that after updating your master's firmware the DB8E might also need an update in order for its display to work correctly with the new features. If the software of the display is not up-to-date, some of the circuits might not get displayed properly. In particular, if the master sends display contents to the DB8E that the latter cannot understand, it shows a screen like `update firmware`.

As long as your display works as you expect, there is no need for an update. But when you upgrade your master I suggest that you check the release notes if there is also a new firmware for the DB8E. Here is how to upgrade your DB8E firmware.

First check the version of the firmware that's running on the DB8E. This is shown briefly after powerup after the "Der Mann mit der Maschine" logo, for example `neol-1`.

Now find the firmware file for the DB8E in your `droid`'s software release. You find this in the ZIP file in the sub folder `firmware`. It's called something like `db8e-neol-2.fw`, where `neol-2` is the firmware's version.

- If that's already installed on your DB8E, do nothing.
- If the new version is different, proceed with the update.
- If there is no firmware file for the DB8E, you have an old version of the master's firmware that does not support the DB8E. In this case you need to get a new `droid` software release and update your master first. A minimum of **blue-7** is required. Update your master and come back here.

This is how to upgrade your DB8E firmware:

1. Switch off the power of your rack.
2. Remove all controllers and the X7 from your master (the G8 does not harm, leave it there if you like).
3. Attach the DB8E as the first and only controller.
4. Set the jumper on the DB8E to LAST.
5. Copy the firmware file for the DB8E from the `firmware` directory of the ZIP file to the SD card and rename it to `db8e.fw`. Insert the SD card back into the master.
6. Switch on your rack.

If you have a **MASTER18**, the update happens automatically. The LED ring around the DB8E's encoder fills with cyan colored LEDs within the next couple of seconds. This shows the progress. At the end, the DB8E starts.

Now make sure that you delete the `db8e.fw` file from the SD card before you reassemble your modules and continue making music.

The procedure for the **MASTER** is different.

1. Enter the maintainance menu with a long press of the button on the MASTER. The maintenance menu should show a cyan menu item at position 2 (if not, the SD card or the file `db8e.fw` on it is missing).
2. Select menu item 2 (cyan) by pressing the button once shortly. (short presses move the cursor, long presses select an item).
3. When the blinking cursor is on item 2, do a long press. This will start the upgrade procedure. Now the DB8E's encoder leds light up cyan on after another until the DB8E restarts.
4. After the update select menu item 1 (white) to leave the menu of simply switch off your rack.

Important notes:

- The firmware upgrade does not work over a R2M/R2C bridge. You need to attach the module directly.
- If you have a MASTER18, delete the `db8e.fw` file from the SD card after you have finished. Otherwise your master will not start anymore but stay in the firmware upgrade mode forever.
- Don't forget to put the DB8E's jumper back to PARK if it is not the last controller in you chain.

## 7. The G8 expander

### 7.1 Introduction

The G8 expander gives you eight additional jacks, each of which can be used as a gate or trigger input or output. They are ideal for working with clocks, gates and triggers, but can be used for simple CV modulations, as well.

There are two hardware versions of the G8. Version 2 was introduced 2023 and allows you to chain up to four G8 expanders to one master. For that purpose it has *two* connectors on the back: one to be connected to the master, one for the next G8.

The original G8 version 1 has only one connector. There is no need to be sad if your G8 is version 1, since it still can work in a chain with more G8s if it is the last one. So if you want a second G8, simply get a version 2 one and use the old one as the second G8.

### 7.2 Installation

If you have just one G8 version 1, simply use the 8 pin ribbon cable that has been shipped with your G8 and connect the G8 to the 8 pin port of the master as shown in the following picture. Put the red stripe down in both modules.

The G8 version 2 has two connectors. Here use the right one labelled "Master":

To create a chain, wire the master to the "Master" input of the first G8, which must be version 2. Then wire the other connector of this G8 to the "Master" input of the second G8 and so on. No termination jumper is needed. The last G8 in the chain can either be version 1 or version 2.

### 7.3 Using the G8 in patches

The name of the registers of the G8 jacks in your patch depend on which master you use.

**MASTER**: You can access the jacks of the first G8 with the registers `G1`, `G2` … `G8`. If you work with more than one G8, you need to use a dot notation and write the number of the G8 in the chain before the dot. So the gate 5 on expander 3 would be `G3.5`. If you want, you can use this "dot notation" also for the first G8, hence `G1.1` … `G1.8`.

**MASTER18**: Since this type of master has four gate outputs integrated, the gates of the G8 have to get other numbers. So the first G8 has the eight registers `G2.1` … `G2.8` and the fourth G8 would get `G5.1` … `G5.8`.

This is how the gates on the G8 work:

- Each jack can either be used as input or as output.
- When used as input it will read a value of 1 at an input voltage of approx **0.75 V** or above and 0 otherwise (also for negative voltages).
- When used as an output, it outputs **5 V** when you send a value 0.1 or higher to its register, and 0 V otherwise.

Why do the gates not output 10 V? Well, while this would be more logical, it was actually impossible to do in hardware easily since the G8 needs a very special chip that is able to switch between input and output via software. This chip does not support 10 V.

99.9% of all Eurorack modules will happily accept 5 V as a valid trigger. Some analog envelopes with vintage circuitry might need higher voltages. If you encounter such a module, you can use one of the outputs of the master, which is able to output 10 V.

The G8 also has eight multicolored LEDs. These indicate inputs in blue lights and outputs in red, when high. You can override the default function of LEDs in order to display something of your own liking. Use the registers `R17` … `R48` for that purpose.

There is nothing special to do in your `droid.ini` for setting up the G8 expanders. They don't need to be declared like the controllers. Using the `G` registers enables the expanders automatically. If you load a patch with `G` registers but don't have a G8, nothing dangerous happens and the rest of the patch will work normally.

## 8. The X7 expander

### 8.1 Quick start

You already know what the X7 is all about? Want to start immediately? Here is a super short quick start guide for experienced DROID users:

1. Wire the X7 to your master just like a controller. It must be the first in the chain.
2. Use the MIDI functionality via the circuits [`midiin`](circuits/midiin.md) (see page 289), [`midiout`](circuits/midiout.md) (page 297) and [`midithrough`](circuits/midithrough.md) (page 306).
3. Access the four gates via `G9`, `G10`, `G11` and `G12`.
4. Connect the USB cable and set the switch *left* for USB access to the SD card. Set it back to the middle position for disconnecting USB and loading the patch.

### 8.2 General overview

#### Features and applications

Welcome to the X7 expander. The X7 gives you USB and MIDI connectivity for your MASTER and also four gate outputs with modular levels.

The X7 can also be used with the MASTER18 in order to add more MIDI connectivity and the four additional gates, even if the MASTER18 already has USB and MIDI integrated.

You can process incoming and generate outgoing MIDI streams, both via classical DIN cables and via USB. Both in and out directions support polyphony with eight or even more voices.

For size reasons the X7 uses 3.5 mm TRS jacks for MIDI instead of the classical DIN jacks. But it comes with two DIN ↔ TRS adapters, so you are free to use either form factor.

As a bonus feature, the X7 provides super fast loading of DROID patches via USB – without any need for putting the SD card in and out anymore.

Here are some examples of what you can do with the X7:

- Attach an external keyboard to your modular.
- Use an external hardware sequencer for playing melodies and beats in your modular.
- Use an external MIDI controller to control your DROID patch.
- Do the same with a MIDI controller app on your tablet or phone (via USB).
- Use your modular for playing polyphonic music and beats on your hardware synths or software synth plugins in your DAW, tablet or phone.
- Connect two DROIDs (both with X7) and exchange values and triggers via CCs and notes.
- Use the four additional gate outputs on the X7 for sending clocks, gates and triggers and free your valuable CV outputs for other things.
- Access the SD card in your master just like a USB thumb drive for direct access to it via your PC, Mac, phone or tablet.
- Alternatively load new patches to your master via MIDI sysex from your PC – *and get your new patch ideas up and running in less than a second.*

#### The switch

At the top the X7 has a **switch** with three positions. This switch selects the current function of the USB port:

| Position | Function |
|----------|----------|
| left | Activate USB access to the SD card |
| middle | Don't use the USB port |
| right | Activate MIDI via USB |

Beware: in the left position the master will not work as usual and does not run your patch. See below for details.

#### The jacks

The X7 has the following jacks:

- One USB-C port for MIDI via USB and for access to the master's SD card from your PC
- One 3.5 mm stereo jack (also called TRS, which stands for "tip ring sleeve") for MIDI input, with *autosensing* for MIDI TRS type A and B
- One 3.5 mm stereo jack for MIDI output
- Four gate outputs for gate and trigger signals at modular level

This sums up to a total of seven ports, hence the name X7 (the original idea of naming it "U1M2G4" was soon abandoned, since that was too clumsy and also wouldn't fit on the face plate).

#### The LEDs

Similar to the master, the face plate has multicolor LEDs indicating what's going on at the seven ports:

- The top left LED shows the current state of the SD card in the master.
- The top right LED shows what's going on on the USB MIDI connection.
- The LEDs in the second row show incoming and outgoing MIDI data at the TRS ports.
- The four LEDs labelled 9, 10, 11 and 12 show the current state of the four gate outputs.

### 8.3 Installation

The installation of the X7 is very easy. These are the rules:

1. Wire the X7 to the shrouded 6-pin header on the top right of the master, just like P2B8, P10 or other controllers.
2. There is no jumper. You don't need one here.
3. Always install it **as the first module** in the chain!
4. Make sure that the switch is in the middle position when you start.
5. You can only attach *one* X7 to your master.

Just like all the controllers, the X7 has an *input* connector, which is at the top *right* side if you look from the back. On the *left* side is the *output* connector. Connect the master with the shipped 6 pin ribbon cable to the *input* connector. If you have any controllers, like P2B8, P10 and so on, wire the first of these to the output connector of the X7.

That's all. The X7 is powered from the master so there is no dedicated power cable.

Note: You don't need to change anything in your DROID patches for now. Even if the X7 is connected to the master like a controller, it does *not* need to be declared. And it also does *not* count when it comes to the numbering of `P1.1` and so on.

### 8.4 USB access to your SD card

The X7 can give you direct access to the SD card of the master via USB. Start with the switch in its middle position. And make sure the micro SD card is in its slot on the master. The top left LED of the X7 always shows you dim white light whenever a SD card is present.

Now connect the USB-C port on the X7 with your PC, Mac, Linux, phone or tablet (I'll just use "PC" for the rest of this manual) and set the switch on the X7 to the left. This enters "USB stick mode".

Note: For a USB-C ↔ USB-C cable to work, your X7 must at least have hardware revision "Rev 1.5.1". The revision is printed on the back of the module top right. Also you need at least the firmware "orange-912" on your X7 (see below for firmware upgrades). If your X7 has "Rev 1.3" or "Rev 1.2" or you have "orange-911" or earlier, the X7 needs a USB-A ↔ USB-C cable. For that reason such a cable is shipped together with the X7.

After a few seconds, your PC should detect a new storage device with the exact contents of the micro SD card. Since X7 is a "class compliant" mass storage device you don't need any driver on your PC.

If you work with the Forge, you should see the *Save to SD* icon become active and you can use that to write your patch to the SD card. Much faster is using MIDI Sysex, however.

If you don't like the Forge, you can edit `droid.ini` directly on the card or copy a patch from your PC to the card, just as you are used to when you are working with your SD card reader. The USB-Stick mode is also helpful for getting the `ERRORS.TXT` or `STATES1.TXT` file from your SD card, even if you work with the Forge.

When you are finished, *eject* the volume / disk on your PC. After that set the switch back to its middle position. This will remove the USB connection and also automatically launch the new DROID patch. So you don't need to press the button on the master.

A few notes:

- If your patch has an error (blinking LEDs and stuff, see page 51) put the switch back to the left, wait for the SD card window to popup and look for the file `DROIDERR.TXT`. Open it and you will see the exact reason for the error.
- The access to the SD card via the X7 is slightly slower than using an SD card reader on your PC since it takes the extra miles via the X7.
- If you need to re-format the card for some reason, better do this in the micro SD card reader that was shipped with your master. It's much faster that way.
- If you are working with Mac and experience that the access is slow, check out disabling Spotlight on the card. A script for that can be found on page 115.

### 8.5 MIDI

#### MIDI features overview

One key feature of the X7 is working with MIDI. The combination of the DROID master with the X7 probably forms the most flexible, comprehensive and powerful MIDI converter in Eurorack land. Here are some of the key features:

- Support for both MIDI → CV and CV → MIDI at the same time.
- Unlimited polyphony (number of simultaneous notes) except that you run out of jacks.
- The MIDI streams of USB and TRS can be used independently in parallel, so you have two input and two output streams.
- Flexible "MIDI through" routing while splicing in and out events.
- Comprehensive support and access to the vast majority of MIDI features such as CCs, clocks, the running state, pitch bend, all types of pedals and much more.
- Automatic pitch stabilization detection in the CV/gate → MIDI conversion, thus working precisely with Eurorack sequencers and quantizers.
- Super fast DROID patch upload via USB-MIDI Sysex.

And of course you benefit from DROID's own flexibility when it comes to quantization, LFOs, chord generators, switches and all that stuff.

#### MIDI over DIN

For space reasons, the X7 uses 3.5 mm stereo jacks (TRS) for MIDI. But we ship two TRS to DIN adapters with the X7. Use these for connecting classical DIN MIDI devices.

**Note: When you use one of the shipped adapters for the MIDI output via DIN, make sure that the switch at the back of the X7 is set to position B (up).**

#### MIDI over USB

The X7 supports MIDI over USB. Hereby it acts as a *USB device*. This does *not* mean any limitation of being an input or output device. It can be both. Even at the same time. But the actual limitation is that the X7 cannot provide power to your MIDI devices and cannot be a USB host.

That means that MIDI devices that are USB devices themselves cannot be connected to the X7 via USB, even if you have a matching cable. Connect your MIDI keyboards and controllers with the TRS jack if USB doesn't work for you here.

But the USB port is perfectly suitable for connecting the X7 to your PC, Mac, tablet or phone. The MIDI implementation is "class compliant". That means that you do not need any driver software. Simply connect the X7 with the shipped (or any other) USB-C cable to your PC and set the switch to the right. You should now see a new MIDI device, which can be selected as input or as output depending on what you are going to do.

Note: As of now the USB-MIDI standard has a concept of up to 16 virtual MIDI "cables". The X7 receives data on all cables and always sends on cable 0. Future software updates might make this more flexible, if there is demand.

By the way: MIDI over USB is not restricted to the standard MIDI data rate of 31250 bits per second.

#### The LEDs

When working with MIDI, watch the corresponding LEDs. Here is what the colors mean:

| Color | Meaning |
|-------|---------|
| black | no data transmitted |
| dim white | steady activity |
| green | note on |
| red | note off |
| blue | some other MIDI event |

The top right LED shows the status of USB-MIDI. The third LED shows MIDI data via *incoming* TRS. The fourth LED shows MIDI data via *outgoing* TRS.

#### MIDI to CV (MIDI input)

The most common application for MIDI and modular synthesizers is converting MIDI note events to CV/gate signals. When you press a key on a MIDI keyboard or when a MIDI sequencer starts playing a note, a MIDI "note on" message is being sent over the wire. Likewise at the end of the note a "note off" message is sent.

A typical MIDI to CV module receives these messages and feeds at least two jacks: one with the pitch of the currently played note in form of the typical 1 volt per octave scheme. And one *gate* output which is high (e.g. at 5 V) while the key is being hold.

Of course there is much more, like clock signals, controllers and so on. This X7 can give you access to the vast majority of MIDI features.

The hardware connection is done either with the 3.5 mm TRS jack or via USB (or both at the same time). The X7 comes with two identical TRS ↔ DIN adapters, so you can use the much more wide spread classical MIDI cables with DIN plugs.

Even if you don't use our adapters but use the 3.5 mm jacks directly, you don't need to care about MIDI "A and B". The X7 does autosensing at its input. Either way will work. Just make sure you use *stereo* cables. Normal modular patch cables don't work.

The basic operation is super simple. All is done with the circuit [`midiin`](circuits/midiin.md) (see page 289). This example converts MIDI to a pitch CV at output `O1` and a gate at output `O2`:

```droid
[midiin]
    pitch = O1
    gate = O2
```

The source is the TRS jack. But you can easily select MIDI via USB instead with the `usb` parameter:

```droid
[midiin]
    usb = 1
    pitch = O1
    gate = O2
```

Per default, `midiin` processes notes from all 16 MIDI channels. You can select one specific channel with the `channel` jack:

```droid
[midiin]
    channel = 5
    pitch = O1
    gate = O2
```

Note: You can use up to 32 `midiin` circuits in your patch. So you could add one circuit for each MIDI channel that you want to process.

For polyphonic patches with more voices simply specify more pairs of gate and CV. This example supports three simultaneous notes:

```droid
[midiin]
    pitch1 = O1
    pitch2 = O2
    pitch3 = O3
    gate1 = O5
    gate2 = O6
    gate3 = O7
```

If you have a [G8 expander](hardware.md) (see page 87), you can directly control eight analog voices:

```droid
[midiin]
    pitch1 = O1
    pitch2 = O2
    pitch3 = O3
    pitch4 = O4
    pitch5 = O5
    pitch6 = O6
    pitch7 = O7
    pitch8 = O8
    gate1 = G1
    gate2 = G2
    gate3 = G3
    gate4 = G4
    gate5 = G5
    gate6 = G6
    gate7 = G7
    gate8 = G8
```

Notes have velocities, also there are MIDI *controllers* like the volume, the modulation wheel or more. These can directly be accessed via output parameters:

```droid
[midiin]
    pitch = O1
    gate = O2
    volume = O3
    modwheel = O4
    ccnumber1 = 17 # get CC number 17
    cc1 = O5       # output that on O5
```

Also you get simple access to various MIDI clocks and the start and stop status:

```droid
[midiin]
    clock = G1
    start = G2
    stop = G3
    running = G4 # alternative to start/stop
```

The MIDI notes needn't be used for playing voices. The following example uses the note for selecting a root note for a [`minifonion`](circuits/minifonion.md) (see page 308):

```droid
[midiin]
    pitch = _PITCH

[minifonion]
    root = _PITCH * 120
```

You even can use MIDI keys (maybe from controller pads) as buttons.

```droid
[midiin]
    note1 = 24 # MIDI note number of C-0
    notegate1 = _KEY_C

[button]
    button = _KEY_C
    onvalue = 0.8
    offvalue = 0.2
    output = O1
```

This was just a quick overview and there are much more inputs and outputs available. Please have a look at page 289 for more details on [`midiin`](circuits/midiin.md).

#### CV to MIDI (MIDI output)

While MIDI to CV interfaces still are the vast majority of MIDI modules, the other way round becomes more and more interesting. With more and more complex quantizers, sequencers and other fascinating and inspiring CV modules people want to integrate existing hardware or software synths into their modular systems for playing melodies and beats that are generated by these modules.

For that task you need a CV to MIDI converter. That converts pitch and gate information that are present in form of CVs, into a stream of MIDI events and sends these over DIN or USB to the sound modules.

Such CV to MIDI converters are still rare in Euroland and many of the existing modules have severe restrictions or instabilities. One crucial problem is that most sequencers do not output gate and pitch information exactly synchronously. Another is that you need to have high quality jitter free AD converters for precisely catching your pitch CVs.

The X7 aims to be the most precise, comprehensive and flexible CV → MIDI converter available and we are confident that it indeed is. It supports an unlimited number of voices (even if your master just has eight CV inputs, more voices can be created internally with all your `sequencer`, `algoquencer`, `chord`, `arpeggio`, `minifonion` and other circuits). Also it gives you access to almost every conceivable MIDI feature. And it benefits from the master's super precise and stable AD converters.

So let's get started with the hardware. Just as with MIDI IN, you can choose between USB and TRS. But here there is a difference. The problem arises from the fact that the mapping of the MIDI DIN plug to 3.5 mm stereo jacks has been – well – fucked up by the hardware vendors. Some have chosen the tip of the plug to be the TX signal, others have found the ring to be more suitable. So two incompatible "standards" have arisen, which were later called MIDI "type A" and MIDI "type B".

While at the input there is an autosensing, at the output side this is not possible. So this time you need to get it right. For that reason on the back side of the X7 there is a small switch where you can select either type A or type B for your TRS output. If you are unsure which one is the correct one for your specific device, simply try both.

**Note: For our shipped adapters set the switch in position B!**

Using the CV → MIDI feature of the X7 is easy. Use the circuit [`midiout`](circuits/midiout.md) (see page 297) for that purpose. Here is an example for a monophonic patch with just one voice. The pitch input is read from `I1`, the gate from `I2`:

```droid
[midiout]
    pitch = I1
    gate = I2
```

Per default, X7 sends on MIDI channel 1 on TRS. You can change both with the parameters `usb` and `channel`:

```droid
[midiout]
    usb = 1
    channel = 7
    pitch = I1
    gate = I2
```

To create a polyphonic patch simply add more pitch/gate pairs:

```droid
[midiout]
    pitch1 = I1
    pitch2 = I2
    pitch3 = I3
    gate1 = I5
    gate2 = I6
    gate3 = I7
```

Of course you can use internally generated or shaped pitch information, as well. In this example the pitch input from `I1` is quantized to C minor before sending it to MIDI (see page 308 for details on the [`minifonion`](circuits/minifonion.md) circuit):

```droid
[minifonion]
    input = I1
    degree = 7
    output = _PITCH

[midiout]
    pitch = _PTICH
    gate = I2
```

You can even create a MIDI to MIDI quantizer – without any further eurorack module:

```droid
[midiin]
    pitch = _INPITCH
    gate = _GATE

[minifonion]
    input = _INPITCH
    degree = 7
    output = _OUTPITCH

[midiout]
    pitch = _OUTPITCH
    gate = _GATE
```

Of course you can also access all the CCs and other controllers, such as velocity, aftertouch, and polyphonic key pressure. Also you can send your modular clock and reset signals via MIDI. Please see page 297 for all details on the [`midiout`](circuits/midiout.md) circuit.

And by the way: as always, all parameters are CV controllable and can be changed on the fly – even things like `channel` and `usb`.

I think you can guess the flexibility of this approach!

### 8.6 MIDI through

The X7 can forward MIDI data, that are incoming via TRS or USB, to one of its two outputs (TRS / USB), while still being able to "feed in" additional events into the same output (using [`midiout`](circuits/midiout.md) (see page 297)) or processing the events (using [`midiin`](circuits/midiin.md) (see page 289)).

Use the [`midithrough`](circuits/midithrough.md) (see page 306) circuit for forwarding data from an input to an output. Here is an example:

```droid
[midithrough]
    fromusb = 1
    tousb = 0 # means TRS jack for output
```

This will forward MIDI events from the USB port to the TRS output. Note: All `midiin` and `midiout` circuits still work, so the output stream on the TRS jack will both contain the original events from MIDI-USB and the events you create with your `midiout` circuits.

`midithrough` cannot do any filter or processing on the fly. But if it would become an issue, we might add useful feature here in future.

### 8.7 Four gate outputs

The X7 has four gate outputs. These are easy to use and also not very thrilling. But useful. Each of these can output modular level triggers or gates of 5 V.

For using the gates, refer to them as `G9`, `G10`, `G11` and `G12`. Why not starting at `G1`? Well, the gates `G1` ... `G8` are reserved for the first [G8 expander](hardware.md) (see page 87), even you don't use one. Note: the gates on the X7 are only outputs, whereas the G8 can also use them as inputs.

Of course you can use the gates in combination with MIDI. Here is an example for outputting three different MIDI clocks as well as a reset signal at the gates:

```droid
[midiin]
    clock = G9   # 16th notes
    clock8 = G10 # 8th notes
    clock4 = G11 # quarter notes
    start = G12  # trigger at MIDI start message
```

### 8.8 Eight multi color LEDs

Just as with the master and the G8, you can override the functions of the eight LEDs on the X7 with your own choice of colors. Use the registers `R49` through `R56` for that purpose.

Here is an example for changing the LED color with a pot:

```droid
[p2b8]
[copy]
    input = P1.1
    output = R49
```

### 8.9 Fast patch upload via Sysex

MIDI defines a type of event that is called "Sysex", which is an abbreviation for "MIDI System Exclusive Message". These are portions of data bytes that just have a meaning to certain types of devices and are not standardized by MIDI. These messages can mean *anything* to a device. In fact one of the original ideas was to load "patches" to and from a hardware synth.

And exactly that original application is implemented by the X7: You can upload DROID patches to your master via MIDI sysex. Why would you do that, if you could simply use "USB stick mode"? Well, there are a couple of advantages:

- The upload via sysex is really super fast.
- Your DROID does not stop playing music for more than a fraction of a second.
- You don't need to touch the switch nor the button of the master. So it's a complete *remote control*.
- You don't need to do this cumbersome "eject" of the USB drive.

If you use the Forge, using Sysex works just out of the box. Put the X7 switch to the right. Let it there. At any time you can upload your current patch just by clicking the *Activate!* icon in the toolbar!

If you don't use the Forge, it's a bit more complicated to setup, since you need a software for sending patches via Sysex. But if anything goes wrong you can always fall back to USB stick mode.

#### Patch upload via sysex on Linux

The best way to setup the patch upload via sysex depends on which operating system you use. Let's start with Linux, just because it's the easiest. On any decent regular Linux installation there usually is a tool called `amidi`. It's part of the sound driver (ALSA), so it's usually already installed. `amidi` can send any MIDI commands including sysex.

Now in the Firmware ZIP-file that you find for download on your shop, you find the directory `utilities/sysex/linux` and in there the script `droidpatch`. Copy that script to `/usr/local/bin` and make sure it is executable.

Now you can upload a patch file by calling `droidpatch` with the name of your patch file. It needn't be called `droid.ini`:

```
user:~ $ droidpatch mypatch.ini
```

Of course the switch on the X7 needs be on the right (MIDI). That's it.

#### Patch upload via sysex on Mac

Now let's look at the Mac. It's basically the same procedure as on Linux just with one change. Mac does not have `amidi`. Instead you need another tool for doing MIDI on the command line. I recommend to use `sendmidi`. This has several advantages over more complex software suites:

- It is small.
- It is free.
- It is command line based and thus good for automating things.

You can get `sendmidi` here: https://github.com/gbevin/SendMIDI/releases. Choose your operating system and download and unpack it. Basically there is no installation necessary since this tool really just consists of one single file, which is called `sendmidi`. I suggest that you copy that file to `/usr/local/bin`, so that it is always ready for you to use.

Just as with Linux, in the Firmware ZIP-file you find the directory `utilities/sysex/mac` and in there the script `droidpatch`. Copy that script to `/usr/local/bin` and make sure it is executable. Put the X7 switch to the right and you can send patches with the new command `droidpatch`:

```
user:~ $ droidpatch mypatch.ini
```

One side note: `sendmidi` on Mac sometimes has a problem that every 256th byte is lost. The problem seems to lie deep in the API of Mac itself. If you run into that problem, you can try to enter a space into your patch file at the right position. Or you might consider using the Droid Forge instead of the command line.

#### Patch upload via sysex on Windows

Just as with Mac, the first step is to install `sendmidi`. You can get it here: https://github.com/gbevin/SendMIDI/releases. There is no real "installation". Just take the program `sendmidi.exe` and copy that to the directory where you keep your DROID patches. If you have none, it's a good time to create one now.

Open a terminal window, go to the directory with `cd` and try it out by simply calling that program. It should output a version number:

```
C:\Users\dmmdm\patches> sendmidi
sendmidi v1.0.15
https://github.com/gbevin/SendMIDI

Usage: sendmidi [ commands ] [ programfile ]...
```

Now connect your X7 with USB to your computer. And put the X7's switch to the right. Then check if `sendmidi` detects the X7, by adding the word `list`:

```
C:\Users\dmmdm\patches> sendmidi list
Microsoft GS Wavetable Synth
DROID X7 MIDI
```

Here it is! Now for every subsequent call to `sendmidi` add `dev x7` in order to select the X7 as output devices.

Now let's try the MIDI connection by sending a note event. This small tool is really cool. In fact you can send all sorts of MIDI events. You can even create sequences with lots of notes events and pauses in between. It's kind of really low level MIDI sequencing. So let's play a C2 at full velocity (value 127):

```
C:patches> sendmidi dev x7 on c2 127
```

If everything goes well, you should see the LED 2 on the X7 shortly flash green:

If this works, you know that the USB-MIDI connection is working and `sendmidi` is also ready. The next step is to convert your DROID patches into MIDI sysex files. To do this you just need to add a sequence of five specific bytes at the beginning, then add the patch and one final special byte at the end.

With the X7 software releases there are the files `sysexhead.txt` and `sysextail.txt` in the subdirectory `utilities/sysex/windows`. These need to be glued to the beginning and the tail of the patch in order to form a MIDI sysex file. I recommand that you copy them to your patch directory.

**Note**: For this all to work it is very important that your patch files don't contain non-ascii characters. So don't use German umlauts or any other special character that's not part of the English language (you would do that just in comments anyway).

On the command line you can use the command `copy` for gluing together the head, the patch and the tail. Use a plus sign between the file names like this:

```
C:patches> copy sysexhead.txt + yourpatch.ini
  + sysextail.txt yourpatch.syx
```

Write this in one line. This will convert `yourpatch.ini` into a new file called `yourpatch.syx`. That file can easily be sent via `sendmidi`:

```
C:patches> sendmidi dev x7 syf yourpatch.syx
```

That's all! Your master should now load the patch, show a very short restart animation and your patch is up and running.

### 8.10 Software update for the X7

Unlike the simple expanders like the P2B8 or the P10, the X7 has a rather sophisticated software. Some bugs might be found. And new feature ideas will be implemented. So the X7 has a software update procedure.

When you start the X7, it shows its current sofware version in the 2x2 LED field of the gates. The first released version is called orange-9 and is indicated by the G9 LED shining orange:

In order to make things as easy as possible for you, the software update for the X7 is done by the master. You don't need to change anything in your cabling for that. Leave the X7 attached as the first expander on the master. The firmware upgrade does *not* work reliably over a R2M/R2C bridge!

First you need the new firmware file. This is contained in the DROID software release package (ZIP file) in the subdirectory `firmwares`. It is called like `x7-orange-1012.fw`. Copy this firmware file to the SD card in the master *and rename it to* `x7.fw`. Put the SD card back into your master. The next steps are dependent on the type of master that you use.

#### X7 upgrade with the MASTER

Here are the next steps for an X7 firmware upgrade if you have an MASTER:

1. Bring the master into the maintenance mode (see page 112 for details). Long things short: this is done by a very long button press.
2. Your maintenance menu should show a *green* menu item at position 8 (if not, the SD card or the file `x7.fw` on it is missing).
3. Now press the button a couple of times until the blinking cursor is at position 8.
4. Press the button longer in order to start the update procedure.

If everything goes well, you see a kind of progress bar running through all 16 master LEDs, while the X7 does the same kind of animation with its 8 LEDs.

In case of an error, all 16 LEDs blink in one color:

- If all LEDs blink **yellow**, the firmware file is missing (which is strange, because it was there at the beginning).
- All blinking **blue** means an invalid size of the firmware file.
- And **orange** means that the file could not be read from the SD card.

After the upgrade, you need to leave the maintenance menu on your master. Do this by navigating the blinking cursor to the white LED 1 and press the button a bit longer.

#### X7 upgrade with the MASTER18

The MASTER18 does not have LEDs and also no maintenance menu. The procedure here is in fact simpler. You just need to *power off and on* your master.

If the MASTER18 detects the file `x7.fw` on the SD card while starting, it will *automatically* go into firmware upgrade mode. If you have visual access to the four diagnostic LEDs on the back of the module, you will see a pulsating white "dot" moving in circles through the four LEDs. This indicates that the master is ready for the update. Also the LED in the button blinks slowly (once a second).

As soon as the X7 is attached (which might be immediately if it was already attached when you powered up), the X7 fetches the firmware file and loads it into its internal flash memory. While this is ongoing, the button LED is lit permanently.

If everything goes well, the four diagnostic LEDs on the MASTER18 show a green "progress bar" and the eight LEDs on the X7 do the same with the same color. Then the X7 restarts and does it's usual small LED start animation and should display the new version number.

Now the button of the master flashes fast. You can now press the button to repeat the upgrade procedure or update another X7.

**Note: When you are finished with the update, you need to remove the file `x7.fw` from the master's SD card. Otherwise the master will enter the procedure again and again!**

Hint: if enter USB-Stick mode, the upgrade procedure is aborted, too, and you can easily remove the firmware file from the SD card without having the remove it from the master.

### 8.11 Some technical details

Are you interested in the technical issues of the X7? Here are some details.

The X7 uses the same micro controller (MCU) as the DROID master: The STM32F446RET6. It is running at 180 MHz and has a 32-bit hardware floating point unit. It's a very powerful processor and hard to get these days (chip crisis). But it's worth it for short latencies and high data rates.

The communication between the master and the X7 is running at a much higher bit rate than is used for the controller communication. It's using 1 MBit/sec, whereas the controller bus is running just at about 50 Kbit/sec. This is the reason why the X7 needs to be attached as first module directly to the master. This higher bitrate allows for transferring MIDI data with low latency – while the controllers are still being process at the same speed as without the X7.

When you switch to "USB stick mode" (switch to the left), the bit rate is even increased to 2 MBit/sec in order to make the access to your micro SD card as fast as possible.

The auto sensing of the MIDI TRS input is done with a bridge rectifier, four diodes, so the polarity of the input is ignored.

## 9. The MASTER18

### 9.1 Introduction

You can think of the MASTER18 as a smaller and cheaper version of the MASTER without CV inputs and LEDs, but with an integrated USB and MIDI. Also it has six additional gate jacks, two of which are inputs and four are outputs.

And it comes with two interesting bonus features:

- It can be connected to the Sinfonion as a follower of the *Harmonic Sync*.
- It has an integrated tuning device for VCOs (can measure their frequency).

The MASTER18 is a good choice if you are intend to just *create* CVs and not need to process incoming CVs. It's perfect for building sequencers and MIDI to CV converters.

### 9.2 Using the Forge

If you are working with the Droid Forge, you need to select your type of master in the menu *Rack*. There you find the item *Master module* where you can select either MASTER or MASTER18. Your selection is saved as a comment in the patch, so next time you load it, the selection is already done for you.

If you load a patch that has been built for a MASTER, it might or might not work with the MASTER18, depending on the features that have been used. In general, when you switch to a different master in the *Master module* menu, some new patch problems might be shown, others might vanish.

### 9.3 The switch

Below the button of the MASTER18 there is a **switch** with three positions. It selects the current function of the USB port:

| Position | Function |
|----------|----------|
| left | Activate USB access to the SD card |
| middle | Don't use the USB port |
| right | Activate MIDI via USB |

Beware: in the left position the master will not work as usual and does not run your patch. See below for details.

### 9.4 USB access to your SD card

The MASTER18 can give you direct access to its micro SD card via USB. This is useful for fast patch upload, access to the `DROIDERR.TXT` and `STATES1.TXT` files and more.

For this you need to bring the MASTER18 into "USB-stick mode". This mode is entered if three conditions are met:

1. The MASTER18 is connected to your Mac or PC.
2. The switch is at its left position.
3. The SD card is present in its slot.

During USB-stick mode the LED of the MASTER18's button flashes twice a second. During disk operation it is lit, in addition.

Moving back the switch to the middle is like ejecting the USB stick. So you probably want to *eject* the card on your Mac / PC first in order to avoid data loss.

After you move the switch back to the middle position, your Droid patch `droid.ini` on the card is automatically reloaded.

Notes:

- If your patch has an error (the button blinks five times in a row), put the switch back to the left, wait for the SD card window to popup and look for the file `DROIDERR.TXT`. Open it and you will see the exact reason for the error.
- If you need to re-format the card for some reason, better do this in the micro SD card reader that was shipped with your master. It's much faster that way.
- If you are working with Mac and experience that the access is slow, check out disabling Spotlight on the card. A script for that can be found on page 115.

### 9.5 MIDI

#### MIDI features overview

The MASTER18 has integrated MIDI connectivity. It can do six independend streams of MIDI data:

1. USB MIDI input
2. USB MIDI output
3. MIDI 1 input
4. MIDI 1 output
5. MIDI 2 input
6. MIDI 2 output

If that is not enough for your application, you can even add an X7 expander to get another USB port and another MIDI in/out connection.

Here are some of the key features:

- You can create powerful MIDI → CV converters.
- Unlimited polyphony (number of simultaneous notes) except that you run out of jacks.
- Flexible "MIDI through" routing while splicing in and out events.
- Comprehensive support and access to the vast majority of MIDI features such as CCs, clocks, the running state, pitch bend, all types of pedals and much more.
- Fast DROID patch upload via USB-MIDI Sysex.

And of course you benefit from DROID's own flexibility when it comes to quantization, LFOs, chord generators, switches and all that stuff.

For examples of how to use MIDI, have a look at the chapter about the [X7](hardware.md) (see page 89) and also at the circuits [`midiin`](circuits/midiin.md) (see page 289), [`midiout`](circuits/midiout.md) (see page 297) and [`midithrough`](circuits/midithrough.md) (see page 306).

#### MIDI over TRS/DIN

The TRS output ports (TRS stands for 3.5 mm tip / ring / sleeve connector) are of type B (there is no switch like in the X7, sorry). The inputs do autosensing so you can used either type A or type B. The MIDI ↔ TRS adapters shipped with your MASTER18 are of type B.

#### MIDI over USB

The MASTER18 supports MIDI over USB. Hereby it acts as a *USB device*. This does *not* mean any limitation of being an input or output device. It can be both. Even at the same time. But the actual limitation is that the MASTER18 cannot provide power to your MIDI devices and cannot be a USB host.

This means that MIDI devices that are USB devices themselves cannot be connected to the X7 via USB, even if you have a matching cable. Connect your MIDI keyboards and controllers with the TRS jack if USB doesn't work for you here. Another way is to buy a MIDI/USB adapter.

The MIDI implementation of the MASTER18 is "class compliant". This means that you do not need any driver software. Simply connect it with the shipped (or any other) USB cable to your PC and set the switch to the right. You should now see a new MIDI device, which can be selected as an input or as an output depending on what you are going to do.

Notes:

1. The USB-MIDI standard has a concept of up to 16 virtual MIDI "cables". The MASTER18 receives data on all cables and always sends on cable 0. Future software updates might make this more flexible, if there is demand.
2. You will see the MASTER18 USB device named as "DROID X7" on your Mac/PC. Don't be confused by that. That's right. It is because it's does exactly the same as the X7 and is compatible with that.
3. Turning on or off the USB connection with the switch or plugging or unplugging the cable can cause a short freeze of the master. This lasts less than a second but it may lead to audible effects in your music.

### 9.6 Sinfonion link

The ACL Sinfonion has a feature called *Harmonic Sync*. If you are as lucky as to own a Sinfonion, you can attach any number of MASTER18 as receiver of harmonic sync. Thus they share the current harmonic situation such as the root note, the scale and much more.

All you need is a normal patch cable from your Sinfonion to the `I1` input of our MASTER18. See that circuit [`sinfonionlink`](circuits/sinfonionlink.md) (see page 386) for details and examples.

### 9.7 VCO tuner

The MASTER18 has a buitin very precise frequency probe. This can measure the frequency of *basic waveforms* like square wave, triangle, sine, sawtooth and so on. This does not only give you access to the exact frequency but you can build your own tuner for exactly tuning your VCO to a reference note or just the nearest semitone.

To do this, connect a basic waveform output of your VCO to the input `I1` and use the circuit [`vcotuner`](circuits/vcotuner.md) (see page 407) to access all information about the current tuning. You find all details and examples there.

Note: Since both the Sinfonion link and the tuner use `I1`, you cannot use both in the same Droid patch currently. Future firmware versions might change this.

### 9.8 Gate inputs and outputs

The jacks labelled `I1` and `I2` are not full featured CV inputs, but gate inputs. They can just be used for clocks, gates, triggers and similar signals.

The jacks labelled `G1` through `G4` are gate outputs. They either output 0 V or 5 V – nothing in between.

### 9.9 Diagnostic LEDs

The MASTER18 has four LEDs on its back that give you some feedback of what's going on – just in case you are lost. Of course you need to unmount the module in order to see them, but it's better than nothing and on the front side there simply was not enough space left.

Here is an overview over all the different blinking patterns that these can show:

**Firmware version:** When you power up the module, the LEDs show you which firmware you are currently running. LD4 blinks a number of times in a certain color. If it blinks four times in blue color, you have BLUE-4. If the version number is greater than nine, LD3 shows the tens. So for MAGENTA-24, LD3 blinks two times in magenta and LD4 blinks four times – both starting at the same time.

**Patch reload:** If you load a new patch, LD1 through LD4 flash shortly one after another in the colors blue, green, yellow and red.

**Global patch error:** If LD1 blinks six times in a row after a patch reload, your patch has some global problem, like you exceeded the maximum amount of RAM. The color indicates the cause of the error. You find a list of all colors on page 53.

**Patch error in some line:** If LD2 blinks six times in a row after a patch reload, your patch has an error in a specific line. The number of the line is not indicated here. You find it in the file `DROIDERR.TXT` on the SD card. But the color indicates the cause of the error. You find a list of all colors on page 53.

**Factory reset:** When you hold the button in order to do a factory reset, LD1 through LD4 light up one after another in blue – like a progress bar.

**Controller update:** While the MASTER18 is waiting for the update to start (is ready to be contacted by the controller), one white LED moves along LD1, LD2, LD3, LD4 and over again to LD1 and so on. During the firmware upgrade of a controller or the X7, the four LEDs show a progress bar from LD1 to LD4 in a color depending on the controller you update.

**Firmware upgrade:** During the firmware upgrade of the MASTER18, the four LEDs show a progress bar in cyan color. If the firmware file is invalid, all LEDs flash magenta a couple of times. If the upgrade failed, the LEDs flash red. And if it's successfully completed, the LEDs flash cyan.

## 10. The R2M/R2C controller bridge

### 10.1 Introduction

The R2M/R2C is a pair of two 2 HP modules that allow you to connect a chain of controllers to your master through a standard 3.5 mm stereo cable (sometimes also called aux-cable). The usual idea is that you put all your Droid controllers into a skiff case and mount your master, X7 and G8 into another case, together with all your fancy Eurorack sound modules.

While you could do this with the typical 6-pin ribbon connector (e.g. the 80 cm version that we offer), using the R2M/R2C combination has some serious advantages:

The connection cable can be almost arbitrary long (20 m have been tested and works perfectly). Since the connection is done on the front of the modules, you can quickly disconnect your skiff for the purpose of travelling to a gig. You can use a standard 3.5 mm stereo TRS cable for the connection. These modules are not just passive connectors but contain special driver ICs that transform the electronic voltage levels, which run in the 6-pin ribbon, to something more stable and reliable that is fit for longer distances in a more hostile environment.

The controllers do not receive their power from the master but from the R2C module, which has a power connector and a voltage regulator for that purpose. Each chain of the R2C module provides the same power to its controller chain as the master does (it contains the identical voltage regulator). That means that you can connect up to 32 controllers (!) to one R2C.

Another nice thing: The R2M/R2C combination allows for two of these master / controller connections in parallel. That means that you can have two masters being attached to their individual controller chains. That does not mean, that each of the masters can access each of the controllers at the same time, however. Both master / controller connections work completely separately.

### 10.2 Setup with one master

First let's assume that your have just one master. On the back of the R2M (M stands for "master") you will find two 6-pin shrouded connectors. These are labelled 1 and 2. Connect connector number 1 with the 6-pin ribbon cable to that output of the master that is usually used for the Droid controllers.

Mount the R2M next to your master. Mount the R2C (C stands for "controller") into your skiff and use the shipped 10-pin power cable for powering it with Eurorack power (red stripe down). Otherwise the controllers won't work. The R2C has two 6-pin connectors on the back, as well. Connect the first controller of your chain to the connector labelled 1.

Now plug one of the shipped 3.5 mm stereo aux cables to jack 1 of the R2M to jack 1 of the R2C. Or use your own 3.5 mm stereo cable for that purpose.

You don't need any changes in your Droid patch.

### 10.3 X7 connected to the master

If you have an X7, connect the R2M to the X7, so that the order is master / X7 / R2M. Mount the X7 next to the master. Connect the R2M to the controller output of the X7.

### 10.4 X7 in the skiff

You can move the X7 to the "other side" of the connection by connecting the R2M directly to the master and using the X7 as the first module after the R2C. If you do this, the maximum distance that you can bridge is smaller, but 2 m should always be possible. This should be sufficient for almost any case.

### 10.5 Controllers before the R2M/C bridge

You can put the R2M/C bridge at any position in your controller chain that you like. So it's possible to have some controllers directly connected to the master. Simply wire the last of these to the R2M.

### 10.6 More than one bridge

If you have lots of controllers and put them in two skiffs, you can even use two R2M/C bridges and put a second bridge somewhere later in the chain of controllers.

### 10.7 Setup with two masters

As states above, the R2C/M is dual channel. You can create a second master / controller bridge with the same pair of R2 modules. Connect the second master to connector 2 of the R2M and its conntrollers to connector 2 of the R2C. Note: both master / controller chains are separated and cannot interact with each other.

## 11. Droid under the hood

### 11.1 How the module's state is saved

If you ask people what's the number one annoyance when using a module, most will answer this: when a module is losing its state when you power cycle your modular. That's also the number one reason for people running their system the whole night through.

Therefore the DROID – of course – will always save its state automatically. But what do I mean with "state" in the first place? It's very simple: if you have defined a `button`, DROID remembers whether it is currently *on* or *off*. If it is on *now*, so will it be after a power cycle of your system or a restart of the module (the same holds for *off*, of course).

Other circuits have states as well, for example the `algoquencer` (state of the step buttons, the accents, the pattern length), the `matrixmixer` (state of all matrix buttons), the `calibrator` (state of the calibration adaption) and so on.

Only the result of manual interaction is saved, not for example the contents of the `cvlooper` or the current phase of an `lfo`.

All these states are saved to the micro SD card into a file with the name `DROIDSTA.BIN`. This file is created with a fixed size of 128 KB when your DROID starts. All manual changes to your circuits are saved there after a short delay of about 1.5 seconds. Also when you press the button for loading a new patch, the states are saved immediately, even if the last change was less than 1.5 seconds ago.

This has the following implications:

- When no memory card is in the DROID, no states will be saved. But you can always put one there even if the module is already running for some time. It will be detected automatically and all states will be saved after a second or two.
- When you move the SD card from one DROID to another, the current circuit states will also be moved.
- If you want to erase all your settings, you can do this by starting the DROID without an SD card and inserting it later. The settings file will only be loaded right at the beginning. If it's not present, all circuits start with their default settings.

The format of the file is binary and looks chaotic. You cannot open or edit it with any software. But the format is very efficient, so the ongoing saving of states doesn't have any impact on the precise timing or performance of the DROID.

**Note:** If you forget to have the SD card inserted when you power up your DROID, it will run with default states. Inserting the SD card afterwards will **not** load the saved settings but the other way round! It will save the current states on the card. This way you **lose your original settings**. So if you have forgotten to start with the card, **power off the module, then insert the card, then power it on again**. That way you won't lose your settings.

You might ask what happens if you change a patch? The state of the circuits of the previous patch was saved to the SD card. How can that saved state be loaded into a new patch that might have a different structure?

The rule is this: DROID numbers all circuits *of the same type*, starting from 1 – according to their appearance in the patch. So there is button 1, button 2, etc. And there is buttongroup 1, buttongroup 2 and so on. When you press a button, DROID writes to the SD card something like "This is the new state of button 2.".

When that state is loaded later into a new patch, the mapping of the loaded states to the circuits uses that same numbering. So the saved state of button 2 is loaded as start state for the second button in the new patch.

From this follows that:

- If your new patch has less buttons than your previous one, some of the saved states are ignored, since the matching buttons don't exist anymore.
- If your new patch has more buttons than your previous one, the exceeding buttons start in the default state.
- If you change the order of the circuits in your patch, circuits will get the "wrong" states when you first start it.

**Note:** There is only one state file on the SD card. If you swap patches back and forth, you will always mix up your state if the patches have different structures. You might want to get a separate SD card for every patch, if swapping and not losing your state is crucial.

Sometimes you don't want a circuit to save its state. You want a fresh start every time you start your DROID. Or you missused a circuit that's meant for manual operation (e.g. `nudge`, see [page 352](circuits/nudge.md)) for some automatic changes that happen very frequent and you don't want to flood your SD card with new useless states.

All circuits that save states have an input `dontsave`. Set this to `1` to prevent the state from being saved (and loaded):

```droid
[nudge]
    dontsave = 1 # prevent loading/saving
    ...
```

### 11.2 The order of the circuits

You might ask yourself what role the *order* of the circuits plays in your patch file. Well – in most cases it doesn't matter at all, in some cases, however, it might cause very subtle timing differences in the range of a couple of hundred μs. In order to understand this, we need to have a closer look at how the DROID works:

The basic working process of your DROID is a simple *loop* that is repeating over and over again – at a speed of approximately 180 μs per cycle, which means that it is running at approximately 5.5 kHz! In each cycle of the loop the following things happen:

- The current values of all inputs, gates, buttons and pots are read in and stored in the `I`, `G`, `B` and `P` registers.
- Each circuit creates a new value for each of its outputs. That might include writing new values into `O`, `G`, `L` or `R` registers.
- The contents of the `O` and `G` registers are converted into voltages for their respective output jacks. The contents of the `L` and `R` registers are translated into brightness and color of the according LEDs.

Now let's look at two circuits that are internally wired:

```droid
[bernoulli]
    input        = G1
    distribution = P1.1
    output1      = _TRIGGER

[contour]
    trigger      = _TRIGGER
    output       = O1
```

Here an external trigger at `G1` (on the G8 expander) is being used to trigger an envelope randomly, which is then sent to `O1`. Here – because of the order of the circuits – the envelope will start *in the same loop cycle* in which the trigger is seen at `G1`.

Now let's change the order:

```droid
[contour]
    trigger      = _TRIGGER
    output       = O1

[bernoulli]
    input        = G1
    distribution = P1.1
    output1      = _TRIGGER
```

Now it is different. In the cycle in that the trigger is detected at `G1`, the envelope has already been processed. It gets its trigger through the internal wire `_TRIGGER` not before the next cycle. This introduces a short delay of up to 160 μs. This is not very long, but it can easily be avoided.

**Note:** However, when your patch contains quite a lot of circuits, the loop time gets longer. Even then, it is likely to stay below 500 μs.

### 11.3 Displaying the value of a register

In the section about finding errors in your patches we already talked about the *status dump file* (see [page 108](hardware.md)). That shows you the exact value of every single input, output, potentiometer and other register.

On the MASTER there is another way of showing a current value from within your patch, and that *live*. This can be useful, for example, if you want to spare a potentiometer and use a fixed value instead but first need to find out which value fits best. Maybe you need a simple envelope with a fixed non-zero attack value. You could try out different values by changing your patch over and over again. But that's quite annoying.

Here the experimental `X1` register helps. It's an output register. When you send a value there, all the LEDs of the front panel will show that value in a way similar to the line-error-encoding of the patch parser. Here is an example:

```droid
[p2b8]

[contour]
    attack  = P1.1
    release = P1.2
    trigger = B1.1
    output  = O1

[copy]
    input  = P1.1
    output = X1
```

Now turn the knob `P1.1` for setting some nice attack value. As soon as you remove that from its zero-position, all LEDs will move around in red and white and show the current value of `P1.1` with three digits. Input LEDs are lit white and red. White digits account for 0.1 and red digits for 0.01. The red digits at the outputs account for 0.001. Here is how the value 0.148 looks like:

The digit 9 will be displayed as 8 + 1. So here is 0.951:

A zero digit means of course that no LED is lit in the according section. Here is 0.950:

But what if digits in the input section collided? E.g. 0.550 would need the LED of input 5 to be red and white at the same time. Well, then it will blink between white and red.

The upper scheme just works for numbers in the range 0.0 … 1.0. But there are different color schemes for the non-white LED that enable showing other ranges:

- Numbers in the range -1.0 … 0.0 (excluding zero) are shown with blue LEDs.
- Integer numbers in the range 2 … 1000 are shown in orange color, with factor of 1000 applied.
- Integer numbers in the range -1000 … -2 are shown in cyan color, with factor of -1000 applied.

Example: this is the pattern for the number 148:

Once you have found a nice value, simply replace `P1.1` with that fixed value and your pot is free for something else!

### 11.4 Displaying current values

There is an easy method for getting the current value of all registers! Simply *double press* the master's button – just similar to a mouse double click. If you do this, all LEDs will flash white once. And on the SD card a file with the name `STATES1.TXT` is being created. This file will not only show you the current value of all registers but also the values of all internal patch cable (see [page 60](basics.md)).

When you do this again, a `STATES2.TXT` and so on is created. When `STATES99.TXT` is reached, it starts over again from `STATES1.TXT`. When you create the first dump file after the DROID has started, all old files from the previous run are automatically deleted.

Here is what such a file looks like:

```
DROID status

Firmware version:    blue-6
Running since:       10.385 sec
Average loop cycle: 0.043 ms
Avg controller cycle: 14.597 ms
Unique constants:    15 (60 bytes)
Unique cables:       0 (0 bytes)
Parameter values:    124 bytes
Free RAM:            107247 bytes (94.908%)
Size of patch:       163 bytes (0.254%)

Inputs:
    I1:  0.3201   I2:  0.8210   I3:  0.0000   I4:  0.0000
    I5:  0.0000   I6:  0.0000   I7:  0.0000   I8:  0.0000

Normalizations:
    N1:  0.0000   N2:  0.0000   N3:  0.0000   N4:  0.0000
    N5:  0.0000   N6:  0.0000   N7:  0.0000   N8:  0.0000

Outputs:
    O1:  1.0000   O2:  0.2000   O3:  0.3333   O4:  0.0000
    O5:  0.0000   O6:  0.0000   O7:  0.0000   O8:  0.0000

RGB-LEDs:
    R1:   0.000   R2:   0.000   R3:   0.000   R4:   0.000
    R5:   0.000   R6:   0.000   R7:   0.000   R8:   0.000
    R9:   0.000   R10:  0.000   R11:  0.000   R12:  0.000
    R13:  0.000   R14:  0.000   R15:  0.000   R16:  0.000

Controller 1 [p2b8]:
    B1.1:      0   B1.2:      0   B1.3:      0   B1.4:      0
    B1.5:      0   B1.6:      0   B1.7:      0   B1.8:      1
    L1.1:  0.000   L1.2:  0.000   L1.3:  0.000   L1.4:  0.000
    L1.5:  0.000   L1.6:  0.000   L1.7:  0.000   L1.8:  0.000
    P1.1: 0.77631  P1.2: 1.00000

Internal patch cables:
    _CLOCK:      1.00000
    _PITCH:      0.23430
    _RELEASE:    0.30000
```

### 11.5 Controller latency

As stated above, you can attach up to 16 controllers to one DROID master. These controllers are connected via a ribbon cable with six wires. Four of these wires comprise a power supply for the controllers with 5 V (except for some controllers like the M4, the E4 and the DB8E, which have their own power supply).

The remaining two wires form a digital serial connection between the modules running at 115200 bits/sec. This bit rate was chosen as a compromise between a good speed and an optimal reliability even in a "hostile" enviroment where other modules within your case might cause nasty electromagnetic disturbances.

The master sends data to the first controller, the first controller to the second and so on until the last controller sends all collected data back to the master. That's the reason why you need a "LAST" jumper on the last controller: it connects the outgoing serial wire of the last controller to the serial input of the master module.

This serial line sends approximately 7200 bytes per second. Every controller needs a different number of bytes per update and for the P2B8 it's 11 bytes. So if you have just one P2B8, you get 7200 ÷ 11 = 654 updates per second. That's roughly one update per 1.5 ms – which is pretty fast. That means that a button press is registered by the master after 1.5 ms plus some internal computation time.

If you have the maximum of 16 controllers (which would be 80 HP of controllers), things slow down a bit, of course, since now every controller get's just 1⁄16 of the data in the serial connection. In that case a button press would need about 25 ms to be registered. This is still way fast enough for the typical switching tasks that you typically do with the DROID. However, playing live drums with the buttons would not be very tight (I wouldn't suggest that anyway).

If you are interested in the actual controller cycle time of your configuration, you can either do a status dump (see right above) or – if you are lucky enough to have one of the new and shiny DB8Es (see [page 80](basics.md)) – you can look it up in the library mode in the display (right most category).

## 12. Firmware upgrade

### 12.1 Why upgrading the firmware?

DROID is an active project, new features are being added, bugs are being fixed. Also new controller modules require changes in the software of the master module. All these things are reasons why, from time to time, we release a new firmware (software) version for the DROID master.

If you want to use the new features or have the bugs fixed, you can update your firmware. You find the newest release always on our download page and also in our Discord community.

Unless most other software, DROID uses a combination of a color and a number in order to name a software version. For example the version this manual is written for is called **blue-7**.

**Note:** Some of the expanders and controllers also have firmwares that you can update. Please see [page 97](basics.md) for the X7, page ?? for the M4 and [page 74](basics.md) for the E4.

### 12.2 Checking your version on the MASTER

When your MASTER starts, you can see your current version in a short LED animation. Look at the first two rows of LEDs (which normally show the inputs) and their numbers from 1 to 8. One or more of them will light up in a color. Read these as a number and add the color and you have the firmware version. The other two lines show a rainbow animation and are not important.

This is how the version green-8 is being shown:

If two numbers light up, don't add them but read them as a number, for example this is blue-13 (not 4!):

### 12.3 Checking your version on the MASTER18

The MASTER18 does not have LEDs on the front panel, but it has four diagnostic LEDs on the back:

When you power up the module, the LEDs show you which firmware you are currently running. LD4 blinks a number of times in a certain color. If it blinks four times in blue color, you have BLUE-4. If the version number is greater than nine, LD3 shows the tens. So for MAGENTA-24, LD3 blinks two times in magenta and LD4 blinks four times – both starting at the same time.

### 12.4 Normal update procedure

Here is how you upgrade the firmware of your DROID:

1. Download the most current firmware package from the DROID's homepage at https://shop.dermannmitdermaschine.de/droid.
2. Unzip that file and go to the folder `firmwares`. There you find all firmware files for the masters, X7 and controllers.
3. Copy the firmware file for your type of master to your micro SD card:
   - For a MASTER the file is called like `droid-blue-4.fw`. Rename it to `droid.fw`.
   - For a MASTER18 the file is called like `master18-blue-4.fw`. Rename it to `master18.fw`.
4. Insert the micro SD card into your master and press the button, or power your master on while the SD card is inserted.

When the master starts, it checks for a firmware file on the SD card and automatically updates itself – if that firmware is different from the one it's currently running.

When the update is running, the 16 LEDs on the front of you MASTER, or the 4 LEDs on the back of your MASTER18 show a "progess bar" in dark cyan color. If everything goes well then at the end all LEDs flash a couple of times and the master starts into normal mode.

Here are some things that could possibly go wrong:

#### Missing firmware file

If you have not copied the file `droid.fw` or missspelled it or it cannot be found for some other reason like a defunct SD card then simply nothing happens. The master starts like usual.

#### Invalid firmware file

A magenta blink code means that your firmware file `droid.fw` has the wrong size. This usually has one of two reasons:

- You copied to wrong file to `droid.fw` or `master18.fw`.
- You try to update to a **blue** version on a MASTER that currently has a **green** version. If you want to switch to blue, you need one extra step. Please see on the next page in the section *Upgrade from green to blue* for details.

#### Fail to program

If there is some error when programming the file into your masters's memory, all LEDs blink dark red. Retry downloading and upgrading the firmware again!

#### Firmware already up-to-date

If the firmware in the file `droid.fw` or `master18.fw` already has been flashed successfully in a previous update, nothing happens. The master automatically detects this and skips the update. So it is save to leave the SD card with `droid.fw` in the SD card slot.

### 12.5 Upgrade a MASTER from green to blue

After the firmware version **green-8** there is a bigger change. So the next version is not green-9 but **blue-1**. The main difference is that **blue** firmwares are larger and allow for more cool circuits and other stuff in your DROID.

In order to make that possible we needed to change the firmware format. For that reason – if your DROID has a green firmware installed – you need to update your **bootloader** first. The bootloader is that part of the software that does the actual firmware upgrade. If your master came already shipped with a blue firmware, everything is fine and you can stop reading here.

With the bootloader from the green firmware you will get all LEDs flashing magenta if you want to update to **blue-2** (or any other blue firmware). So in this case you need to do the following steps:

1. Update to **green-8**. This is important since only this firmware has a menu entry for updating the bootloader.
2. Use the maintenance menu to update the bootloader. After which you are on green-8.
3. Update to **blue-2** or any other blue firmware just as described on the pages before.

Here is how step 2 works in detail. Do the following steps for this:

First make sure that you have the firmware file of **green-8** on your SD card. This is probably the case anyway if you just updated to green-8. Now press the button long in order to enter the maintenance menu (see [page 112](hardware.md) for details).

If everything goes well, LED 7 must show a new **blue** menu entry:

If the blue menu entry does **not** appear, it's for one of the following reasons:

- The file `droid.fw` does not match the firmware that is currently running (update your firmware first)
- Your bootloader is already uptodate (identical with the one in `droid.fw`).
- The file `droid.fw` is missing on the card.
- The file `droid.fw` is damaged.
- The file `droid.fw` cannot be read from the card (try reformatting the card with a FAT filesystem in that case).
- The SD card is not readable.
- No SD card is present.

Now use short button presses in order to move the blinking cursor to LED 7. There press the button long. This will start the update. A blue LED will run one cycle around, the DROID will restart and your are done. This whole thing should last just a few seconds.

**IMPORTANT: Do not switch off your DROID until the procedure is finished!!! Doing so will make it completely unusable. It has the be reprogrammed in our labs if that happens.**

## 13. Calibration, factory reset and other maintenance stuff

### 13.1 The maintenance mode of the MASTER

The MASTER has a special mode for various maintenance tasks. This mode is a bit "hidden" so that you do not enter it accidentally. You enter the maintenance mode by holding the button on the master for a couple of seconds. After 1.5 seconds of holding the button, an animation of light blue LEDs going from O8 over to I1 starts:

When the blue LEDs reach I1, continue holding the button. DROID restarts. Still hold the button. Now the animation of the blue LEDs starts in the opposite direction:

When the end is reached – this time at O8 – and you *now* release the button, the DROID enters the maintenance mode. If you let go the button before this you go back into normal operation.

In maintenance mode you will see a white "cursor" blinking at the LED for I1. Cell I3 is red, Cell I4 is magenta:

The four positions I1 … I4 represent four different menu options:

1. **WHITE (I1):** leave the maintenance mode and restart the DROID.
2. black: currently unused.
3. **RED (I3):** reset the DROID to factory mode (but keep calibration).
4. **MAGENTA (I4):** start the procedure of calibrating the voltage of the eight outputs.

A *short* press of the button moves the cursor to the next cell. Pressing three times brings you to cell 4:

A *long* press of the button selects the item the cursor is currently at. It starts an animation on the LEDs of O1 … O8 in the same color as the selected item (in this case calibration mode):

When the animation reaches O8, the item is being selected.

### 13.2 Factory reset on the MASTER

The factory reset can help in situations where – due to some software problem, maybe in a beta or testing version – the DROID is stuck and does not want to run again. The problem might be triggered by the current saved states of the circuits or by the currently loaded patch.

You do a factory reset in the maintenance menu by selecting position I3 (red).

All circuit states are erased. Also the current patch is erased from the internal flash memory of the master.

**Note:** If the patch is still on the SD card, it will immediately be reloaded after the reset, so if you want to avoid this, put either a different patch on the card or remove the card while doing the factory reset.

The calibration of the voltages of the outputs is *not* lost, when you make a factory reset!

**NOTE:** If due to some unexpected bug or other strange behaviour your MASTER does not respond to the button anymore, switch off your rack and hold the button while switching it on again. Now hold it until all 16 LEDs are violet and release it. This brings you directly to the maintenance mode even before your patch has started.

### 13.3 Factory reset on the MASTER18

The MASTER18 does not have LEDs and no maintenance menu. But you can do a factory reset as well:

Press and hold the button for at least four seconds. It starts blinking fast. Now release it. This triggers the factory reset. The MASTER18 resets after that and comes to live *without any patch loaded*. Press the button to load the patch on the SD card.

**NOTE:** If due to some unexpected bug or other strange behaviour your MASTER18 does not respond to the button anymore, switch off your rack and hold the button while switch it on. Now hold it for four seconds until it begins flashing fast. Release it. This procedure does a factory reset mode even before your patch has started. Make sure that the SD card is either removed or contains the new patch that you want to try, since it will be loaded immediately after the factory reset.

### 13.4 Calibration of the outputs of the MASTER

The MASTER comes with 8 high precision DA converters (DACs) that produce highly accurate voltages for the output jacks. These need to be calibrated in order to match their designed precision. Calibration of the DACs is done in our labs before we ship the units to you.

There is a super tiny chance that your calibration get's lost: When you switch of your rack just in that fraction of a second when you load a new patch by pressing the button and at the same time deleting the calibration backup file on your SD card! However unlikely: if your DROID does not start with its usual rainbow animation but with a white LED animation, your DACs are not calibrated and not very precise anymore. In that case do as described here.

Otherwise you probably never will need to calibrate your outputs. If you want to do so anyway, please make sure that your DROID has warmed up before you start. That gives the best precision. Calibration is easy and you just need a patch cable. As a preparation unplug all jacks before you start.

Now enter maintenance mode and select cell number 4 (magenta):

After entering the calibration mode, the top 8 LEDs are black and the bottom 8 LEDs are cyan – with the exception of input 1 blinking magenta and output 1 blinking cyan.

Now use a patch cable and connect input 1 to output 1. DROID now tries out different output voltages and measures them by means of the precision ADC of input 1. This information is being used for the exact calibration. The result of the calibration is saved to the DROID's internal flash memory.

As soon as channel 1 is calibrated the LED O1 changes to green. The cursor moves to the next channel:

Now proceed to the second pair of jacks and connect input 2 to output 2. Do this until all eight channels are green. DROID will then automatically end calibration and start normal operation.

If one of the channels will not go green in spite of having a proper connection between the relevant input and output you might have a hardware problem. Please contact us.

**Hint:** If you like you can use eight patch cables and patch all eight connections at once. Then you just have to wait for a couple of seconds until everything is calibrated.

By the way: If you are looking onto your SD card, you will find a file with the name `DROIDCAL.BIN`. This is a backup of your DAC calibration. Don't touch it. Just leave it there. If you delete it, it will automatically reappear anyway. If your DROID looses it's calibration for some reason (currently there is none I can think of…), starting the DROID with a card with this file will automatically restore the DAC calibration.

### 13.5 Calibration of the outputs of the MASTER18

Just as the MASTER, the MASTER18 has eight precision CV outputs that need to be calibrated. The calibration is done in our labs with a professional voltmeter from Rohde & Schwarz.

If for any reason you believe that you need to recalibrate your outputs, you can do this with the special circuit `outputcalibrator` (see [page 358](circuits/outputcalibrator.md)). There is an example patch in the firmware package with the name `m18calibration.ini`. It uses an E4 controller for the calibration.

The procedure is like this:

1. Select the output you which to calibrate with encoder 1.
2. Select the target voltage to calibrate with encoder 2. Turn left for 0 V and right for 5 V.
3. Check the actual voltage on the output with your voltmeter.
4. Turn encoder 3 left or right to adapt the voltage until it matches exactly 0 V or 5 V.
5. Repeat this step for all relevant outputs and target voltages.
6. Press the push button in encoder 4 to save the calibration.

**Beware: the calibration cannot be undone. It is very likely that you never need to calibrate your MASTER18. Before you do the calibration first check if your outputs are really unprecise.**

Especially, the output calibration is a **bad** idea for compensating badly tracking VCOs. For that rather use the circuit `calibrator` (see [page 164](circuits/calibrator.md)).

### 13.6 Using your own SD card

#### Formatting a micro SD card

DROID comes shipped with a micro SD card ready to use, but you can use your own card if you like. Usually when you buy a card it should work out of the box. If not, you might need to reformat it. The following filesystem types are supported:

- FAT 12
- FAT 16
- FAT 32

Exfat is *not* supported. Also the cluster size (sector size) needs to be 512 Bytes.

#### Speed up cards on Mac

The Apple Mac automatically creates several files and directories on every storage device it finds, in order to support spotlight search and a trash bin. Both of which is not needed for your DROID and *substantially* slows down the card access when you use it with the X7.

The card that comes with your master has been prepared by us in a way that avoids these special Mac features – if your master came shipped with at least version blue-1. If you create your own card, or if yours came shipped with an older firmware version, you can prepare it yourself.

This can be done by the following commands that you need to enter on the terminal while the card is inserted into your Mac. Hereby we assume that the name of your card is `Untitled`. If not, please adapt the commands to your name:

```
mdutil -i off /Volumes/Untitled
cd /Volumes/Untitled
rm -rf .{,_.}{fseventsd,Spotlight-V*,Trashes}
mkdir .fseventsd
touch .fseventsd/no_log .metadata_never_index .Trashes
cd -
```

Please double check what you are typing. Especially the `rm` command is very dangerous if you are not in the right directory or have mistyped one of the dots or curly brackets!

## 14. Hardware

### Notes

The power consumption has been measured under the assumption that there is no short circuit. If you set 10 V to an *output* at a master module and patch the to a different output at 0 V (or even -10 V), or you touch ground with the tip of the patch cable, the power consumption goes up by 10-20 mA (per output).

The consumption is the maximum under normal conditions. If you don't use all features of the module (like LEDs at full brightness, MIDI outputs, CV outputs, etc.) the power draw is less.

For the controllers that do not have their own power connection but are powered by the master, also for the G8 and X7, the power consumption displayed here is measured as the *raise* of the power consumption of the master module.

### MASTER

Doepfer A-100 compatible "Eurorack" module with 8 HP

- STM32F446 Micro controller running at 180 MHz
- 8 CV input jacks with a voltage range from -10 V to +10 V, driven by highly accurate low jitter 16 bit AD converters
- 8 CV output jacks with a voltage range from -10 V to +10 V, driven by highly accurate low jitter 16 bit DA converters
- 16 full color LEDs
- MicroSD card reader
- Button for reloading the MicroSD card
- Expansion port for up to four G8 expanders
- Expansion port for up to 16 controllers

Power consumption:

| Rail | Current |
| --- | --- |
| +12 V | 154 mA |
| -12 V | 15 mA |

### MASTER18

Doepfer A-100 compatible "Eurorack" module with 6 HP

- STM32F446 Micro controller running at 180 MHz
- 8 CV output jacks with a voltage range from -10 V to +10 V, driven by highly accurate low jitter 16 bit DA converters
- 2 gate inputs switching at 0.1 V, with option for VCO tuning and Sinfonion link
- 4 gate ouputs switching between 0 V and 5 V
- MicroSD card reader
- Button for reloading the MicroSD card
- Expansion port for up to four G8 expanders
- Expansion port for up to 16 controllers

Power consumption:

| Rail | Current |
| --- | --- |
| +12 V | 73 mA |
| -12 V | 7 mA |

### G8 Expander

Eurorack compatible expander for the DROID master, with 4 HP

- 8 tristate gate/trigger-jacks that can each be used either as an input or an output
- 8 full color LEDs

Power consumption:

| Rail | Current |
| --- | --- |
| +12 V | 41 mA |
| -12 V | 0 mA |

### X7 Expander

Expander with USB, MIDI TRS in/out, four gates, with 4 HP

- STM32F446 Micro controller running at 180 MHz
- USB-C connector supporting USB 2.0 device mode
- Four gate outputs with 0 V or 5 V
- Switch for USB mode with three positions: SD / off / MIDI
- 8 full color LEDs
- Port for connection to the master
- Expansion port for connection to the controllers

Power consumption:

| Rail | Current |
| --- | --- |
| +12 V | 94 mA |
| -12 V | 0 mA |

### P2B8 Controller

Eurorack compatible expander for the DROID master, with 5 HP

- STM32F030 Micro controller running at 48 MHz
- 2 potentiometers
- 8 buttons with LEDs

Power consumption:

| Rail | Current |
| --- | --- |
| +12 V | 12 mA |
| -12 V | 0 mA |

### P4B2 Controller

Eurorack compatible expander for the DROID master, with 5 HP

- STM32F030 Micro controller running at 48 MHz
- 4 potentiometers
- 2 buttons with LEDs

Power consumption:

| Rail | Current |
| --- | --- |
| +12 V | 11 mA |
| -12 V | 0 mA |

### B32 Controller

Eurorack compatible expander for the DROID master, with 10 HP

- STM32F030 Micro controller running at 48 MHz
- 32 buttons with LEDs

Power consumption:

| Rail | Current |
| --- | --- |
| +12 V | 24 mA |
| -12 V | 0 mA |

### P10 Controller

Eurorack compatible expander for the DROID master, with 5 HP

- STM32F030 Micro controller running at 48 MHz
- 2 large potentiometers
- 8 small potentiometers

Power consumption:

| Rail | Current |
| --- | --- |
| +12 V | 10 mA |
| -12 V | 0 mA |

### S10 Controller

Eurorack compatible expander for the DROID master, with 5 HP

- STM32F030 Micro controller running at 48 MHz
- 2 switches with 8 positions each
- 8 small switches with 3 positions each

Power consumption:

| Rail | Current |
| --- | --- |
| +12 V | 10 mA |
| -12 V | 0 mA |

### P8S8 Controller

Eurorack compatible expander for the DROID master, with 8 HP

- STM32F030 Micro controller running at 48 MHz
- 8 sliders with amber LEDs, moving range 20 mm
- 8 toggle switches with 3 positions each

Power consumption:

| Rail | Current |
| --- | --- |
| +12 V | 20 mA |
| -12 V | 0 mA |

### M4 Controller

Eurorack compatible expander for the DROID master, with 14 HP

- STM32F030 Micro controller running at 48 MHz
- 4 Alps motorized faders with a fader way of 60 mm
- 4 RGB multicolor LEDs
- 4 touch sensitive plates

Power consumption:

| Rail | Current |
| --- | --- |
| +12 V | 350 mA – 600 mA (configurable) |
| -12 V | 0 mA |

### E4 Controller

Eurorack compatible expander for the DROID master, with 6 HP

- STM32F030 Micro controller running at 48 MHz
- 4 Bourns encoders with 96 steps per rotation
- 4 integrated push buttons
- 128 RGB multicolor LEDs

Power consumption:

| Rail | Current |
| --- | --- |
| +12 V | 53 mA (typical) |
| +12 V | 220 mA (maximum with all LEDs white) |
| -12 V | 0 mA |

### DB8E Controller

Eurorack compatible expander for the DROID master, with 6 HP

- STM32F030 Micro controller running at 48 MHz
- 1 OLED display with 128 × 64 pixels
- 8 buttons with LEDs
- 1 Bourns encoder with 96 steps per rotation
- 32 RGB multicolor LEDs

Power consumption:

| Rail | Current |
| --- | --- |
| +12 V | 40 mA (typical) |
| +12 V | 103 mA (maximum with all LEDs white) |
| -12 V | 0 mA |

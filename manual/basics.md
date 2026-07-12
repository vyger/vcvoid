# DROID Manual — Basics (Chapters 1–5)

Installation, patching fundamentals, advanced patching, patch generators, and the patch-file text syntax. For hardware modules see [hardware.md](hardware.md); for the per-circuit reference see [circuits/index.md](circuits/index.md).

## 1. Installation of the master module

Installation of the MASTER:

### Controller connector

The connector for the controllers has 6 pins (two rows of three pins) and is used for connecting a chain of `B32`, `P2B8`, `P4B2`, `B32`, `P10`, `P8S8` and `M4`. Also the `X7` is connected here. An X7 must always be the first in the chain.

### Programming port

The 6 pin programming port is not mounted in a box. **Caution: Do not connect anything to this port!** It is solely for the initial programming in our labs. Later firmware upgrades are done via the Micro SD card.

### Power connector

The power connector has 10 pins (two rows of five pins). Use the shipped 10 pin ribbon cable in order to connect it with the bus board of your Eurorack case. **Important: Put the red stripe down!**

### Expansion port for G8 expanders

The connector for the G8 expanders has 8 pins (two rows of four pins). Here you can add up to four G8 expanders for an additional 8 - 32 gate inputs/outputs. Please refer to [page 87](hardware.md) for details.

> **Do not mix up the connectors!** This will destroy your electronics. Do not force in cables in the wrong orientation or with the wrong number of pins! Do not attach anything to the programming port.

### Installation of the MASTER18

The MASTER18 has the same controller connector, programming port, expansion port and power connector as the MASTER, with the following differences and additions:

- **Controller connector** — identical to the MASTER: a chain of `B32`, `P2B8`, `P4B2`, `B32`, `P10`, `P8S8` and `M4`. The `X7` must always be the first in the chain.
- **Programming port** — the same 6 pin programming port. **Caution: Do not connect anything to this port!**
- **Expansion port for G8 expanders** — 8 pins, up to four G8 expanders for an additional 8 - 32 gate inputs/outputs (see [page 87](hardware.md)).
- **Power connector** — 10 pins, connect via the shipped 10 pin ribbon cable. **Important: Put the red stripe down!**

#### Diagnostic LEDs

The MASTER18 does not have LEDs on the front panel. Instead diagnostic information is displayed with these four LEDs on the back of the module. Usually you don't need this information. If you run in trouble you can unscrew the module and get some additional information of what's going on here.

## 2. Getting started

### 2.1 DROID explained

`DROID` is a flexible system for generating and processing control voltages in your Eurorack modular system. It can do almost any CV task you can imagine, including sequencing, melody generation, quantizing, switching, mixing, working on clocks and triggers, envelopes, LFOs, random voltages and any combination of these at the same time. It also gives flexible access to MIDI.

The base of every `DROID` system is a DROID *master* module. There are two kinds to choose from:

The MASTER has:

- 8 CV *inputs* with high quality 16 bit converters
- 8 CV *outputs* with high quality 16 bit converters
- a 4 × 4 multicolor LED matrix displaying the state of the inputs and outputs.

The MASTER18 has:

- 8 CV *outputs* with high quality 16 bit converters
- 2 inputs and 4 outputs for gates and triggers
- A builtin VCO tuning device
- a USB-C connector for MIDI and fast configuration
- 2 MIDI inputs via TRS (3.5 mm jacks)
- 2 MIDI outputs via TRS (3.5 mm jacks)

Extension modules:

- You can add USB and MIDI to your MASTER by adding an `X7` expander (see [page 89](hardware.md)).
- You can add up to 32 additional gate inputs and outputs to your master by adding up to four `G8` expanders (see [page 87](hardware.md)).
- And finally you can add up to 16 controllers with potentiometer, buttons, encoders, motor faders, switches and more to your master to complete the system.

Note: In this manual whenever I write "master" in lower case I refer to both MASTER and MASTER18.

### 2.2 Creating DROID patches

To bring your `DROID` system to life, you need to create a *Droid patch* and load it to your master.

What is a Droid patch? Well, the `DROID` is like a self contained modular system for CV in a module. In order to avoid confusion with "real" modules – the building blocks in a Droid patch are called *circuits*. There are very simple circuits like a mixer for CVs. And there are also very complex circuits like an sophisticated algorithmic trigger sequencer called [`algoquencer`](circuits/algoquencer.md) (see [page 127](circuits/algoquencer.md)).

Much like real modules, the circuits have input and output jacks. These are called inputs or outputs, or sometimes also "parameters". Each of them can be set to a fixed value, wired to one of `DROID`'s physical inputs or outputs, set by a knob or button on a Droid controller or internally wired to other circuits in order to create more complex patches.

A Droid patch lists all the circuits you want to use and describes how they are connected and how the parameters are set.

Technically, a patch is a small text file with the name `droid.ini`, which is located on the micro SD card in the SD slot of the master. You can create and modify this file with any text editor you like, and the chapter *Writing Droid patches with a text editor* goes in all length through the structure of that file (see [page 50](basics.md)).

However, starting in November 2022 there is a new application for Mac and Windows called the *Droid Forge* – or simply the *Forge*. That's the new graphical tool for creating patches and makes working with the Droid super easy. The Forge is available for free download for on [https://shop.dermannmitdermaschine.de/pages/downloads](https://shop.dermannmitdermaschine.de/pages/downloads).

Working with the Forge is highly recommended. However, in this manual you will find lots of examples that refer to the *text representation* in `droid.ini`, because it's much easier for showing just small portions of a patch than a full sized screen shot of the Forge. And it is straight forward to recreate these examples in the Forge.

#### A first patch example – step by step

So let's start! First install the Droid Forge. Download it from the upper link and install it to your Windows PC or Mac. After starting it you get a window like in the screenshot above. The window is divided into three areas:

- At the top there is the *rack view*, where you see the Droid modules that you are working with
- At the bottom right is the *patch view*, where you see the circuits and their parameters
- At the bottom left is the list of *sections*. They are for dividing your patch into sections and make it easier to read.

Now let's create a first simple patch. The first step is to choose whether to use MASTER or MASTER18. Select this in the menu *Rack* in the entry *Master module*. For this example we assume the MASTER.

Then it's time to add your first circuit. From the *Edit* menu choose *New circuit…* This opens a dialog for adding a circuit to your patch:

Select the LFO circuit and click *OK*. This adds an LFO to your patch. Because the setting at the bottom left is set to *Start with typical example*, your LFO will already have a couple of inputs and outputs defined.

Input are written in blue, outputs in red. You learn about all available parameters of a circuit in its chapter here in this manual. Have a look at the [`lfo`](circuits/lfo.md) circuit on [page 267](circuits/lfo.md). For example:

- `hz` sets the speed of the LFO in cycles per second.
- `level` defines the maximum voltage level of the output
- `bipolar` changes the range from 0 V … 10 V to -10 V … 10 V, if set to 1.

The outputs provide various wave forms of the LFO.

If you want to add more inputs or outputs, choose *New parameter…* from the *Edit* menu or press the icon *Parameter* in the toolbar. And of course every action in the Forge has a keyboard shortcut, in this case ⌘ N (or Ctrl N on Windows).

Now move the cursor to the row **square**, either with the cursor keys or by clicking with the mouse. Move the cursor to the second column.

In the rack view, click on the Droid master on the first jack in the third row of jacks. That jack is called "Output 1" or simply `O1`. This inserts *Output O1* as a value for the **square** parameter. The LFO will now send a square wave to output 1 of the Droid master.

Move the cursor to the second column of the parameter **hz** and type `5` and hit the enter key.

Move the cursor to the *first* column of all other parameters and delete those rows by hitting the backspace key so that you just have two lines left. We don't need these parameters for now.

This is how it should look like when your are finished:

```droid
[lfo]
    hz = 5
    square = O1
```

Your first patch is ready!

There are two ways to load the patch to your master. The first is by manually swapping the SD card:

- Pull the memory card from your master and put it into a card reader in your Mac / PC. After a couple of seconds the toolbar icon *Save to SD* becomes active.
- Press that icon to copy your patch to the SD card. It will automatically be ejected afterwards.
- Put the SD card back to your master and press the master's button. That loads the patch and the LED for output 1 will start flashing in 5 Hz (five times a second).

The second way to deploy a patch is much more convenient, but needs an attached `DROID` X7 expander (see [page 89](hardware.md) for more details on the X7). With the X7 you can deploy the patch via MIDI sysex:

- Wire the X7 with the shipped USB-C to classic USB cable to your Mac / PC.
- Set the switch on the X7 to the *right*. After a short delay the *Activate!* icon in the Forge becomes active.
- Click *Activate!*. Your patch will immediately be loaded an become active.

> Hint: Always keep the micro SD card in the master while making music with your `DROID`. It is needed to store the current state of your patch so you don't loose your settings, sequences and so on when turning off your rack. Also when the SD card is missing there might be very tiny timing issues (in the range of some milli seconds) while the master is trying to contact the SD card and can't.

### 2.3 Working with the Forge

Before we have a deeper look at how Droid patches work, let's first have a closer look at the Forge.

#### Problems

Your patch can have *problems*. These are inconsistencies that would confuse your `DROID`, if you load it. One example is a parameter line without a value. In order to avoid such trouble, the Forge does not let you load a patch while it has problems.

As you see from the screenshot, there is a red triangle in the toolbar and also a note in the statusbar telling you that there are two problems. If you click on either of them, your cursor will jump to the next unsolved problem. Fix these and you will be able to load the patch.

#### When loading a patch does not work

As we have seen in the first section, the two toolbar icons for loading a patch are only active, when that is possible. If you encounter problems with *Save to SD*, please check:

- Make sure your micro SD card is in the card reader of your computer.
- Make sure it is an SD card that already has been used in the Droid. New and empty cards will not be accepted.
- If unsure, check with your Finder or Explorer, if the card is really accessible.

In case of a problem with *Activate!*, check the following:

- This button only works if you have an X7 expander attached to your master.
- Check the correct wiring of the X7.
- The switch of the X7 must be in the right position.
- The X7 must be connected with a USB cable to your Computer.
- USB-C to USB-C do not work! Use the cable shipped with the X7 or a similar one.
- If the icon still does not get active, try putting the X7 switch to the middle position and after a small pause right again.

#### Working with sections

In the bottom left of the Forge you see a pane with the entry *Untitled section*. Sections are a good way to organize more complex patches. Each section contains a list of circuits – and thus a part of your patch. You can move around sections with drag & drop. You can duplicate, rename and delete them and do many other practical things.

### 2.4 Using the master's inputs and outputs

#### Inputs and outputs

The MASTER has eight CV inputs and eight CV outputs, both ranging from -10 V to +10 V. The inputs are abbreviated with `I1`, `I2`, … `I8`, the outputs with `O1`, `O2`, … `O8`. The MASTER18 does not have CV inputs, but instead it has two trigger/gate inputs called `I1` and `I2` and four trigger/gate outputs called `G1` .. `G4`.

These jacks allow your Droid patch to communicate with the outside world. The abbreviations `O1` and so on are also called *registers*.

To use an output, you need to connect an output parameter of a circuit to it. There are several ways to do this:

- Click on the output jack in the image of the master while the cursor is right next to an output parameter.
- Type the output's name while the cursor is at that position, e.g. `O3`.
- Press *enter* while the cursor is next to an output. That opens a dialog where you can see all options.

For inputs it's much the same. Move the cursor into the second column, right next to the input name, and assign one of the inputs.

#### Input normalization on MASTER

Eurorack modules know the concept of *input normalization*. This means that an input gets some default signal when nothing is patched in the jack. The `DROID` supports this by offering the registers `N1` … `N8`. These behave like *outputs* that are internally connected to the normalizations of the input jacks.

When circuit send an output signal to `N1`, this signal is seen by input `I1`, as long as nothing is patched into that input. This allows you to create more flexible patches.

You might for example have an internal clock in your patch (created with an LFO circuit) that can be overridden by patching something into `I1`.

To do that, send your internal LFO clock signal to `N1`. Then let the rest of the patch use `I1` as clock input.

#### Gate in- and outputs on the MASTER18

The two inputs `I1` and `I2` on the MASTER18 can be used also as inputs just as the inputs of the MASTER. The difference is that these just know the logical levels "low" and "high". Low is when the input voltage is below 0.75 V. In the patch this is treated like `0`. If the voltage is above, it's considered high and treated as `1`.

If you send something to one of the four gate outputs `G1` … `G4`, it will output 0 V if your input signal near to 0 and 5 V otherwise.

All these six jacks are meant for tasks like clocks, triggers and gate signals.

#### Using the G8 gates expander

You can connect up to four G8 expanders to your master. Each G8 gives you eight additional gate inputs or outputs. Each jack of the G8 can be used as an input or output, depending on how you use it in your patch.

In the Forge there is one G8 being displayed in your rack view per default. If you don't have a G8 or you have more than one, you can fix that in the `View` menu. When the current patch actively uses any of the G8 jacks, the needed G8s are always being displayed. Use your G8 either by clicking on one of the jacks in its image, or press *Enter* for a guided dialog and select *G: Gate*, or simply type e.g. `G2.7` for gate 7 on the second G8 expander.

Note: The G8 cannot output continuous CV values. When used as output it either sends 0 V or 5 V. And inputs see a high signal at a voltage about 0.75 V.

Please refer to [page 87](hardware.md) for more details on the G8.

### 2.5 Numbers and voltages

#### How voltages are converted

`DROID` is a CV processor that inputs and outputs control voltages. But internally it works with just numbers, because this is much more convenient. Here is how the `DROID` operates:

1. When reading voltages from the *input jacks*, these are converted from the range -10 V to +10 V into the number range from -1 to +1.
2. All circuits operate on these numbers.
3. When sending numbers to the *output jacks*, the numbers are converted back from -1 to +1 to the voltage range -10 V to +10 V.

This means that if the `DROID` reads a voltage of 2.5 V at one of its inputs, in the `DROID` patch this will appear as `0.25`. Or if you send a value of `0.5` to one of the outputs, it will output exactly 5.0 V. This is in fact very convenient as you will see.

In your patch you can either write `2.5V` or `0.25`. Both mean the same. It's up to you which of both you prefer.

#### Voltages out of range

The `DROID`'s hardware cannot work with voltages beyond ±10 V. This is no limitation, since Eurorack has a maximum voltage range of ±12 V and barely any module reaches even 10 V at its output. Many digital modules are even limited to the range 0 V…5 V.

That means that any voltage out of that range appearing at an input is simply truncated. Send -10.8 V at an input and `DROID` will see it as -10 V. Or send the number 1.1 to an output (which would be 11 V) and it will output 10 V nevertheless.

**But**: internally – in your `DROID` patch – numbers can get arbitrarily low or high. So in intermediate steps it's absolutely no problem to work with larger numbers. Some circuits even require such numbers. E.g. in the [`minifonion`](circuits/minifonion.md) (see [page 308](circuits/minifonion.md)) you specify the root note B by saying `root = 11`. On the side of the jacks that would mean 110 V, but that's not relevant here.

For those of you wanting to dig more into the details of number processing: `DROID` works internally with 32 bit floating point values. The exponent is 8 bits. The largest number is slightly above 300000000000000000000000000000000000000 (a 3 with 38 zeroes).

The smallest number greater than zero is approximately 0.00000000000000000000000000000000000011 (that's 37 zeroes after the decimal point). The negative range is similar.

One word about the G8 expander: its outputs can only output two possible voltages: 0 V and 5 V. The rule is: any number >= 0.1 sent to one of its `G` registers will set its output to 5 V, any other number to 0 V.

### 2.6 Multiply and add, attenuation and offset

As you might have noticed, input parameters of circuits have *three* columns where you can enter values, whereas outputs just have one. These three columns are:

- **A: Input value**
- **B: multiplication / factor / attenuation**
- **C: offset**

So the value that's actually used by the input is *A × B + C*. That's much like Eurorack modules that have an additional potentiometer for CV attenuation (hence multiplication) and/or offset.

The special thing about `DROID` is: Even the attenuation and the offset can themselves be CVs (come from external sources, other circuits, etc.). So essentially every input has a small VCA and mixer included.

### 2.7 Internal connections

One important concept for building more interesting patches is adding connections between circuits. These connections are called *internal cables*.

Consider the following example: You have one LFO circuit that outputs a square wave, which should be used as a clock signal. That clock shall trigger an envelope circuit (called [`contour`](circuits/contour.md)).

Let's assume you want to create a cable from the **square** output of the LFO to the **gate** input of the envelope. To do this, move the cursor to the second column of the square output and press = (equals). This starts creating a cable. You will see an indicator in the statusbar.

Now move the cursor to the target of the cable: the parameter value of the gate input. Here press = again (or enter, if you like). This opens a small dialog for giving the cable a name. Choose a nice name that helps you understand what's going on later – for example `CLOCK`.

After hitting enter or pressing *OK*, you get a connection from the square output to the gate input. The envelope's output is wired to `O1` in this example, so you get an envelope triggered at 8 Hz at output 1.

These are the rules for internal cables:

- Every cable must be connected to *exactly one* output.
- Every cable must be connected to *at least one* input.

That means that you can use a cable as a multiple and distribute signals to several circuits. But if a cable has no input or no or more than one output connects, it counts as a problem and you cannot load the patch.

Note: There are more ways to create patch cables:

- In a cell type an underscore followed by the name of the cable.
- In a cell press enter and choose a cable in the value dialog (or type a name for a new cable)
- Hold ⌥ while clicking into another cell (Windows: Alt key). That creates a cable between the two cells.

### 2.8 Controllers

#### Adding controllers

The fun part with `DROID` is attaching one or more controller modules to your master. When the project started, there was just the P2B8 controller available, which has two potentiometers – or short pots – and eight buttons. Hence the name! Now there are alltogether six controllers that you can get for Droid. Learn more about the available controllers and how to connect them to the master on [page 63](hardware.md).

In a nutshell, when wiring the controllers please check the following things:

- Check that the small green jumper on each controller is set to *Park* (or removed). Just on the last controller it must be at *Last*.
- The X7 must always be the first in the chain.
- The cable coming from the master must go to *IN*, the cable to the next controllers is plugged into *OUT*.

Once your system is setup, it's very easy to use controllers in your patch. The first step is adding them to the rack view of the Forge. To do this double click on the background or choose *New controller* from one of the menus or use the Icon *Controller* in the sidebar. The order of the controllers from left to right in the Forge must match the order of the wiring in your rack.

Notes:

- You can rearrange controllers with drag & drop. The patch will automatically be adapted so all references to the controls still work as expected. That's an easy way to adapt a foreign patch to your rack.
- When you remove a controller the Forge offers you to remap its controls to other existing controllers.
- The master, X7 and G8s cannot be moved.
- If you don't have or don't use the G8 or X7, you can hide it from the rack view. Check the *View* menu for that.

#### Using pots

The easiest way of using a potentiometer is by moving the cursor to a cell of an input parameter and then clicking on the pot in the rack view. This will insert something like `Potentiometer P1.2` in the cell.

Here `P1.2` is the register name for the pot and it means *controller one pot two*. If you aren't a mouse guy, you also can type `P1.2` if you like (omit the word *Potentiometer*, that will appear automatically). Or you press enter in a cell to get the value selector where you find the pots under *Controls*.

A pot always represents a value from 0.0 to 1.0 depending on the pot position. Often that range is not what you need, but with the help of the columns 2 and 3 (factor and offset) you can create any custom range. Consider using pot `P1.2` for setting and LFO speed between 1 and 10 Hz. This can be done by:

```
Column 1: Potentiometer P1.2
Column 2: 9
Column 3: 1
```

In the text representation this would be:

```droid
    hz = P1.2 * 9 + 1
```

The math is easy: If the pot is totally at its left position, the register `P1.2` has the value 0.0. So 9 × 0.0 = 0.0 and thus adding one gives 1. At the right position the value of the pot is 1.0, so 9 × 1 + 1 = 10.

You can do much more complex things with potentiometers. For all of those please have a look at the circuit [`pot`](circuits/pot.md) (see [page 362](circuits/pot.md)). For example you can:

- Overlay one pot with several independent functions by using `select`
- Save different values of a pot into up to 16 presets
- Create a virtual center notch, to make it easy to select the middle position *exactly*.
- Have a pot output discrete numbers, for example `0`, `1`, … `8`, to select preset numbers, pattern lengths und much more
- Apply a non-linear slope to the output value

If you don't need any of these, just use pot directly without the `pot` circuit. That keeps your patch simpler.

Hints:

- If you right-click on a pot, button or other control in the rack view, you get a context menu.
- You can rearrange assignments of controls with drag & drop in the rack view.
- Double clicking on a control allows you to label it.

#### Using buttons

A button outputs the value `1` while pressed or `0` otherwise. It's register abbreviation is `B`, so `B3.4` is the button four on controller 3. You usage them just like pots.

The main difference is that buttons contains an LED. So if you want to make use of that, you need to *output* a value to the LED.

The button LEDs have their own registers, named `L`. So the LED in button `B3.4` is called `L3.4`. If you send a `0.0` to an LED, it will be dark. A `1.0` will make it shine at full brightness. Anything inbetween selects some intermediate brightness.

Sounds complicated, but at the end it makes sense, as you will see. And it also gives you flexibiliy.

Most times you don't like to *hold* the button all the time to make it do its work. You want it to *switch* between on and off with each press. This is done with the circuit [`button`](circuits/button.md) (see [page 154](circuits/button.md)). And that also helps you to deal with the LED.

The following example is in Droid source syntax, but it is straight forward to setup this in the Forge. Add the circuit *Button* and the two parameter lines **button** and **led**:

```droid
[button]
    button = B1.1
    led = L1.1
```

Now each press at button 1 on controller 1 will *toggle* the button. `led` is an output parameter so the LED register `L1.1` will hold the current state of the button – either `0` or `1`.

You can use that as an input to some other circuit, for example for switching on and off an LFO by setting its level to 0 or 1:

```droid
[button]
    button = B1.1
    led = L1.1

[lfo]
    hz = 3
    level = L1.1
    sine = O1
```

There are many more ways for using buttons. Please look at [page 154](circuits/button.md) for more examples. And also look at the circuit [`buttongroup`](circuits/buttongroup.md) (see [page 160](circuits/buttongroup.md)). It can group several buttons together in a convenient way.

Hint:

- If in a circuit the LED definitions do not match the buttons, a light bulb icon will appear in the circuit header. Click that to make the LEDs automatically match the buttons.

#### Switches

The S10 controller has ten switches. They have the register abbreviation `S`. The first two switches have eight positions and output the discrete numbers `0`, `1`, … `7`. The small switches just have three positions: `0`, `1` and `2`.

You can either use these switches directly in your patch or might want to try the circuit [`switch`](circuits/switch.md) (see [page 394](circuits/switch.md)), for assigning something for every switch position. Create a circuit with one input for every position and just one output.

You get more details on the S10 on [page 71](hardware.md).

#### Motor faders

The motorized faders from the M4 are always accessed via special circuits. Please refer to [page 75](hardware.md) for all details about the M4.

## 3. Advanced patching concepts

### 3.1 One knob – multiple functions

#### Introduction

What I liked about modular synthesizers from the beginning was the principle known as "one knob one function". In the 60's that was certainly not yet a principle. It was the only way to build devices. Today buttons dedicated exclusively to a specific function have been almost completely rationalized away – whether it's washing machines, cars or even doorbells of apartment blocks. Sure, the manufacturer saves money by simply installing one touchscreen instead of 50 real mechanical switches. The only thing that's unfair is that we are being told that it's progress that our car's cockpit is so "clean" that we have to navigate to the third menu level to change the seat heating.

So "one knob one function" feels like pure luxury these days! And `DROID` is built for you to indulge in such a luxury. After all with 16 B32 controllers you can connect no less than 512 buttons to one master. So you can get quite far in reserving one button for one function.

The problem, however, (and I have to admit this at this point) is that it *is* a luxury. If you spend some time in creating cool `DROID` patches, new ideas pop up like mushrooms and in no time all pots and buttons are occupied. And not everyone has the money, the time and the patience, to order new controllers all the time.

That's why `DROID` has a sophisticated system of overlaying your controls with almost as many functions as you want and switch between them, similar to menus or modes.

#### Overlaying pots

Let's start with pots. Let's assume that you have one P2B8 and want to use the upper pot to control both the attack and release of an envelope. The first step is to use the circuit [`pot`](circuits/pot.md) (see [page 362](circuits/pot.md)). It is able to create a *virtual* pot from a real one. Let's do this and start with controlling the attack:

```droid
[p2b8]

[pot]
    pot = P1.1
    output = _ATTACK

[contour]
    trigger = I1
    output = O1
    attack = _ATTACK
```

While this works, it has not really helped, yet. Still the pot has just one function. In order to map a second function on the same pot we need to do three things:

- Create a second `pot` circuit *for the same potentiometer*.
- Add a button for switching between these two functions.
- Use the `select` input in both `pot` circuits to choose which of the two functions is active.

For our example we want to use the button `B1.1` to switch between controlling attack and release. For that we create a `button` circuit, so that we can toggle the button. *On* should choose release and *off* attack.

We use the normal `output` of that circuit for selecting the release function. And the `inverted` output of the button is `0` when the button is active and `1` otherwise: just the opposite of `output`. We use that to select the other virtual pot – that for attack. Here is the complete patch:

```droid
[p2b8]

[button]
    button = B1.1
    led = L1.1
    output = _SELECT_ATTACK
    inverted = _SELECT_RELEASE

[pot]
    pot = P1.1
    select = _SELECT_ATTACK
    output = _ATTACK

[pot]
    pot = P1.1
    select = _SELECT_RELEASE
    output = _RELEASE

[contour]
    trigger = I1
    output = O1
    attack = _ATTACK
    release = _RELEASE
```

To summarize:

- For each virtual pot function that you need, create one `pot` circuit.
- Patch the outputs of these circuit to the inputs you want to control.
- Use the `select` inputs of the pots to decide which pot should be active.
- Make sure that at any time exactly one of the pot circuits is selected.

Note: As soon as you map several virtual functions to one pot, there is a difference between the *physical* position of the actual pot and the current virtual value. Nevertheless, turning the physical knob changes the virtual value. Please refer to [`pot`](circuits/pot.md) (see [page 362](circuits/pot.md)) for details.

#### Using button groups for selection

In the upper example we used a button for toggling between two states. If you want to have more than two function on a pot you need to choose a different method for selecting the "mode". One is to use a [`buttongroup`](circuits/buttongroup.md) (see [page 160](circuits/buttongroup.md)), like the following one:

```droid
[buttongroup]
    button1 = B1.1
    button2 = B1.2
    button3 = B1.3
    button4 = B1.4
    led1 = L1.1
    led2 = L1.2
    led3 = L1.3
    led4 = L1.4
```

This group acts like "radio buttons". If you press one of the four buttons, it is selected and the other three buttons are switched off. At any time, exactly one of the buttons is active.

Now we can use the `L1.1` … `L1.4` outputs of the button group for selecting four different pot functions:

```droid
[pot]
    pot = P1.1
    select = L1.1
    output = _ATTACK

[pot]
    pot = P1.1
    select = L1.2
    output = _DECAY

[pot]
    pot = P1.1
    select = L1.3
    output = _SUSTAIN

[pot]
    pot = P1.1
    select = L1.4
    output = _RELEASE
```

An alternative way is to use the `output` of the `buttongroup`. This outputs one of the numbers `0`, `1`, `2` and `3` depending on the selected button.

You can have the `pot` circuit get active on a specific number by using it's `selectat` input in addition to `select`. If you use that, you can specify a value that `select` needs to have for the circuit to be selected (This also avoids a problem with the `led` outputs of the button group, which don't work if the button group *itself* uses select, as we will see later). Look:

```droid
[buttongroup]
    button1 = B1.1
    button2 = B1.2
    button3 = B1.3
    button4 = B1.4
    led1 = L1.1
    led2 = L1.2
    led3 = L1.3
    led4 = L1.4
    output = _SELECT

[pot]
    pot = P1.1
    select = _SELECT
    selectat = 0
    output = _ATTACK

[pot]
    pot = P1.1
    select = _SELECT
    selectat = 1
    output = _DECAY

[pot]
    pot = P1.1
    select = _SELECT
    selectat = 2
    output = _SUSTAIN

[pot]
    pot = P1.1
    select = _SELECT
    selectat = 3
    output = _RELEASE
```

Here the first `pot` circuit is selected when `_SELECT` has the value `0`, and so on.

#### Selecting with switches

The S10 controller (see [page 71](hardware.md)) is perfect for selecting virtual functions. The two rotary switches have eight positions each and can directly be used for `select` in combination with `selectat`.

```droid
[s10]

[pot]
    pot = P1.1
    select = S1.1
    selectat = 0
    output = _ATTACK

[pot]
    pot = P1.1
    select = S1.1
    selectat = 1
    output = _DECAY

[pot]
    pot = P1.1
    select = S1.1
    selectat = 2
    output = _SUSTAIN

[pot]
    pot = P1.1
    select = S1.1
    selectat = 3
    output = _RELEASE
```

Note:

- In this example the switch positions 4 though 7 don't have any function.
- The small toggle switches of the S10 output 0, 1 or 2 and are useful for smaller selections.

#### Overlaying buttons

Just as pots, buttons can have multiple overlayed functions. This time you need to use the `select` input from the circuit that *controls* the buttons. The most obvious such circuit is `button`. But also `buttongroup` and even more complex circuits like [`algoquencer`](circuits/algoquencer.md) (see [page 127](circuits/algoquencer.md)), [`matrixmixer`](circuits/matrixmixer.md) (see [page 279](circuits/matrixmixer.md)) and [`nudge`](circuits/nudge.md) (see [page 352](circuits/nudge.md)).

Here is an incomplete sketch of a circuit that uses a buttongroup with three buttons to select one of three instances of an algoquencer. That way the buttons `B1.1`, `B1.2` and `B1.3` choose between three "tracks" or "instruments":

```droid
[p2b8]
[b32]

[buttongroup]
    button1 = B1.1 # select track 1
    button2 = B1.2 # select track 2
    button3 = B1.3 # select track 3
    led1 = L1.1
    led2 = L1.2
    led3 = L1.3
    output = _TRACK

[algoquencer]
    select = _TRACK
    selectat = 0 # track 1
    button1 = B2.1
    button2 = B2.2
    button3 = B2.3
    button4 = B2.4
    ...
    led1 = L2.1
    led2 = L2.2
    led3 = L2.3
    led4 = L2.4
    ...
    trigger = O1

[algoquencer]
    select = _TRACK
    selectat = 1 # track 2
    button1 = B2.1
    button2 = B2.2
    button3 = B2.3
    button4 = B2.4
    ...
    led1 = L2.1
    led2 = L2.2
    led3 = L2.3
    led4 = L2.4
    ...
    trigger = O2

[algoquencer]
    select = _TRACK
    selectat = 2 # track 3
    # and so on...
```

Notes:

- The three `algoquencer` circuits are mapped to the same buttons but at any time just one them uses them and displays its state at the LEDs of these buttons.
- Since the `buttongroup` outputs the values `0`, `1` and `2`, the first track (aka "Track 1") is selected by `0`, not by `1`.

**Important:** CV inputs of `algoquencer` like `activity` are *not* handled by the `select` input, even if you assign a pot to them. These are "dump" CV inputs that just use the value that is patched there. If you want your activity pot to be switched, as well, use additional `pot` circuits and use the `select` input at these, as discussed above.

#### Multi level menues or selections

Selections can be nested into several levels. Let's make an example: You have a top level `buttongroup` made out of the buttons `B1.1` … `B1.4` on a B32. Each button selects one of four instruments. Each such instrument is represented by one [`arpeggio`](circuits/arpeggio.md) (see [page 140](circuits/arpeggio.md)).

The second level consists of eight buttons on the B32 – the buttons `B1.5` … `B1.12` – shall select the allowed scale notes for the arpeggio, such as `select1`, `select3` and so on. So altogether you have 4 × 8 = 32 settings, but just 12 buttons.

The implementation is straightfoward if you keep in mind that you must not used the `led...` outputs of a `buttongroup` for something else than the actual LEDs, *if that group uses its select input*. Remember: the `led` outputs of an unselected circuit are inactive.

In the toplevel group of buttons this is not a problem, since it is always active. It doesn't use its `select` input:

```droid
# Select the instrument
[buttongroup]
    button1 = B1.1
    button2 = B1.2
    button3 = B1.3
    button4 = B1.4
    led1 = L1.1
    led2 = L1.2
    led3 = L1.3
    led4 = L1.4
```

The second level groups can directly use the `L1.1` … `L1.4` registers for their selection. But here we cannot use the `led` outputs for selecting the scale notes, since they will be inactive if the instrument is not selected. Instead, we can use the `buttonoutput` outputs. They always keep their value – regardless of the current selection.

```droid
# Scale notes of instrument 1
[buttongroup]
    minactive = 1
    maxactive = 8
    select = L1.1 # first instrument
    button1 = B1.5
    button2 = B1.6
    button3 = B1.7
    button4 = B1.8
    button5 = B1.9
    button6 = B1.10
    button7 = B1.11
    button8 = B1.12
    led1 = L1.5
    led2 = L1.6
    led3 = L1.7
    led4 = L1.8
    led5 = L1.9
    led6 = L1.10
    led7 = L1.11
    led8 = L1.12
    buttonoutput1 = _SEL_INST1_1
    buttonoutput2 = _SEL_INST1_3
    buttonoutput3 = _SEL_INST1_5
    buttonoutput4 = _SEL_INST1_7
    buttonoutput5 = _SEL_INST1_9
    buttonoutput6 = _SEL_INST1_11
    buttonoutput7 = _SEL_INST1_13
    buttonoutput8 = _SEL_INST1_FILL
```

In the [`arpeggio`](circuits/arpeggio.md) (see [page 140](circuits/arpeggio.md)) circuit of instrument 1 you can now wire the selection cables to the corresponding inputs:

```droid
# Arpeggiator 1
[arpeggio]
    select1 = _SEL_INST1_1
    select3 = _SEL_INST1_3
    select5 = _SEL_INST1_5
    select7 = _SEL_INST1_7
    select9 = _SEL_INST1_9
    select11 = _SEL_INST1_11
    select13 = _SEL_INST1_13
    selectfill1 = _SEL_INST1_FILL
    selectfill2 = _SEL_INST1_FILL
    selectfill3 = _SEL_INST1_FILL
    selectfill4 = _SEL_INST1_FILL
    selectfill5 = _SEL_INST1_FILL
    ... # further stuff here
```

Notes:

- This example shows how you can use one `buttongroup` with eight buttons and `maxactive = 8` as a elegant replacement for eight individual `button` circuits.
- Other use cases might prefer the `output` of the button group instead of the `buttonoutput` outputs.

#### Dealing with unused buttons

You might have situations where some of the buttons are *not selected at all*. With this I mean that none of the selected circuits use them. `DROID` doesn't touch the LEDs in these buttons and they keep their last state. This can be confusing and you probably will want to switch LEDs in unused buttons off.

You can do this with the `select` (see [page 381](circuits/select.md)). It is similar to `copy`, but only copies if it is selected.

```droid
[select]
    select = _SOME_SELECT
    input = 0
    output = L2.5
```

The upper example switches off the LED `L2.5` whenever `_SOME_SELECT` is not zero.

#### Overlaying switches of the S10

People keep asking how they can put multiple functions on the rotary or toggle switches of the S10. I must admit that I haven't found a good way to do this. The LED in a button can be switched as the function switches. In a pot I always can detect some movement. But how would you deal with the fact that the current position of a mechanical switch does not match it's "logical" position. OK, you toggle a switch back and forth after switching the mode, in order to show that you want to changd its value. But that's not really fun to do.

So right now, the S10 is for the true believers in the "one switch one function" principle.

#### Overlaying faders of the M4

The motor faders in the M4 are *meant* to be overloaded with multiple functions. It's really what makes them stand out against all other input devices. Unlike pots they can correctly show their current value physically. And they even can behave as switches with discrete position if needed.

Using the faders of the M4 is done by dedicated circuits. Please refer to the chapter about the M4 for details (see [page 75](hardware.md)).

### 3.2 Presets

#### Introduction

If you look carefully through the description of all circuits, you will find some that have a `preset` input. Among these are [`algoquencer`](circuits/algoquencer.md) (see [page 127](circuits/algoquencer.md)), [`button`](circuits/button.md) (see [page 154](circuits/button.md)), [`buttongroup`](circuits/buttongroup.md) (see [page 160](circuits/buttongroup.md)), [`calibrator`](circuits/calibrator.md) (see [page 164](circuits/calibrator.md)), [`encoder`](circuits/encoder.md) (see [page 213](circuits/encoder.md)), [`encoderbank`](circuits/encoderbank.md) (see [page 208](circuits/encoderbank.md)), [`matrixmixer`](circuits/matrixmixer.md) (see [page 223](circuits/matrixmixer.md)), [`faderbank`](circuits/faderbank.md) (see [page 247](circuits/faderbank.md)), [`fadermatrix`](circuits/fadermatrix.md) (see [page 250](circuits/fadermatrix.md)), [`motoquencer`](circuits/motoquencer.md) (see [page 315](circuits/motoquencer.md)), [`motorfader`](circuits/motorfader.md) (see [page 343](circuits/motorfader.md)), [`notebuttons`](circuits/notebuttons.md) (see [page 349](circuits/notebuttons.md)), [`nudge`](circuits/nudge.md) (see [page 352](circuits/nudge.md)) and [`pot`](circuits/pot.md) (see [page 362](circuits/pot.md)). All these circuits have in common that they have some internal "state" that can be changed by user interaction. For example in `algoquencer` this state comprises the current trigger pattern that you've entered with the buttons.

A preset is one "memory slot" where you can load or save the circuit's state. This is done with the inputs `preset`, `loadpreset` and `savepreset`. When you load another preset, the circuit immediately switches to a different state. This does *not* mean that it does a reset of the current running state: For example the `algoquencer` does not jump to the first step when you load a preset.

For internal reasons the total memory that a circuit can use for its state is limited. Therefore, each of the upper circuit provides a different number of presets. For example the `algoquencer` has 16 presets whereas `motoquencer` has only 4. Hereby the currently active state does *not* count as a preset, so `motoquencer` has *five* times the memory for storing its state: the currently active one plus the four presets. All these five states are automatically saved to your SD card whenever there is a change.

#### Switching presets with a button press

Switching between the presets can be done in two ways: in *triggered mode* and in *immediate* mode. Let's start with the triggered mode. Here you need to use all three mentioned inputs:

- The input `preset` tells the circuit which of the presets to load or save. The first preset has the number `0`, the second is `1` and so on.
- A trigger to `loadpreset` loads a preset into the circuit.
- A trigger to `savepreset` saves the current state of the circuit into a preset.

Typically you would use a [`buttongroup`](circuits/buttongroup.md) (see [page 160](circuits/buttongroup.md)) to specify the preset number. If you have a S10 controller, it's straight forward to use one of the rotary switches for the preset number. But you can also turn a normal pot into a rotary switch by using the circuit [`pot`](circuits/pot.md) (see [page 362](circuits/pot.md)) and set `discrete` to the total number of different presets that you want to use.

Here is an example of switching presets in an `algoquencer` using a pot. We use the full 16 presets. Loading is done with button `B1.1` and saving with button `B1.2`. Note: the preset numbers start from `0`, so it's a perfect match for the `discrete` function:

```droid
[p2b8]

[pot]
    pot = P1.1
    discrete = 16 # output will be 0 ... 15
    output = _PRESET

[algoquencer]
    preset = _PRESET
    loadpreset = B1.1
    savepreset = B1.2
    ...
```

Notes:

- When you load a preset, changes to the current state get lost (if you haven't saved them before).
- The current state does *not* get lost when you restart your `DROID` or switch off your modular. It is saved to the SD card along with the presets.

#### Using long presses to avoid losing data

It's not entirely unlikely that you will press the wrong button from time to time. When that's your load or save button, you might overwrite some sequence that you've carefully crafted.

It's therefore a common trick to shield the preset triggers with *long presses*. Use a [`button`](circuits/button.md) (see [page 154](circuits/button.md)) circuit for each of the two buttons and use it's `longpress` output. The `led` output is not neccessary as the button has no state. Here is the upper example with the extra safety net enabled:

```droid
[p2b8]

[pot]
    pot = P1.1
    discrete = 16 # output will be 0 ... 15
    output = _PRESET

[button]
    button = B1.1
    longpress = _LOAD_PRESET

[button]
    button = B1.2
    longpress = _SAVE_PRESET

[algoquencer]
    preset = _PRESET
    loadpreset = _LOAD_PRESET
    savepreset = _SAVE_PRESET
    ...
```

Now the loading and saving just happens when you press and hold the respective button for at least 1.5 seconds.

Hint: If you are a more experienced `DROID` geek, you could try using a [`burst`](circuits/burst.md) (see [page 152](circuits/burst.md)) circuit to create a short blinking animation in the button whenever a preset is loaded or saved (left as an exercise).

#### Immediate switching of presets

The other way of switching presets is without triggers or buttons. This is even simpler to implement. Just omit the `loadpreset` and `savepreset` inputs:

```droid
[p2b8]

[pot]
    pot = P1.1
    discrete = 16 # output will be 0 ... 15
    output = _PRESET

[algoquencer]
    preset = _PRESET
    ...
```

Here are the differences to the triggered mode:

- As soon as you turn the pot (i.e. the `preset` input changes, a new preset is loaded.
- The current preset is automatically saved.

And a subtlety: because the current preset and the current state are essentially the same, you "lose" one memory slot. With immediate switching, `motoquencer` has just the four presets and no "extra" preset in the current state.

#### Switching with triggers only

There is yet another way of switching presets. It is a combination of the other ways. Here you work with triggers, but these triggers at the same time hold the number of the preset to load or to save. This makes situations easier where you have one button per preset. Look at the following example:

```droid
[mixer]
    input1 = B1.1 * 1
    input2 = B1.2 * 2
    input2 = B1.3 * 3
    output = _LOAD_PRESET

[mixer]
    input1 = B1.4 * 1
    input2 = B1.5 * 2
    input2 = B1.6 * 3
    output = _SAVE_PRESET

[algoquencer]
    loadpreset = _LOAD_PRESET
    savepreset = _SAVE_PRESET
    preset = _PRESET
    ...
```

This means that if the trigger CV has the value `2` when it is non-zero, it loads preset number 2. This mode is automatically active, if you don't patch the `preset` input.

There is one drawback of this method: you cannot easily access preset number `0` that way, since the CV `0` is not sufficient for triggering the input. The trick is sending a value larger than `0.1` (which is the threshold for boolean "true" values) and less than `0.5` (which would be rounded to 1). So for example send a trigger with the value `0.3` to load or save preset number 0.

#### Things not stored in presets

Every now an then the question pops up why things like `activity` of the `algoquencer` are not saved in a preset. The answer is: the `activity` is not part of the internal state of the `algoquencer`. It's a *CV* input. Its value comes from the *outside*.

At first this might be counterintuitive if you map a pot to it (like `activity = P1.1`). But believe me: it's still a CV input. `algoquencer` cannot *know* that it's a pot. And if it would save that to a preset, and load it later: What should it do with the CV input? Should it be ignored in future?

You see: lot's of problems…

Still you might want to save the *pot's position* to a preset. And this can be done with a [`pot`](circuits/pot.md) (see [page 362](circuits/pot.md)) circuit, as we will see below.

#### Saving pots to presets

You might ask yourself: How can I get a preset for the position of a potentiometer, such as on the P2B8? Especially if I use it for controlling things like `activity` in an `algoquencer`?

The solution is very easy: Use [`pot`](circuits/pot.md) (see [page 362](circuits/pot.md)). It has a `preset` input. And then patch it's `output` to the input that you want to control with the pot via an internal cable:

```droid
[pot]
    pot = P1.2
    preset = _PRESET
    output = _ACTIVITY

[algoquencer]
    activity = _ACTIVITY
```

Of course you can combine that with the presets of `algoquencer` and switch the value of `activate` along with the actual sequencer pattern. Here is an example:

```droid
[p2b8]

[pot]
    pot = P1.1
    discrete = 16 # output will be 0 ... 15
    output = _PRESET

[pot]
    pot = P1.2
    preset = _PRESET
    output = _ACTIVITY

[algoquencer]
    preset = _PRESET
    activity = _ACTIVITY
    ...
```

Note: After loading a preset into a pot, its physical position does not reflect its logical value anymore (it would need a motor for that, just as the motor faders). Please look at the description of [`pot`](circuits/pot.md) (see [page 362](circuits/pot.md)) to learn how this works.

### 3.3 Tap tempo

There are a few circuits that have a `taptempo` input. Among these are [`burst`](circuits/burst.md) (see [page 152](circuits/burst.md)), [`contour`](circuits/contour.md) (see [page 183](circuits/contour.md)), [`gatetool`](circuits/gatetool.md) (see [page 263](circuits/gatetool.md)) and [`lfo`](circuits/lfo.md) (see [page 267](circuits/lfo.md)). Such an input is used to specify a time interval or a frequency. That's basically the same. For example an interval of 0.5 seconds corresponds to a frequency of 2 Hz. Sometimes that interval is then used as a gate length. The circuit [`lfo`](circuits/lfo.md) (see [page 267](circuits/lfo.md)) is an example of a circuit that uses this information as a frequency.

With `taptempo`, instead of specifying a number of seconds or milliseconds, you send a number of succeeding triggers. The time span between these triggers is used as *the* time interval.

There are two ways of using `taptempo` inputs. One way is, as the name suggests, a manual input. You can wire a button to the input and then "tap in" the time interval with a series of button presses. Here is an example with [`lfo`](circuits/lfo.md) (see [page 267](circuits/lfo.md)):

```droid
[lfo]
    taptempo = B1.1
    sine = O1
```

There are a few details that you should now when inputting a tap tempo:

- Two button presses are enough to enter a tap tempo.
- If you press three times, the two intervals between the three presses are averaged so your tempo input gets more precise.
- If you press more than three times, just the last three presses are recognized.
- If you press the button and the last press was more then four seconds ago, you start a new row of presses. So you cannot tap in an interval greater than four seconds.
- After you start your `DROID`, the `taptempo` is preset to 0.5 seconds (which corresponds to 2 Hz).

The second way of using a `taptempo` input is by patching a steady clock here. Most probably this will be your master clock. Since always the last three clock ticks ("taps") are recognized, the set interval is constantly updated to any changes in the speed of the clock. Please note:

- Speed changes in the input clock need some time to be recognized.
- When the input clock stops, the tap tempo is not set to zero or infinity, but simply keeps at the last setting.
- The `taptempo` input of the LFO does not keep the *phase* in sync. If you need that, patch the `sync` input in addition to the `taptempo` input.

## 4. Patch generators

### 4.1 Introduction

Building complex patches for `DROID` can be quite challenging, especially if
you are just at the beginning of your journey. So what people have suggested
since the beginning was a good collection of ready-to-use patches.

While this idea sounds appealing, it's actually not as easy as it seems.
Everybody has a different set of modules and controllers, and different ideas.
And creating a variant for every possible situation would vastly multiply the
number of needed patches.

As an example take a patch that creates a performance sequencer based on M4
motor fader controllers (see [page 75](hardware.md)). There are so many
possible configurations:

- How many tracks should be provided?
- Should the output be via MIDI or CV/Gate?
- What parameters per step should there be?
- Shall we use two of our four M4 controllers?
- Should it rely on a G8 expander for gate output?
- What type of master module should it use?
- … and so on.

It's obvious that creating one dedicated patch for each variant like one for
"Sequencer for three tracks with two M4s, MIDI output, no G8, using MASTER18,
extra CV for velocity" would not be feasable.

So we had to find a better solution. And here it is: the *Patch generators*. A
patch generator is a program or script that creates a ready-to-run patch based
on choices you make in a dialog in the Forge. For example in this dialog you
would select the number of tracks, the configuration of modules to use and so
on. Then you press *OK* and get a patch that you can upload to your master.
Since it's a normal patch, you can edit it before this. For example you could
rearrange the buttons by dragging and dropping them with the mouse.

As a start the Forge comes with one first patch generator: that for a
performance sequencer with motor faders.

You can even create your own patch generators. But that's a topic that will be
covered in a future version of this manual.

### 4.2 Enable the patch generators

The patch generators are not enabled by default. The reason is that additional
software needs to be installed on your box. This software is the programming
language "Python 3". It's very common but still may be missing on your system.

If you are running the Forge on Windows, you should now install Python 3, if
it's not already there. You can do this either from the Microsoft store. The
exact version (3.11, 3.12 or whatever) is not important. It just needs to be
version 3. Or you can get Python directly from its official home page. This is
at *Python downloads for Windows*.

**Important**: If you see a checkbox with the text "*Add Python to environment
variables*", make sure that you **enable** it. Otherwise the Forge won't be
able to find the Python interpreter and thinks it's not installed.

If you are not sure if you have set this checkbox, you can do it any time
later. You should find this in *System Properties / Advanced / Environment
Variables / User Variables / Path*. If you have missed the checkbox, you can
add it manually by clicking *NEW* there and browsing to your Python executable.

To enable the patch generators select the menu *File / Patch generators /
Enable patch generators.*. This will bring up the following popup:

Now click *Yes* to proceed. If you are running on Mac and Python 3 is not yet
installed, you need to install it now. You probably get the following popup:

The "python" command requires the command line developer tools. Simply confirm
by clicking on *Install* and you are done.

If everything went fine, or if Python 3 was installed anyway at the first
place, you get the following summary: "The patch generators have successfully
been enabled."

When you now enter the patch generators menu again, you new see a check mark
next to "*Enable patch generators*" and below it the list of all available
patch generators appear – including "Motor Fader Sequencer". Then you can
proceed with the next section.

### 4.3 How to use patch generators

Here is how to generate a patch with a patch generator:

1. Open the DROID Forge and go to the Menu *File → Patch generators*. Here you
   find a list of all generators your version of the Forge offers.
2. Select one of these. A dialog with options appears.
3. If you like, select one of the presets and press *Load preset* to load it.
4. Go through all tabs of the dialog and change any of the options if you like.
5. Press *OK*. This generates a new patch.
6. Make sure that the order of the controllers of the generated patch matches
   that in your rack.
7. Load the patch to your master as usual.

In the rack view all used buttons and jacks are labelled, so you see how this
patch is operated. Furthermore the generated patch might have comments in the
circuits. This makes it easier to learn how it is built.

Note: It is completely possible and OK that you edit the generated patch before
using it. You might want to change the order of the controllers, to match your
current setup of your modules. Or you might want to change the assignments of
some jacks or buttons with drag and drop. But: As soon as you generate the
patch again, your changes will be overwritten and you need to do them, again.

Hints:

- There is a menu shortcut for the patch generators (Command + Shift + G on Mac
  and Ctrl + Shift + G on Windows). This shortcut brings up the most recently
  used patch generator. So redoing the generation with different options is
  really fast.
- Some generated patches can be really complex. I suggest that you turn on all
  options for compressing patches before uploading them to the Droid module,
  otherwise the patch might exceed the maximum size of 64.000 bytes. You do
  this in the *Preferences*:

The Preferences offer the following options:

- **Compress patch before loading into master**
  - Remove empty lines (mixes up line numbers in error messages)
  - Rename patch cables to `_A`, `_B`, etc. (makes patch less readable)
- **Patch validation**
  - Do not treat unknown parameters as errors
  - Denounce deprecated circuits
- **Activation / Loading of patches**
  - Poll regularly for X7 connection
  - Poll regularly for DROID SD card

### 4.4 Motor Fader Performance Sequencer (MFPS)

#### Introduction

This patch generator creates a patch for a performance sequencer based on M4
motor fader controllers (see [page 75](hardware.md)). The sequencer aims at
creating interesting melodies for bass lines and lead voices. It is not so much
about drum sequencing, even if you could use it for that task, as well.

The Motor Fader Performance Sequencer (MFPS) excels in situations where you are
*performing*. It offers lots of features that are useful in live improvisation,
finding inspiring melodies and simply playing music. Especially the fast and
intuitive control with the M4 faders with force feedback make it stand out
amongst all other sequencers.

Here are some of the features:

- up to 8 parallel tracks
- output via CV/gate and/or MIDI
- builtin arpeggiator per track
- up to four presets for each track
- gatelength, velocity and glide per step
- pitch and gate randomization
- up to 32 steps per track
- steps can have a length of 1 - 16 clock ticks
- create even longer tracks using forms and conditional gates
- diatonic transposition within the chosen scale notes
- performance menu with toplevel control over all tracks
- everything is controlled with motor faders
- the faders give haptic feedback
- Get root, scale and transposition from a Sinfonion
- Mini arpeggiator for repeats and ratchets
- Pitch accumulator
- and much more…

To use this sequencer, you need the following modules:

- 1× MASTER or MASTER18
- 1× P2B8 or DB8E
- 1× B32
- 2× M4

You can extend the patch by adding further modules:

- up to three G8s provide more gate outputs
- an X7 adds MIDI support to your MASTER
- two additional M4s let you edit 16 steps without switching

The sequencer has one to eight *tracks*. Each track controls one external
instrument, either via CV/Gate or via MIDI. When using MIDI, you can assign
multiple tracks to the same channel and thus create polyphony.

Each track consists of a normal melody sequencer with 8, 16 or 32 steps. In
addition it has a builtin *arpeggiator*, which creates algorithmic melodies
based on many parameters. You can switch to the arpeggiator with a button.

Note: If you use the full eight tracks you need to reduce the feature set per
track, otherwise you run out of Droid memory.

#### Moto Kit and Moto Kit 2

If you read this you have most probably purchased a Moto Kit or a Moto Kit 2.
Both are sets of modules that contain exactly what you need to build this
sequencer.

The Moto Kit was offered until spring of 2025. It contains a P2B8 as the second
module and looks like this:

Moto Kit 2 is offered since 2026. Instead of a P2B8 it contains the new DB8E
controller with the little display and an encoder instead of the two pots (see
[page 80](hardware.md)).

This patch generator can generate the sequencer for both configurations – both
with a P2B8 or with a DB8E. It's super easy to upgrade your Moto Kit into a
Moto Kit 2: simply get a DB8E and install that instead of the P2B8. After
you've done this, regenerate your patch by selecting the preset *Moto Kit 2
(from 2026)*. You also need to update your MASTER18's firmware to at least the
**blue-7**. See [page 110](hardware.md) for how to do this.

#### Setting up the sequencer

Call the patch generator *Motor Fader Performance Sequencer* like described on
[page 26](basics.md). Make sure that your modules are mounted in the correct
order in your case.

Then load the preset *Moto Kit (until 2025)* if you have a P2B8 or *Moto Kit 2
(from 2026)* if you have a DB8E, press *OK*, connect your PC or Mac via USB to
the MASTER18 and hit *Activate*. This takes about 20 seconds.

Your Moto Kit comes with a sheet of stickers. We suggest you use these stickers
to label all the buttons and the two pots (if you have a P2B8) – once you are
satisfied with your sequencer configuration.

And now let's see, how this sequencer works…

#### Basic operation

The one basic feature that the sequencer always has – regardless of the
configuration you choose in the patch generator – is that of playing notes.
This means that the clock is moving a kind of "pointer" through your 8, 16 or
32 step sequence. In the normal *Note* mode every fader represents the pitch of
one step. The touch button below let's you toggle the gate of this step between
on and off.

When you move the faders you will notice that you can feel something like dents
or notches. These are simulated with the fader motors and give you haptic
feedback. Every notch represents one note of the current scale. This makes it
easier to precisely change a note without looking at some display.

`CLR` — When you learn how to play the sequencer, you might get stuck. Things
might get weird because you have changed a setting without knowing what's going
on. At any time you can do a **long press** (1.5 seconds or longer) on the
button `CLR` to reset everything to the factory defaults. Don't forget to turn
on some of the steps (touch buttons!) in order to get some notes played, after
that.

#### Bulk edit

`CTRL` — Whenever you edit some aspect of a step – either by moving a fader or
by touching the step button below the fader – you can *hold* the CTRL button
while you do this. If you do so, *all other steps after the one being modified*
will be set to the same value.

For example if you hold CTRL and move a fader while in NOTE mode, all other
steps right of this will be moved to the same pitch – even the steps that are
on another page and not currently visible.

Or if you turn on a gate of a step while holding CTRL, the gates of *all* later
steps are turned on.

The same works for all other aspects of a step (as those are shown later), like
velocity, gate length, randomization etc. Try it out! It is a powerful tool for
fast changes to the sequence as a whole and very handy for jamming one of the
parameters. Or for quickly setting up a new sequence.

If you want to change the whole sequence, use step 1 for your bulk changes. If
you want to set one value for the first 8 steps and another for the second 8,
first use bulk edit on step 1 and set the first value, then repeat for step 9
with the alternate value.

#### Selecting and muting tracks

`1` — If you configure your sequencer with more than one track, there will be
one button with a number for each track. Press this button to select the track.
All settings that are track-specific, such as the values of the sequence steps,
the track menu or other things, always refer to that track.

Some of the settings are globally and affect all tracks at once (such as the
root note and the scale). If in question, this manual will point out wether a
setting is track-specific or global.

`CTRL` + `1` — Tracks can be muted. Hold the CTRL-button and press the track
button at the same time. This will mute or unmute the track. Muting means that
the gate output of the track is suppressed. The track moves forward even if
muted, so it stays in sync with the other tracks.

#### Root, scale, other tonality things

Many of the features in the sequencer can be enabled or disabled in the patch
generator. So some of the functions described in the manual may not be part of
your specific sequencer – especially if you started with a minimum setup. If
you are missing something, return to the patch generator and you will find a
checkbox there to add it.

Regardless of your specific configuration there is always the *Tonality menu*.
By menu I mean a "layer" where the eight faders control certain things that are
not related to sequencer steps. There are several such menus in the sequencer,
as we will see later. A menu always uses the first eight faders. If you have a
setup with 4 M4s, the faders 9 to 16 don't have any function here.

`MENU` — You bring up the tonality menu by pressing the button labelled MENU in
the patch generator. In the sticker set that you get with the Moto Kit, there
is a button with a keyboard symbol for this menu.

The main task of the menu is selecting the root note and scale that you are
playing in. Most of the faders have a *global* meaning – they affect all tracks
at once. Here is the meaning of the eight faders:

```
Tonality menu
1. Root note
2. Scale
3. octave switch
4. Diatonic transposition
5. Absolute transposition
6. Tuning / compose mode
7. Glide duration (per track)
8. Note range (per track)
```

The first fader selects the root note. The fader has 17 notches that you can
feel (force feedback). These fader positions mean (from bottom to up): C, C♯,
D, … B, C. The C is duplicated at the top.

The second fader selects the musical scale. It has 12 positions which represent
the following 12 scales (from bottom to up). The more common scales have
colors, so that you can find them faster. The color is displayed in the touch
button below the fader when that scale is selected. The default scale is
natural minor (8). It is selected when you reset the sequencer to the factory
settings.

| Position | Scale |
|----------|-------|
| 12 | aug – Augmented scale (just whole tones) |
| 11 | dim – Diminished scale (whole/half tone) |
| 10 | phr – Phrygian minor scale (with ♭9) |
| 9 | hm – Harmonic minor (♭6 but ♯7) |
| 8 | min – Natural minor (aeolian) |
| 7 | dor – Dorian minor (minor with ♯13) |
| 6 | hm⁵ – Harmonic minor scale from the 5th |
| 5 | alt – Altered scale |
| 4 | sus – mixolydian with 3rd/4th swapped |
| 3 | X⁷ – Mixolydian (dominant seven chords) |
| 2 | maj – Normal major scale (ionian) |
| 1 | lyd – Lydian major scale (it has a ♯4) |

*Scales*

`NOTE` — The root note together with the scale determine the notes that you
select with the pitch faders when the sequencer is in NOTE mode. You even
reduce the musical "material" more by switching off some of the notes on the
P2B8 or DB8E.

`CHRO` — Enabling the button CHRO brings you into chromatic mode, where always
all 12 notes are in use and the root note and scale don't have any effect. More
details about root and scale are in the next chapter.

The third fader is a global octave switch with five positions. So you can go up
or down by two octaves. It affects all your tracks at once. It's neutral
position is in the middle.

Fader four is more musical. It does a *diatonic transposition*. For each
position you move it up or down, the sequenced melodies of all tracks are moved
up to the next or previous note *within the selected scale notes*.

The *absolute transposition* on fader 5 – on the other hand – simply changes
the final output pitch by semitones. That effectively also changes the root
note of the scale. The fader has 25 positions. The middle position is neutral.
So you have a range of one octave in 12 semitone steps up or down.

Hint: touching the button below fader 3, 4 or 5 snaps it back to its neutral
position.

Fader 6 has three positions. The bottom position is the normal position. In the
middle here all tracks output a C and produce steady gate rythms. This allows
you to tune your VCOs. The octave switch still works so you can tune your VCOs
in the pitch that they are played later on.

The top position of fader 8 enabled the *compose mode*. Here whenever you move a
fader in pitch mode, the new pitch is immediately played. The clock does not
forward the steps. This makes it much faster to dial in melodies. Try it out!
Note: the compose mode only works if the sequencer is running and the track is
not muted.

The remaining two faders control settings *per track*. This means that the
setting you edit here depends on the currently selected track. You can try this
by switching between tracks while the menu is open.

Fader 7 sets the length of *glides* of the current track. If you don't have
enabled glides or the track has just MIDI output, the fader is without function.

Fader 8 is also per track and selects the pitch range of the melody sequencer.
The fader has six positions, which select one, two, three, four, five or six
octaves. The position second from the bottom selects two octaves and is the
default setting.

Note: If you change this setting, your current melody in the selected track
changes. If you increase the note range, the melody will be spread out over a
larger pitch range. Decreasing the note range compresses your melody to a more
narrow range.

#### Scales and scale notes

A very important concept of the sequencer is that of scales and scale notes. As
we have seen above, there is always one scale selected – for example C minor.

Within this scale for each track you select which of the seven notes to use
separately. This is done with seven button of the P2B8 or DB8E, which are
layouted as follows:

```
ROOT  3RD
5TH   7TH
9TH   11TH
13TH  CTRL
```

`CTRL` — The button CTRL is for selecting alternate functions. We will talk
about that later. It has nothing to do with the scale but is located on the
P2B8 / DB8E because there was just this nice place left.

The other seven buttons represent the seven notes of the currently selected
scale. For example if you have selected C minor, the button *3RD* represents
the E♭. This note selection is used both for the normal sequencer and for the
arpeggiator. By switching on and off the note buttons you select which notes of
the scale are currently allowed to be played.

Hint: When you hold CTRL while pressing one of the seven scale note buttons,
all other buttons are switched off. That way you can "perform melodies" by
holding CTRL and pressing various buttons while the sequence or the arpeggiator
is running.

Don't get alarmed when your faders wiggle when you change the note selection.
If you remove a note, the number of allowed notes is reduced and there are less
positions your pitch fader can have. It automatically adapts to the nearest
allowed position. As long as you don't move your pitch fader, it remembers its
original position and moves back there as soon as you re-enable the note that it
originally set for that step.

Each track has its own note selection and when you switch the track, the seven
buttons will go on and off automatically to represent the note selection of the
new track.

`CHRO` — The button CHRO switches to a chromatic scale and allows all 12 notes
to be used – ignoring the scale and the note selection.

#### Clocking

Every sequencer needs a clock to forward the sequences from step to step. Our
sequencer has several options clocking. They have the following order or
precedence:

1. The internal clock – if it is included and running
2. External clock via CV input if that feature is enabled
3. MIDI clock from TRS port 1 (MASTER18 or X7)
4. MIDI clock from TRS port 2 (MASTER18)
5. MIDI clock from USB (MASTER18 or X7)
6. Sinfonion-Link, if this is enabled (MASTER18)

`CLK` — The internal clock is only present if you add it to your patch. This is
done in the setting *Features → Internal clock*. This adds a button lablled
CLK, which brings up the following menu:

```
Clock menu
1. Set 0, 100 or 200 BPM
2. Add 0, 10, 20 … 90 BPM
3. Add 0, 1, 2, … 9 BPM
4. Continous clock bend
5. Swing
6. Start / stop the clock
7. Extra clock divider
8. Pitch accumulator (per tr.)
```

The first three faders are notched and let you set the BPM of the internal
clock in 100s, 10s and ones. As always, the lower settings are at the bottom.
So you select 120 BPM by setting the first fader in the second notch (counting
from the bottom), the second fader in the third notch and the third fader at
the bottom. This way you get 100 + 20 + 0 BPM. The maximum speed is thus 299
BPM.

Fader 4 modifies this speed in a *continous* way from complete stop (bottom) to
exactly double speed (top). To return to *exactly* 120 BPM (or whatever your
have set), touch the button below fader four. It snaps back to its center as the
LED goes green. Red means that the speed is modified. When fader 4 is at the top
position, your maximum clock speed is 598 BPM.

Fader 5 adds a swing / shuffle feeling from none at all (bottom) to strong
(top). At is applied on external clocks, too – even if they are already shuffled.

Fader 6 has just two positions and will jump back and forth if you move it just
a bit or if you touch the button below. At the bottom position the LED is red
and the internal clock is stopped. If there is an external clock source, that is
taken instead.

Fader 7 is a clock divider for the case that you have enabled *Connectivity →
Output for clock with user defined divider*. The fader has 16 positions for the
clock divisions 1 to 16.

Fader 8 edits a setting *per track*: The maximum range of the pitch accumulator
for the current track. The accumulator is described below. If you have disabled
the internal clock in the *Features* tab, the range of the pitch accumulator is
set to 4.

#### The track menu

`TRK` — Another menu is the track menu. This is not global but the faders
control values of the current track. You switch between your tracks with the
buttons *Track 1*, *Track 2* and so on.

The track menu has the following eight settings:

```
Track menu
1. Autoreset
2. Shift steps
3. Octave switch
4. Diatonic transposition
5. Activity
6. Movement pattern
7. Even clock divisions
8. Odd clock divisions
```

In this menu there is the general rule, that *each* setting can be snapped to
its neutral position by touching the button below. This makes it fast to go
back to *normal* if you have got lost. Just swipe with your finger over all
eight touch buttons. A bright LED shows that the fader is in its neutral
position.

Fader 1 is called *Autoreset*. Autoreset is enabled by moving the fader away
from its bottom position. It sets a number of clock ticks after which the
sequence is restarted – regardless of what's going on in it. For example if you
move the fader three steps upwards, your sequence will be restarted after three
clock ticks. This might or might not be three steps – depending on the number
of repeats that you've chosen on the first steps.

*Shift steps* on fader 2 cycles all your steps by that number of positions to
the right. This shifts the melody in time and can create interesting rhythmic
effects.

The *octave switch* on fader 3 has its neutral position in the center and can
go up or down by two octaves. It is added to the global octave switch in the
tonality menu.

Fader 4 does a *diatonic transposition* of the current track within the scale
and the selected scale notes. This is a very musical feature and you need to
try it out and listen to it. This transposition is added together with the
diatonic transposition from the tonality menu.

Fader 5 selects a minimum activity level a step must have. Other steps are
silenced. This allows you to reduce a melody and make it simpler for the while.
If you don't have enabled *Activity* in the generator dialog, this fader is
unused. See below for how to set the activity of steps.

Fader 6 selects alternative *movement patterns* for your sequence. By this I
mean how the sequence moves through its 8, 16 or 32 steps. There are 10
different patterns to choose from:

| Position | Movement pattern |
|----------|------------------|
| 10 | random jump to any allowed (other) note |
| 9 | go forward by a small random number of steps |
| 8 | random single step forward or backward |
| 7 | double step forward, single step forward, double step backward, single step forward |
| 6 | double step forward, double step backward, single step forward |
| 5 | double step forward, one step backward |
| 4 | two steps forward, one step backward |
| 3 | ping pong – forth and back |
| 2 | backward |
| 1 | forward |

*Track movement patterns*

Faders 7 and 8 allows you to alter the speed in which the track is running in
reference to the master clock. Fader 7 has the following seven positions for
altering the clock speed:

| /8 | /4 | /2 | 1:1 | ×2 | ×4 | ×8 |
|----|----|----|-----|----|----|----|

Fader 8 provides odd divisions and multiplications:

| /7 | /5 | /3 | 1:1 | ×3 | ×5 | ×7 |
|----|----|----|-----|----|----|----|

If you combine fader 7 and 8 you can polyrythmic things like ¾.

#### The performance menu

The performance menu is an optional feature that gives you instant access to a
selection of faders from the track menu but for all tracks at once. You enable
it by selecting at least one option in the tab *Performance menu*.

All options you select here are put into the performance menu *for each track*.
So if you have four tracks and eight faders, it does not make sense to select
more than two options.

The colors of the LEDs below the faders match those of the same functions in
the track menu.

The available options in the *Performance menu* tab are:

- Auto reset: Force reset after that many steps:
- Shift steps: Shift sequence steps by this number:
- Octave switch:
- Diatonic transposition: move up or down the melody within the selected scale:
- Activity: play or mute steps based on their activity:
- Pattern: go to the steps in some non-linear mode:
- Set clock to /8, /4, /2, normal, ×2, ×4 or ×8:
- Set clock to /7, /5, /3, normal, ×3, ×5 or ×7:

#### Pitch and gate

`NOTE` — Now let's talk about the actual sequencer. Every sequence step has at
least a pitch and a gate – and depending on your configuration lots of other
aspects. Hit the button NOTE to start editing pitches and gates.

When you move the faders you will see that you feel notches (force feedback).
Each notch represents one of the selected scale notes. If you alter the note
selection by switching on and off intervals on you P2B8 / DB8E, the number of
notches accordingly changes and your faders might wiggle into new positions.
The maximum number of notches is 25. If you select a large pitch range in the
global menu, the notches might be turned off.

Touching the buttons below the faders toggle the gates for the steps.

#### More steps than faders

Depending on space and money, you can either use the sequencer with two or with
four M4 controllers. That means that you have either 8 or 16 faders. You set
this in the tab *Modules*.

The *Modules* tab has these settings:

- Master module: MASTER18 - master with MIDI, no CV inputs
- Controller for the steps: M4 - motor fader controller
- Number of M4 controllers: two M4 (eight steps)
- Number of G8 expanders (0 - 4): 0

Independent of this you can set the length of the tracks to 8, 16 or 32 steps.
This is done near the top of the *Configuration* tab.

The *Configuration* tab offers:

- Number of tracks (1 - 8): 4
- Maximum number of sequence steps: 16 steps
- Number of presets (0 - 4): 3
- Pitch range of sequencer faders: Two octaves

`(1|2)` — In the default configuration there are four tracks, each has 16
steps. With two M4's you can control eight steps at a time. The button *PAGE*
switches to editing steps 9 - 16 while it is lit.

If you have eight faders and 32 steps, there are four pages of eight steps each.
Cycle through them with the (1|2) button. If it is unlit, you are at page 1 with
the steps 1 - 8. Press it once to get to page 2 with the steps 9 - 16. Press it
again to go to steps 17 - 24 and one last time for steps 25 - 31. Now the button
has full brightness. Pressing once again brings you back to step 1 - 8.

`1` — Hint: A press on the track button of the already selected track does the
same as the *PAGE* button: it switches to the next / other page.

#### Copy & Paste

`COPY` — If haven't deactivated the setting *Button for copy & paste* in the
*Features* tab, on the position 24 of your B32 a button for copying steps is
added. There are four different ways this button can operate. Let's start with
the default setting, which is *Copy individual step / Double the range*.

Here the button has two functions. The first one is copying individual steps. To
do this:

1. Press and *hold* the COPY button.
2. Touch the plate below any sequencer step. This step is then copied into an
   internal clipboard.
3. Touch the plate of another step. The copied step is pasted here.
4. Touch more other steps to copy there, as well.
5. Release the step COPY button.

This will copy all settings of a step – regardless in which mode you currently
are.

If you just press and release the button without touching any of the steps
inbetween, the second mode is triggered: the doubling of the range. And this
works as follows:

1. Start with a reduced range of your sequence. For example set the *end step*
   to step 8 of 16. See below in the section *Limit the range of steps* for how
   to do this.
2. Press and release the COPY button.
3. Now two things happen:
   - Steps 1 … 8 will be copied into 9 … 16.
   - The end step is set to step 16.

So the sequence is doubled in length, but the since the contents of the second
half is a copy of the first half, there is no *audible* difference. Now you can
edit the second half to create some variation.

You even can start with a single step by setting the end to the first step.
Pressing the COPY button once doubles the length from 1 to 2. Pressing again
makes a sequence of 4 identical steps. And so on…

Let's go back to the various possible modes for the COPY button. The modes *Copy
individual steps* and *Double the range with content* should be clear now: They
activate only one of the two features we just explained. Use this if you want to
keep things simple if you don't use one of these.

The setting *Copy & paste sequence page* has been the only mode in the first
version of the MFPS and is only useful if you work with pages. Press the COPY
button to copy the contents of the current page into an internal clipboard.

`CTRL` + `COPY` — Then select another page and press CTRL + COPY to paste its
contents.

This copy & paste mechanism copies all aspects of the steps, not just those that
are currently selected.

Hint: Try this following: press COPY to copy the current page. Jam around by
changing the melody. Later you can come back to your original melody by pressing
CTRL + COPY, without using a preset for this.

#### Further step parameters

There are lots of aspects of sequencer steps that you can add with the patch
generator tab *Step parameters*:

The *Step parameters* tab offers:

- Velocity and additional gate (MIDI: velocity + CC#1): Just the velocity
- Randomize pitch:
- Randomize gate:
- Gate length + glides:
- Step length (1 - 16), skipping of steps:
- Ratchets (1 - 8):
- Activity (mute steps based on activity level):
- Set range of steps to play (start, end):

#### Pitch randomisation, pitch accumulator

`RAND` — The button labelled RAND brings you to pitch randomisation and to the
pitch accumulator. The eight positions of the fader have the following meanings:

| Position | Meaning |
|----------|---------|
| 8 | accumulator: shift up twice each turn |
| 7 | accumulator: shift up each turn |
| 6 | accumulator: shift down each turn |
| 5 | accumulator: shift down twice each turn |
| 4 | strong pitch randomization |
| 3 | medium pitch randomization |
| 2 | slight pitch randomization |
| 1 | randomization + accumulator off |

*Pitch randomization / accumulator*

The default is that the fader is at the bottom. This turns off any randomization
or accumulation.

The lower three settings above zero turn on pitch randomization. Here the pitch
of the step is randomly raised slightly, intermediatly or strongly.

The other four settings are much more interesting and enable the *pitch
accumulator*. The pitch accumulator makes melodies more interesting by altering
a note every time it is played. You have four different settings (per step): you
can shift the note up by one or two notes in the scale on each sequence
repetition. This is selected by the fader position 7 and 8. The LED below the
fader changes from cyan to green. Or you can shift the note down by one or two
notes every turn. This is selected by the fader positions 6 and 5 (red LED). The
shift is always done within the current scale and selected scale notes.

For example let's assume you are in C major (white keys on a keyboard) and the
step's note in question is set to a G. Now, if you set the pitch accumulator to
shift one note up (position 7), the first round of the sequence a G is played,
next time a A, then a B, then a C (next octave) and so on.

Now you might ask: does this go to infinity? Of course not! In fact you can set
the number of turns until the note is reset to its original value (in this case
G). This number is set with fader 8 in the clock menu and has a range from 0
(pitch accumulation turned off) up to 16 (reset the accumulator after 16 times
the sequence has been played). If you have disabled the internal clock, there is
no clock menu. The default setting is 4, which is the most natural and musically
least surprising setting.

At the end, using a pitch accumulator with the repeats set to 4 can change a
melody with just 16 steps into one with 64 steps.

#### Velocity

`VELO` — The button VELO switches to editing an additional CV value per step.
Here the faders run freely without notches. If you are using MIDI output, the
fader position sets the note velocities. In CV/gate mode, the velocity is output
at an additional CV output per track and the maximum range is 0 V…10 V. You can
patch it to wherever you like, for example to the filter cutoff or your voice.

Sometimes the range 0 V…10 V is too large. If you want to limit this without the
need of an external attenuator, you can set a different range in the patch
generator. Just set *Voltage at minimum velocity (milli volts)* to a value
between 0 (0 V) and 10000 (10 V). Set a maximum output velocity with the
parameter *Voltage at maximum velocity (milli volts)*.

If the track has MIDI output, the value 10000 translates into maximum velocity.
Beware: if you set the minimum velocity to 0, your MIDI source is probably
totally silent if the velocity fader is totally down. A velocity of 0 often has
the same meaning as "note off".

In velocity mode the touch buttons show the gates, just as in the pitch mode.

#### Gate length and glide

`GL` — The button GL selects editing a gate length per step with the faders. It
ranges from super short to almost the length of one clock cycle, if the fader is
up.

The touch buttons below select steps to have pitch gliding enabled. The length
of these glides can be set in the *tonality* menu.

Glides do not work in MIDI output.

It is a super lucky coincidence that both *gate length* and *glides* can be
appreviated with *GL* :-).

#### Gate probability

`PROB` — The button PROB introduces a probability that a step is actually
played. At the *top* position of the fader, the step is always played if the
gate is on. The fader has eight positions (notches), with the following
meanings:

| Position | Meaning | Chance |
|----------|---------|--------|
| 8 | played always | 100% |
| 7 | random chance of 50% | 50% |
| 6 | played every *even* turn | 50% |
| 5 | played every *odd* turn | 50% |
| 4 | random chance of 25% | 25% |
| 3 | played every 4th turn | 25% |
| 2 | random chance of 12% | 12% |
| 1 | played if last random was positive | – |

*Gate probabilities*

As you can see, not every setting is a simple random chance. Especially the
settings 5 and 6 are very musical. They make a step be played just every odd or
even turn of the sequence. This essentially doubles the length of the sequence.
Steps with such a setting have a green LED below the fader (instead of cyan). It
blinks in turns where the step is silenced and light steady when the gate is
played.

Note: In gate probability mode the default fader setting is at the *top*, not at
the bottom as with most others.

#### Ratchets

`RATC` — Ratchets are edited with the button RATC. They can be set from 1
(normal, fader at the bottom) to 8. They divide the clock cycle of the step into
equal time intervals in which the step is repeated. If you set ratchets to 2,
for example, you will get two notes played at double time.

Note: If you use ratchets it might be neccessary to select a short enough gate
length for the notes to become audible.

The most interesting feature about the ratchets, however, is the builtin "mini
arpeggiator" – also called "ratchet shift" or "ratchet note shift".

On the P2B8 you control it by turning the *lower* of the two pots. (while the
"normal" argpeggiator is turned off). You can select 15 different values, from
-7 to +7. The neutral position is in the middle and the mini arpeggiator is
turned off there.

If you have a classic MASTER (not MASTER18) the current value is displayed in
its 4×4 LED field in blue. The neutral position is at the LED of input 8:

On a system with a MASTER18, the current setting is displayed in the upper half
of the B32.

If you have a DB8E instead of the P2B8, turn the encoder on the DB8E *while
holding CTRL*. The current setting of the ratchet shifting is shown in the
display:

#### Repeats (step duration), tieing, skipping steps

`REP` — The button REP lets you edit the length of the steps. Each step can be 1
to 16 clock ticks long – while 1 is set with the fader at the bottom. In this
mode, the touch buttons select steps to *skip*. This is not the same as
silencing them, since skipped steps makes the sequence shorter.

While you move the fader, the LED below the fader helps you dialing in a specific
number of repeats. It uses the following color scheme: The numbers 4, 8, 12 and
16 are displayed *red*. The numbers 2, 6, 10 and 14 are displayed *yellow*. The
remaining (odd) numbers are black (LED is off).

`PAT` — If you increase the duration of a step, you might want to edit the way
in which this step is played. To do this press PAT to select the gate pattern.
The gate pattern decides how gates are played when *Repeats* is 2 or larger.
There are four gate patterns: In the first setting (fader down) just the first
repetition of the step is "played" (i.e. a gate signal sent). Setting 2 will
play one gate per repetition. Setting 3 plays one long gate. And setting 4 is
like 3 but lets the gate open when the step ends. This ties this step to the
next one. And this setting also has an effect when the note duration is just 1.

| Position | Gate pattern |
|----------|--------------|
| 4 | play a long gate and tie to the next step |
| 3 | play a long gate |
| 2 | one gate per repetition |
| 1 | play just the first gate |

*Gate patterns*

`REP` — Hint: When you hold the REP button while you change the number of
repeats of a step, the MFPS tries to keep the overall length of the sequence
constant (up to blue-6 this was the CTRL button). It does this by changing the
repeats in subsequent steps in the opposite way. The fader will move
automatically in order to keep the total number of repeats of all steps
constant. Of course this only works if there are enough repeats to "work on". If
all steps are at one repeat and you increase the repeats of step 1 from 1 to 9,
the other steps cannot reduce the number of repeats and stay where they are. So
your sequence gets longer by 8 16th notes. But if you then move the fader back
(while still holding REP), other steps will increase their repeats. Try it out!

Similar to the ratchets, there is also a "mini arpeggiator" for the repeats. If
activated, for every repetition of a step the note is shifted up to seven steps
up or down on the scale within the selected scale notes.

You have a P2B8? Then turn the upper knob of the P2B8 to set the repeat shifting,
when the normal arpeggiator is turned off. You can select 15 values, from -7 to
+7. The middle one – 0 – is neutral and switches the mini arp off.

If you are using a MASTER, its LED field show the selected value in magenta. The
middle position is that LED of input eight:

On a system with a MASTER18, the current setting is displayed in the upper half
of the B32.

If you have a DB8E instead of the P2B8, simply turn the encoder (without holding
CTRL). The display shows you the current setting of the repeat shift feature:

Note: The mini arpeggiator only works if the gate pattern is set to 2 (one gate
per repetition).

#### Activity

`ACT` — Activity is a feature that allows you to change the complexity of a
pattern with just one fader. First you assign an activity level to each step.
This is done with the button ACT.

Each step has an activity level from 0 to 7. Steps where the fader is at the top
position have the maximum activity of 7. They are *always* played. When you lower
the activity or a step, it is silenced, when the activity setting of the track is
lowered. This is done in the track menu (see above).

With the fader 5 in the track menu you set the minimum required activity a step
must have to be played. The default is the bottom position, which means 0. Since
no a step cannot have less than zero, all steps are played. When you move the
fader to say position 4, just those steps are played that have an activity of at
least 4. All other steps are silent (not skipped). They behave just like if
their gate was not enabled.

#### Limiting the range of steps

`S/E` — If you enable the feature *Set range of steps to play (start, end)*, you
will find a button labelled S/E. This allows you to restrict the part of the
sequence to be played by using touch buttons.

While you *hold* the S/E button, the cyan gate LED vanishes and instead a green
LED marks the first step to be played and a red one the last. Beware: If you
have more steps than faders, you might need to switch pages, because the start
and end step are on different pages.

Touch any of the buttons below the faders to set the new end step. All remaining
steps will be skipped and the sequence is now shorter.

Setting the start step is a bit more work. Here you need to touch *and hold* the
button for the end step and while this touch another button. This will be the
start step. If the start step is after the end step, the selected portion of the
sequence is played backwards.

`CTRL` + `S/E` — You can reset the start/end setting by holding CTRL and then
pressing S/E. This sets the start step to be the first and the end step to be the
last of the sequence.

`CLR` — Pressing the CLR button (see below) does the same but in addition resets
other settings that remove the linearity of the sequence.

By the way: Another method for making the sequence shorter is using *Autoreset*
in the track menu (see above).

#### Peace, clear and master reset

`CLR` — If you have added the feature *Clear button: reset play mode, clear
pattern, factory reset*, you get a button labelled CLR. This button has three
"escalation levels" of resetting things:

Pressing the clear button in a normal way, resets all features of the *current
track* that alter the effective duration of one sequence cycle:

- Start and end are reset to the first and last step of the sequence
- Steps tagged with "skip" are reset to normal (non-skip)
- The number of repeats (step length) is set to 1 for every step
- Autoreset is disabled (first fader in track menu)
- The shifting of steps is disabled (second fader in track menu)
- The movement pattern is set to "forward" (6th fader in track menu)

`CTRL` + `CLR` — Press the CLR button *while you hold CTRL* to reset every aspect
of the current track. All faders in the track menu go to their neutral position.
All steps are set to *gate off* and to the lowest note (you can change the
default gate to *on* in the tab *configuration*). All additional parameters of
the steps are set to their neutral position, as well.

A *long press* of the clear button, makes a factory reset of the *whole
sequencer!*. All tracks and all presets are reset. All settings are reset to
their defaults.

The long press of the CLR button is your help if you get completely lost
somewhere. There are many possible reasons why the sequencer won't play any
notes. The clock might be stopped, the clock division set to something very slow,
steps might been skipped, have a too low activity and so on. In such a situation
where you are not able to find the reason, try a factory reset.

This is also a good idea after you change the configuration of the patch in the
patch generator dialog. Enabling or disabling features might lead to saved fader
positions move to other faders that now have different meanings.

#### Manual reset to step 1

`RST` — When you play together with other musicians or with other sequencers,
you might get out of sync. The features *Button for resetting the current (Ctrl:
all) tracks to step 1* adds the button RST.

If you press RST without the CTRL-button, the current track is reset to step 1
immediately.

`CTRL` + `RST` — If you press RST with CTRL, *all* tracks are reset to step 1
immediately.

#### Transpose by root note

`TBR` — The feature *Button for transposing the melody when the root note
changes* add a button with the label TBR. This is an abbreviation for *transpose
by root note*. This button has two states: on and off and it is a setting per
track.

If it is on, any change in the root note also transposes the melody. The
reference is C. Try the following: Set the root note to C (in the tonality menu)
and compose a melody. Now change the root note to E♭.

When TBR is on, the melody will be transposed along with the root note and
played three semitones higher. That way it sounds exactly like the original
melody, just three semitones higher.

No try the same with TBR off. This time the melody stays in the same general
pitch, just with some of the notes modified by a semitone or so to match the new
scale.

Both settings are musically useful and a change of the settings can sound very
interesting – as long as you have changes in the root note as time goes by.

The TBR setting is also applied to the arpeggiator (see below).

#### The inversion

`INV` — The feature *Button for inverting the melody (switch low/high)* adds a
button with the Label INV. It is a setting per track and vertically mirrors the
melody when on. High notes get low and low notes get high.

#### Forms like AAAB, and AABB

`AAAB` — When you add the features *Button for switching the form (A / AAAB /
AABB)*, you get a button labelled AAAB. Again, this is a setting per track, but
this time the button has three states. You cycle through these by pressing it
several times.

Per default the button is off. Press it once to switch to the form AAAB. The
button is half-lit. Now your steps are divided in two halves. Say you have 16
steps. Then the first eight steps are part A and are repeated three times, before
the second eight steps are played once.

The third state (button fully lit) selects AABB. Now every half is played twice.

These forms essentially double the length of your pattern in a musically
appealing way.

If you have set a reduced range of steps to play via the S/E button, the parts A
and B are cut from that smaller range. If that is an odd number of notes, A and B
don't have the same length and funny things will happen.

#### Presets

`A` — In the tab *Configuration* you can add up to four presets to your
configuration. A preset is a storage for the current melody of a track (including
all extra attributes of the steps) together with all settings of the faders and
buttons, including the scale note selection.

When you have enabled presets, up to four buttons with the labels *Preset A*,
*Preset B*, *Preset C* and *Preset D* appear. Here is how presets work:

- A short press of a preset button does *nothing*. This is for your safety.
- A *long* press (≥ 1.5 secs) *saves* the current melody and all other track
  settings into that preset.
- A press of a preset button while CTRL is held, loads that preset.

Notes:

- Loading and saving presets affects the *current track*.
- Every track has its own independent presets.
- Loading a preset does *not* reset the track to step 1.

#### Randomize the fader positions and gates

`LUCK` — The button LUCK either randomizes your current fader settings.

`CTRL` + `LUCK` — When you press LUCK together with CTRL, your touch button
settings are randomized instead (for example gates).

This feature is enabled with the checkbox "*Lucky: Randomly change the faders
(with Ctrl: buttons)*".

As long as you hold this button, more and more faders or touch buttons will
change – at a rate of 40 changes per second. If you push the button just for a
short time, just one or a few things will change. Holding it longer completely
changes everything.

What the faders or buttons mean depends on the current mode. So if you want to
randomize the velocity, bring up the velocity mode and hold LUCK.

#### The arpeggiator

One of the most fun features of the MFPS is the builtin *Arpeggiator*.

`ARP` — If you enable this feature in the patch generator, you get a button
labelled ARP. This is for switching a track to arpeggio mode.

In arpeggio mode the pitches of the notes are not longer determined by the pitch
faders but are played by the arpeggiator. This is an algorithmic melody creator,
which can do more than you would think when you hear its name.

To get started with it, choose a track and switch on the ARP. Enable all seven
scale notes on the P2B8 / DB8E. Then set the base pitch and the pitch range of
the arpeggio to suitable values. If you use a P2B8:

1. Set the lower pot of the P2B8 to totally left. This is the base pitch of the
   melody.
2. Set the upper pot of the P2B8 in the middle. This is the pitch *range* the
   melody uses.

Note: Both pots on the P2B8 are overlayed with several functions depending on
the context. There is a separate setting for the base pitch and range of the
arpeggiator for each track. After switching to a different track the pot
probably is not in the position of the value it actually controls. As soon as
you turn the pot just a bit you get the current value either visualized in the
4×4 LED field of the MASTER or on the upper half of the B32 (if you use a
MASTER18).

On a DB8E it goes like this:

1. Turn the encoder on the DB8E to set the base of the arpeggio's pitch.
2. Turn the encoder *while holding* the CTRL-button to modify the pitch range
   through which the melody moves.

The display shows you the pitch base and range in semitones. A range of 7 does
not mean that seven notes are played but that the difference between the base
pitch and the highest pitch is not more than seven semitones.

After setting the base pitch und pitch range to suitable values, you should hear
your synth voice, or whatever is attached to the selected track, to play all
notes of the current scale upwards until it reaches some upper limit (which is
selected by the upper pot) and starts over again.

There are dozens of ways to alter that melody. Try the following:

- Alter the base pitch and the range with the two pots. Even a range of zero can
  be interesting. Here you can "play" a melody with the pitch knob.
- Try different selection of scale notes. Try to use just three notes, for
  example a normal triad (ROOT, 3RD, 5TH).

As you can see, all of these alterations changes the length of the melody and
thus creates interesting polymetric effects.

`TRK` — And there are lots of other parameters. These are in the track menu.
Press the button TRK to bring it up. When the arpeggiator is turned on, most of
the faders have different meanings. All of them only affect the arpeggiator –
not the normal mode, even if some of them they have the same name.

```
Arpeggio menu
1. Autoreset
2. Up / ping pong / down
3. Octave switch
4. Butterfly
5. Octaving pattern
6. Movement pattern
7. Drop notes from scale
8. Clocking
```

*Autoreset* is similar than in the normal mode. If enabled, after that number of
steps the arpeggio is reset to its starting point.

*Up / ping pong / down* changes the order of the movement from up to
up-and-down-again or to downwards movement.

The *Octave switch* transposes the whole melody up to two octaves up or down.
This adds up with the pitch knob.

The *Butterfly* fader has just two positions: down and up. When it is up,
butterfly mode is active. Now the order of played notes changes to first, last,
second, second last and so on.

The *Octaving pattern* has three settings. In the middle or upper position after
each note, the same note is repeated but one octave up or down.

The *Movement pattern* changes the linear movement mode through the scale to
something more complex. It has seven settings as follows:

| Position | Arpeggio movement pattern |
|----------|---------------------------|
| 7 | random jump to any allowed (other) note |
| 6 | random single step forward or backward |
| 5 | double step forward, single step forward, double step backward, single step forward |
| 4 | double step forward, double step backward, single step forward |
| 3 | double step forward, one step backward |
| 2 | two steps forward, one step backward |
| 1 | step forward through the selected notes |

*Arpeggio movement patterns*

The fader *Drop notes from the scale* enables certain patterns of leaving out
(skipping) notes on the way. It has four settings:

| Position | Drop pattern | |
|----------|--------------|--|
| 4 | Skip the 2nd and 3rd note | ❶②③❹❺❻ |
| 3 | Skip every third selected note | ❶❷③❹❺❻ |
| 2 | Skip every other selected note | ❶②❸④❺⑥ |
| 1 | Do not skip any notes | ❶❷❸❹❺❻ |

*Arpeggio drop patterns*

The last fader in the arpeggio menu selects the clock or rhythm while in arpeggio
mode. At the bottom setting the master clock is used for the gates. The next
setting selects the same gates as in the sequence. Starting from position three
there are some more faster clocks as you move the fader up:

| Position | Arpeggio clocking |
|----------|-------------------|
| 8 | master clock × 8 |
| 7 | master clock × 6 |
| 6 | master clock × 4 |
| 5 | master clock × 3 |
| 4 | master clock × 2 |
| 3 | master clock × 1.5 |
| 2 | Gates from the sequence |
| 1 | master clock |

*Arpeggio clocking*

#### CV/gate or MIDI output

Each track can output its notes via CV/gate, MIDI or both. You decide this in the
tab *Output*:

For each tracks you can select either CV/gate or MIDI. When choosing CV/gate, the
patch generator chooses two to four output jacks depending on the number of
features that you have selected.

Enable MIDI output by selecting one of the possible MIDI ports. There is TRS 1 or
2 (3.5 mm stereo jack) or USB. In order to use MIDI output, you need either a
MASTER plus X7 or a MASTER18. TRS 2 is only available on the MASTER18.

You can also have some of the tracks output both MIDI *and* CV/gate. To do this,
put these tracks at the beginning of the list. Then set them to MIDI output
(choose one of the three MIDI output ports). And then enter the total number of
these "dual" track in the field *Number of tracks using MIDI + CV/gate (0-8)*.

I suggest that you don't enable MIDI output for tracks where you don't need it,
since it takes valuable bandwidth of your MIDI outputs and also needs same RAM in
the Droid.

The MIDI channels for the individual tracks are set in the tab *MIDI*:

It is allowed to use the same channel for more than one track. This creates a
polyphonic MIDI sequence.

**Hint**: If you do not use MIDI output, disable it. Or enable it just for those
channels that you are using with MIDI. This saves CPU ressources. Saving CPU is
always good as the less computing power your patch needs, the better its timing
is. The *Default* preset has MIDI enabled, so you might want to change this.

#### MIDI Input

You can get the following things via MIDI input:

- clock / start / stop
- root and scale

Both are done in the tab *MIDI*. For the clock you use the option *Get
start/stop/clock from MIDI*. Either you select a specific port to listen on or
you select *auto detection on TRS and USB*. If you do the latter, make sure that
there is just one clock arriving from your ports so things don't get confused. If
you get a clock via MIDI, also START / STOP events from this port are honored and
start and stop the sequencer.

Note: For the MIDI clock to work, you need to stop the internal clock of the
sequencer! This is done by pulling fader 6 down in the clocking menu!

You can also get the current *root and scale* information via MIDI. To do this
select *Get root/scale from MIDI* and select a port. The option "all" is not
available here.

You need to send the root information as a note event on channel 16. The octave
from the note is ignored, so just send any D♭ note to set the root to D♭.

The scale is set via CC#1, also on channel 16. A CC value of 0 selects the first
scale, which is Lydian. If CC#1 is 1, you get the second scale, which is ionian.
You get a list of all 108 possible scales on [page 119](scales.md).

Notes:

- These scale differ from those on the fader in the tonality menu, even if the
  first two scales are the same.
- You must put the fader for the root and scale to the bottom position or else
  they are used as offsets for these inputs (and might produce strange results).

The default position of the scale fader is "bottom" if you have enabled to get
the scale via MIDI, so a factory reset of the sequencer will set this up
correctly.

#### Connectivity

In the last tab – *Connectivity* – there are some options for adding CV inputs and
outputs for several things. Most of it is pretty straight-forward, so I won't go
through every detail, just allow me some notes:

The *Output for clock with user defined divider* sends the sequencer clock to the
outside, but with a division from 1 to 16 applied. This division is set by fader
7 in the clock menu from bottom (1) to top (16).

If you enable *Sinfonion link*, you can have the sequencer follow the musical
state of a Sinfonion. This requires a MASTER18. To do this:

1. Set `OUT 1` of the Sinfonion to *Sync master*.
2. Draw a standard patch cable from `OUT 1` of the Sinfonion to `I1` of your
   MASTER18.
3. Enable Sinfonion link in the patch generator.

If you do this, the current root note, scale and transposition of the Sinfonion
is automatically used by the sequencer. Beware: while you do this, you should
keep the corresponding three faders of the tonality menu in their neutral
position, because they will add up. For example if you set root to C♯ in your
sequencer, your root note will be always one semitone off. You need to set it to
C (fader completely down).

The setting *Get tonality, clock and reset on start of song* resets all your
sequencer tracks to step one if the chord progression sequencer of the Sinfonion
is at the start of its programmed song.

#### VCO Tuner

If you have a MASTER18 and a DB8E controller as part of your MFPS configuration,
you can active the *VCO tuner* in the "Features" tab. This adds a circuit of type
[`vcotuner`](circuits/vcotuner.md) (see [page 407](circuits/vcotuner.md)) to your
patch. You need the input `I1` free.

Now when you patch the output of a basic VCO waveform to `I1`, the tuner jumps
into action and the display looks like this:

Unpatch the cable to get rid of the tuner display when you are finished with
tuning.

#### Cheat sheets

Here, again, all of the fader menus at one glance:

```
Tonality menu                     Track menu
1. Root note                      1. Autoreset
2. Scale                          2. Shift steps
3. octave switch                  3. Octave switch
4. Diatonic transposition         4. Diatonic transposition
5. Absolute transposition         5. Activity
6. Tuning / compose mode          6. Movement pattern
7. Glide duration (per track)     7. Even clock divisions
8. Note range (per track)         8. Odd clock divisions

Clock menu                        Arpeggio menu
1. Set 0, 100 or 200 BPM          1. Autoreset
2. Add 0, 10, 20 … 90 BPM         2. Up / ping pong / down
3. Add 0, 1, 2, … 9 BPM           3. Octave switch
4. Continous clock bend           4. Butterfly
5. Swing                          5. Octaving pattern
6. Start / stop the clock         6. Movement pattern
7. Extra clock divider            7. Drop notes from scale
8. Pitch accumulator (per tr.)    8. Clocking
```

### 4.5 Droid Megasequencer

#### Overview

Welcome to the DROID Megasequencer. It is solely built from standard Droid
modules and comes preloaded with a special Droid patch that has been carefully
crafted to create a unique musical device. Because the Droid is an open system
you can change that patch and tweak it to your own liking. Or even change the set
of modules and build something completely new with the modules. The patch for
the Megasequencer is created by a patch generator that is directly built into the
Droid Forge.

The Megasequencer uses a 32 x 16 button matrix (512 buttons) to build a "piano
roll" like sequencer that can play two independent instruments via MIDI.
Instrument one is controlled by the left half of the button. That's 16×16 buttons
with white LEDs. Instrument two occupies the right half. Its buttons have blue
LEDs.

Features of the Megasequencer:

- 512 sturdy mechanical hardware buttons, each mounted and soldered in Germany.
- Instant hands-on creation of melodies and chords.
- Polyphony with up to 16 voices in parallel for two, three or four instruments.
- Independent clock divisions, pattern length and lots for more features for
  creative musical journeys.
- MIDI output via USB and DIN/TRS.

#### Modules

The Droid Megasequencer consists of the following modules in that order:

1. DROID master
2. X7
3. 16 × B32

If you want to create a Megasequencer from standard Droid modules, mount them in
a Eurorack case in that order and load the megaseq.ini patch onto your master. If
you've purchased the Megasequencer as a set, you can use the Droid modules for
something completely new and different at any time.

#### Inputs and Outputs

The Megasequencer patch uses the following inputs and outputs on the master and
X7:

Inputs:

- `I1`: Optional external clock. When you patch something here, that is used as
  clock for forwarding the sequencers.
- `I2`: Extern reset. A trigger here resets both sequencers to the start.

Outputs:

- `G9`: Clock output

All the other outputs and inputs are free. You can attach functions to it by
adapting the Droid patch.

#### MIDI Connectivity

The Megasequencer outputs a MIDI clock and a running state, as well as the note
events, both on the USB and the TRS output jack of the X7. Please consult the
Droid manual for details on the X7. If you want to use a standard DIN MIDI cable,
use the TRS to DIN adapter shipped with the Megasequencer.

If you send a MIDI clock via USB or TRS into the X7, that will be used as clock
and overrides the internal clock.

Notes:

- If you use USB-MIDI, the switch on the X7 module must be at the right position.
  Never put it to the left, that will bring your Droid master into SD card mode
  and disable the sequencer.
- Currently USB-C to USB-C cables might not work. Use the shipped USB-C to USB-A
  cable.
- If the top right LED of the X7 keeps lighting magenta and the USB MIDI device
  "Droid X7" is not detected by your Mac/PC, put the switch into the middle and
  to the right again.
- The TRS jack of the X7 (the top right jack) uses MIDI type B. The shipped
  adapter is also type B. At the back of the X7 is a little switch for changing
  the type to A. You need to unscrew the module for that. See the Droid manual
  for more details.

#### Clocking

Each sequencer needs a clock in order to move the steps forward. For this
sequencer you have four clocking options:

1. MIDI Clock via USB MIDI
2. MIDI Clock via TRS/DIN (at the top left jack of the X7)
3. Internal clock generated by the sequencer itself
4. External clock via input `I1`

There is no configuration but a precedence rule: If you patch anything into `I1`,
external analog clocking is enabled. All other clocks options are ignored. If you
provide a MIDI clock, that is used. Otherwise the internal clock is used. Note:
if you provide a MIDI clock both via TRS and USB, you run intro trouble since
both are honored at the same time. Don't do that.

The resulting effective clock is then sent to the output `G9` on the X7.

#### Basic operation

Pressing any button will enable a note in a 16 step pattern. Whenever the current
step reaches the button, a note will be played. The left instrument plays on MIDI
channel 1, the right instrument on channel 3. You can change the MIDI channels in
the menu (see below).

#### The menu

There is a hidden menu layer where you can do lots of interesting settings. You
bring up that layer by pressing and holding the bottom left button for at least
0.2 seconds. The menu is active while you hold the button.

While you are in the menu, the buttons do not longer reflect notes but have
special meanings. Since there are no labels on the buttons, this is probably a
bit confusing at the beginning, but you will learn that fast. And the last page
of this document is a printable version of menu layout. This is the layout of the
menu:

The upper half of the menu is split into left and right. Both sides control one
of the two instruments / sequencers. The lower half is for things that affect
both sequencers.

**Split**: If the split button is lit, the instrument is split into two halfs. In
split mode the upper eight rows output their notes to the MIDI channel plus one,
so if the instrument outputs on channel 1, the lower half plays on channel 1, the
upper half on channel 2. That allows you to control two different instruments
with one half of the sequencer.

**Legato**: Legato is a toggle setting. If it is enabled, the notes will be tied,
so a row of consecutive buttons is played as one long note, otherwise several
short notes are played.

**Reset**: The two reset buttons in the first row reset each individual sequencer
to its start step immediately.

**Speed x2**: If this toggle button is lit, the sequencer plays at double speed.

**Octave**: The next five buttons form a group in which at any time one of the
buttons is lit. This is an octave switch that transposes the output notes up or
down by octaves.

**Preset**: The next six buttons select the active preset. Each sequencer has six
presets. A preset contains of a 16×16 step melody. Switching to another preset
will not do a reset but immediately load a new pattern into the 16x16 buttons. At
the beginning all presets are empty so if you switch for the first time that will
clear all buttons. There is no load/save logic. Every change is saved
immediately.

**Clear**: Press this button to clear the current page of the 16x16 buttons of
the sequencer. This affects just the current preset. Beware: a long press (> 1.5
seconds) resets the whole Megasequencer to factory settings (including all the
settings in the menu page). You can use that if you are completely lost.

**MIDI channel**: The second row of buttons selects the MIDI channel the
sequencer should use for playing notes. The default is channel 1 (split mode:
1+2) for the left sequencer and channel 3 (split mode: 3+4) on the right
sequencer. Switching channels can be done in real time. This can be a nice
performance feature if you prepare a couple of different instruments on different
channels. Don't select channel 16 when in split mode, because there is no channel
17 in MIDI.

**Volume**: The third row of buttons select the volume. That's a MIDI message
which is transferred for the selected instrument. A volume of 0 (left button)
will probably silence the sound completely (that's up to your MIDI instrument).

**Non-Accent velocity**: This button row selects the velocity of the notes that
do not have an accent. Accents are discussed below. The left most button selects
a velocity of 50%, the most right button selects 100% and thus makes notes with
and without accent sound equal.

**MIDI Modulation wheel**: This sends MIDI CC#1 messages for the instrument. You
can use this to map changes in the sound, vibrato or similar effects.

**Clock divider**: These 16 buttons range from 1 (first button) to 16 (right most
button). If that is not set to 1, the sequencer advances to the next step after
that many clock ticks. You can use the clock dividers for polymetric effects. Or
you might have one instrument play at 1/16th of the speed and play slowly
changing chords.

**Pattern length**: If this is not set to 16 (the right most button), the
sequencer just plays the first X steps of the sequence and then jumps to the
beginning. Using a different pattern length for the left and right instrument can
create interesting polymetric effects.

**Activity**: This setting is usually at the right position, which means an
activity of 100%. If you select another value, just a random part of the selected
notes are played. That reduces the musical complexity of the pattern at just one
button press. Selecting an activity of 0% (left button) mutes the instrument. You
can this a mute button. Press button 16 to unmute.

**Transpose by semitones**: This setting affects both sequencers. One of the 32
button in the row is active and selects the base semitone (root note) for the
sequences. Each button is a different semitone.

**Master clock speed**: One of these 32 buttons is active and shows the selected
speed of the internal clock.

**Menu**: Holding this button brings up the menu, if held at least 0.2 seconds.
Release the button to leave the menu.

**Stop**: The stop button toggles the running state. If it is lit, the sequencer
is running, otherwise it is stopped. This overrides any start/stop signal from an
external MIDI clock.

**Reset**: A press on the reset button brings both instruments to their first step
of the sequence.

**Normal / Alt / Accent / Scale**: These last four buttons select one of four
global modes. Each mode is a kind of "page" for the buttons. So alltogether there
are five pages of 512 buttons. The Menu page is – as stated above – selected by
holding the menu button for at least 0.2 seconds. You switch to one of the other
pages by holding the menu button, selecting one of Normal / Alt / Accent / Scale
and then releasing the menu button.

**Normal mode**: This is the mode the sequencer comes when you first start it.
Every button represents one note to be played.

**Alt mode**: Every button in the "Alternate" mode represents one note - just as
in the normal mode. The notes in this mode are played every second bar - in
addition to the notes in the normal mode's page. That way you can create some
extra fills or ornamental notes that are just played half of the time.

If you select a note in the Alternate page that is already active in the normal
page thius note will be removed every second bar. So basically every active
button in the Alternate mode inverts the corresponding button in the normal mode
- but just every second bar.

**Accent mode**: Again, every button represents one note in the sequence.
Selected notes get an accent. Per default all the downbeats are selected and the
offbeats are deselected. Notes with an accent are played at 100% velocity. Notes
without an accent are played at lower velocity. You can set that in the menu page
(see above). Accents can make your patterns sound more interesting.

**Scale mode**: This mode is completely different than all the other modes. It
gives you complete control over the musical scale that is used. To be more
precise: For every of the 16 rows you can select which note to be played.

The field of buttons is divided into two parts. The left part with 31 out of 32
buttons per row selects one note for each row of the sequencer. Each button
represents one semitone. The following picture shows the default situation: a
natural minor scale (aeolian):

The last column of buttons shows a kind of "wave" animation to make clear that
these buttons are special. They are for loading one of 16 default scales into the
configuration. Pressing any of these buttons will set all 16 rows to notes of the
chosen scale. These are the default scales that you can load:

The 16 default scales are:

1. Natural major (ionian)
2. Dorian minor
3. Phrygian minor
4. Lydian major
5. Mixolydian major
6. Natural minor (aeolian)
7. Half diminished (locrian)
8. Melodic minor
9. Mixolydian ♯11
10. Altered
11. Harmonic minor
12. Spanish (harmonic minor from 5th)
13. Whole tone
14. Diminished (starting with whole tone)
15. Diminished (starting with semitone)
16. Chromatic (only semitones)

Note: any changes in the scale notes takes immediate effect. Try to setup some
melodies in the sequencer that use as many different notes as possible. Then go
to the scale mode and simply play around. You will see that interesting musical
effects can be achieved. Also have in mind that it can be interesting if several
rows play the same note for a while. That reduces the harmonic complexity of the
melody.

#### Megasequencer menu cheat sheet for printing

```
Top half (per instrument, left and right):
  Row buttons:  Split · Legato · Reset · Speed x2 · [Octave ×5] · [Preset ×6] · Clear
  MIDI channel
  Volume
  Non-Accent Velocity
  MIDI Modwheel (CC1)
  Clock divider
  Pattern length
  Activity

Bottom half (both sequencers):
  (free)
  (free)
  (free)
  (free)
  (free)
  Transpose by semitones
  Master clock speed
  Menu · Stop · Reset · Normal · Alt · Accent · Scale · (free)
```

## 5. Creating DROID patches with a text editor

### 5.1 General procedure

If you don't like to use the Forge, you can write patches by directly editing the text file. This is the general procedure:

1. Create a text file called `droid.ini`.
2. Copy this file to a micro SD card.
3. Insert the card into your DROID master.
4. Press the button on the DROID master.

If the DROID finds an error in your patch, LEDs will blink and tell you more about that error. Fix your error and try again. That's all.

On the MASTER18 or if you have attached an X7 expander to your MASTER, you have an additional option for loading a patch, which is a lot easier. The USB port on the MASTER18 or X7 gives you direct access to the SD card. The card is attached to your computer by putting the little switch on the MASTER18 / X7 to the left. This is like *inserting* the card into your computer. Now you can edit or copy your `droid.ini`. Afterwards simply put the switch back to its center position. That will remove the card from your computer (eject it first with your file browser). Also the patch will be immediately loaded by your master, no need to press the button.

Since the Forge operates on the same kind of text files, you can open such a manual file with the Forge and also edit Forge-created files with a text editor. The Forge even has a simple built in editor for editing the patch or just parts of it in its text form.

#### Procedure in details

Here is the procedure again with some more details:

1. Use your PC, Mac or Linux box for creating a text file with the name `droid.ini`. A text file is not a MS Word file. In Windows you can create or edit a text file with Notepad or with some more convenient text editor. Note: some might want to edit `droid.ini` directly on the SD card. This is possible, of course. It's always handy, however, to have a copy of that file on your computer, just in case.

2. When you are finished, copy this file to the micro SD card your DROID has been shipped with or to any other micro SD card that is compatible with DROID. You need a micro SD card reader for this. Do not use any subdirectories on the card. Put the file into the main directory. The card needs to be formatted with the standard FAT filesystem. If you buy a new card, it is most likely formatted that way anyway. Hint: If you like, you can create and edit your file directly on the card, of course. This saves the extra step of copying it.

3. Insert the micro SD card into the small card slot of your DROID master. Put it in with the metal contacts downwards. Be gentle, as always :-)

4. Press the button left of the SD card slot. Of course your DROID has to be powered up while you do this. The DROID now reads the file `droid.ini`, copies it into its internal flash memory and restarts, in order to load and activate the new patch. If everything is OK, one light will make one quick circle around the 16 LEDs and your patch is up and running. After that you can remove the card if you like. Your DROID does not need it anymore. Note: If you are using an X7 expander, the memory card remains in the master module all the time. You also don't need to press the button on the master, just use the switch on the X7.

### 5.2 Basic structure of the patch file

DROID offers a long list of pre-programmed functionalities - called circuits - from which you can pick and choose for your needs. Each circuit takes input values, processes them and produces output values. It is your task to set the inputs to values you like. Such a value could be taken from a hardware input, a button, a pot, or simply be a fixed value. The outputs of the circuit can be connected to hardware outputs, LEDs or even to the inputs of other circuits in order to create more complex patches.

All this is configured in a simple text file with the name `droid.ini`, which is also called the **Droid patch**. Using a simple text file has lots of advantages:

- You can edit it with nearly every operating system.
- No special software is needed. This will probably still work in 30 years, when you just have bought a vintage DROID on ebay for a couple of thousand bucks.
- You can easily post and share your DROID patches or patch snippets in our Discord community or on other internet boards.
- You can copy & paste parts from other one's DROID patches.
- You can add comments to your patch.

Here - again - is an example of a DROID patch:

```droid
[lfo]
    hz        = 0.5
    triangle  = _CABLE_1

[contour]
    gate      = I1
    decay     = _CABLE_1
    sustain   = P1.1
    release   = I2
    output    = O1
```

As you can see the `droid.ini` is a list of circuit declarations. In the upper example we see two circuits: `[lfo]` and `[contour]`. Each one comes with a list of inputs and outputs that are assigned to jacks, fixed values or internal patch cables.

In the example all jack declarations are indented for better readability.

### 5.3 Finding a problem in your DROID patch

It is not entirely unlikely that you got something wrong in your patch, some syntax error, some invalid line, stuff like that. Humans make errors, but this is no big deal, since DROID helps you finding the reason and location of any problem in your DROID patch by several means:

1. It blinks the button five times in a row.
2. It creates a file called `DROIDERR.TXT` on your SD card.
3. It flashes some LEDs in a certain way.

So if you experience any strange button or LED blinking after loading your patch, put the card back into your computer (or put the switch on your X7 to the left again) and look into the file `DROIDERR.TXT`, which should be there now. This file just contains one line, maybe like this one:

```
ERROR IN LINE 17: Invalid output 'O9'. Allowed is O1 ... O8
```

This tells you the exact location and reason of your problem so that you can easily fix it.

#### LED blink codes on the MASTER

As an alternative to the error file, the MASTER also shows the location and reason of the error in form of LED blink codes in its 4 × 4 LED matrix.

There are two types of errors that you can make:

1. **General errors** concern the patch as a whole. The SD card is missing. You have misspelled the file name. Things like that. In such a case *all* LEDs will flash in the same color. The color indicates the reason of the error. On the next page you find a table of all *global error codes*.

2. **Local errors** concern just one specific *line* in your DROID patch. In that case just some of the LEDs will flash. Again, the color shows you the reason for the error, according to the table *local error codes*. In addition, the LEDs show you the exact *line number* where your error occurs. This is done in the following way:

- The input LEDS 1 … 8 indicate the *tens* of the line number. If the error happens to be in line 90, then LED 1 + 8 will flash. If it is in line 1 to 9, then no input LED flashes at all.
- The output LEDS 1 … 8 indicate the *ones* and are added to that number. Again, if a 9 is needed, then 8 + 1 will flash.
- If your patch has more than 99 lines, then the error could be in line 100+. In that case one of the input LEDs will flash *white*. That LED indicates the hundreds of the line number.
- If the error is in some line at 900 or more, several LEDs will flash white. Just add them up. So e.g. if LED 2 and LED 8 flash white, this means 100 times 1000, hence 1000.
- The maximum line number that can be shown that way is, if all eight LED flash white plus 99. That is 100 + 200 + … + 800 + 99 = 3699. If your patch has even more lines, better look into the file `DROIDERR.TXT`. There you can see the line number of the error in clear text.

#### Examples for error codes

The following examples show how errors appear in the 4 × 4 LED matrix of the MASTER:

- Invalid parameter value in line 81
- Undefined parameter in line 90
- Invalid register in line 99
- Line too long in line 144
- The SD card was not found or could not be read (all LEDs cyan)
- Too many circuits or out of memory (all LEDs red)

#### LED blink codes on the MASTER18

The MASTER18 does not have LEDs on the front panel. But it has four LEDs on its back. They do not show the location of the error but at least the type. The rule is this:

- If LD1 blinks, it's a global error. The color matches those in the table below.
- If LD2 blinks, it's an error in some line of the patch. Again look for the color in the table below.

The exact location of the wrong line is not visible in the LED blink code. You find it in the file `DROIDERR.TXT` on the SD card.

### 5.4 Table of error codes

#### All LEDs flashing at once (global error)

| Color | Meaning |
|---|---|
| yellow | **Patch not found**: This can happen in the following situations: 1. No file with the name `droid.ini` is present on the memory card. 2. You DROID started without having loaded a patch ever. 3. You did a factory reset without loading a patch afterwards. |
| red | **Too many controllers**: You have declared more than the allowed number of 16 controllers. |
| blue | **Patch is too big**: The size of your `droid.ini` file is too big. The maximum of the size *without* spaces and comments is **64,000** bytes – which is quite a lot. |
| cyan | **Out of memory:** The circuits in your patch use too much memory. So you have too many large circuits or too many circuits in total. The memory consumption of each circuit only depends on its type. The smallest circuit is `bernoulli` and has a size of about 200 bytes. The largest circuits are `midifileplayer` with 7000 bytes and `cvlooper` with 18,000 bytes. Most circuits need between 400 and 800 bytes. And the total available memory is about 110,000 bytes. |
| magenta | **Invalid firmware file:** The firmware upgrade failed because the contents of `droid.fw` is invalid. The file is incomplete or corrupted. |
| white | **No SD card found**: No card could be found. Maybe you inserted it in the wrong way? Or your card is not supported. Or you pressed the button too early. Sometimes it helps to simple press the button again. |

Note: If you get your *start animation* with just white LEDs instead of colored ones, your DAC calibration needs to be redone. See page [113](hardware.md) for details.

#### Just some of the LEDs flashing (local error in one line in `droid.ini`)

| Color | Meaning |
|---|---|
| yellow | **Unknown register**: You used a non-existing register name (registers are the things like `O1`, `I7` and so on). Please check the list of allowed registers in this manual on page [55](basics.md). |
| orange | **Unknown parameter name**: that circuit does not support that parameter. Please check the circuit references in chapter [16](circuits/). |
| red | **Unknown circuit**: This type of circuit does not exist. Please check the exact spelling. Maybe you have an old firmware that does not support that circuit yet? On page [110](hardware.md) you learn how to do a firmware upgrade. |
| blue | **Line too long**: One line in your patch exceeded the maximum allowed line length of 63 characters. |
| green | **Internal patch cable misused**: One of your internal patch cables (see page [60](basics.md)) is not properly used: **1. No input:** One patch cable is only used as output. **2. No output:** One patch cable is only used as input. **3. Double output:** One patch cable is used twice as an output. |
| magenta | **1. Invalid header of circuit**: DROID was expecting an opening square bracket `[`, but found something else. **2. Invalid parameter line**: DROID was expecting something like `clock = I7`, but found something completely different. Parameters always start with a letter. This is followed by an equals sign. **3. Invalid parameter value**: Your parameter has an invalid value. Please checkout this manual about allowed values for parameters and their exact syntax. |

### 5.5 Inputs, outputs and other registers

Your master has lots of inputs and outputs. Also the LEDs on the MASTER and in the buttons of your controllers behave like outputs. Buttons and pots behave like inputs. All these are called *registers*, because they behave like things that can store values. Each register is named with one special character followed by a number or number combination.

The eight CV outputs of your master start with the letter `O` and are named `O1` through `O8`. The CV inputs of the MASTER are called `I1` … `I8`. With the normalizations `N1` … `N8` you can specify a signal or value that should be used for `I1`, `I2`, … `I8` when no patch cable is inserted. But we will come to that later.

The MASTER18 has two gate/trigger inputs called `I1` and `I2` and four gate outputs called `G1`, `G2`, `G3` and `G4`.

When you have attached a G8 expander, you get eight more jacks. On the MASTER these are called `G1` through `G8`. On the MASTER18 they are `G2.1` … `G2.8`. Each of these can either be used as an input or an output. They are simple gate inputs/outputs that just know "On" and "Off", or 0 and 1. When used as an output they output either 0 V or 5 V.

Starting with the blue-3 firmware and the new version of the G8 expander, you can add up to four G8s to you master. If you have more than one G8, you need a dot-notation for the gate names, for example the gate 7 on the second G8 is called `G2.7` on the MASTER and `G3.7` on the MASTER18.

The stuff on your P2B8, P4B2, B32, P10 and other controllers can also be accessed via registers. Here there is always a dot in the name, separating two numbers, like `P1.2` or `B4.8`. The first number is always the number of your controller. The second number is the number of the element on the controller. So `B4.8` is the 8th button on the 4th controller. P10 controllers just have `P` registers, no `B` or `L` registers. Likewise the B32 has just buttons and thus no `P` registers.

Please note that each button has *two* registers: one with the letter `B` for the button itself. DROID will set that to `1.0` while the button is pressed (and hold) and to `0.0` otherwise. The second register is for the LED in the button and begins with `L`. This is an *output* register where you can write values to. A value of `0.0` will set the LED off, while `1.0` creates full brightness. But the LEDs also support any number in-between and will have a brightness according to that number. Negative numbers are treated like positive numbers here, so `-0.5` will produce the same brightness as `0.5`.

As long as you do not actively use the `L`-registers the LED in a button will automatically be lit while you hold it. Please look at the [`button`](circuits/button.md) circuit in page 154 for how to convert a push button into one that toggles its state on each press.

#### Overriding the LEDs of master, G8 and X7

The registers `R1` through `R56` give you access to the 4 × 4 LED matrix on the MASTER and to the 2 × 4 LED matrices on the G8s and X7. The let you override the normal function of these LEDs and give you a way to show internal states of your patch. This is especially useful when you have a couple of unused inputs (and thus unused LEDs). Sending some internal values to one of these LEDs gives you some feedback about what your DROID is doing.

Sending a value of 0.0 to such a register makes the corresponding LED dark. Other values select a color at full brightness. Here is the table of colors (intermediate values give intermediate colors):

| Value | Color |
|---|---|
| 0.2 | cyan |
| 0.4 | green |
| 0.6 | yellow |
| 0.73 | orange |
| 0.8 | red |
| 1.0 | magenta |
| 1.1 | violet |
| 1.2 | blue |

Registers on the **MASTER**:

| Register | Type | Description |
|---|---|---|
| `I1 I2 I3 I4 I5 I6 I7 I8` | input | The eight CV inputs |
| `N1 N2 N3 N4 N5 N6 N7 N8` | output | The normalization of these inputs. When nothing is patched into an input, the according `I`-register will take its value from the matching `N`-register instead. Any they are `0.0` if you have not set them. |
| `O1 O2 O3 O4 O5 O6 O7 O8` | output | The eight CV outputs |
| `G1 G2 G3 G4 G5 G6 G7 G8` | input/output | The jacks of the first G8 expander. Each can be used either as an input or as an output. Instead of `G1` you can write `G1.1`. |
| `G2.1 G2.2 G2.3 G2.4 ... G2.8` | input/output | The eight gate jacks of the second G8 expander. Use `G3.X` and `G4.X` for the third and fourth G8 expander. |
| `G9 G10 G11 G12` | output | The four gate jacks of the X7 expander. These are always outputs. |
| `R1 R2 R3 R4 R5 R6 R7 R8` | output | The colored LED squares in the first two rows (those for the inputs) |
| `R9 R10 R11 R12 R13 R14 R15 R16` | output | The colored LED squares in row three and four (those for the outputs) |
| `R17 ... R48` | output | The colored LED squares on the first, second, third and fourth G8 expander |
| `R49 ... R56` | output | The colored LED squares on the X7 expander |
| `X1` | output | Special register for displaying a value encoded in the 4 × 4 LED matrix |

Registers on the **MASTER18**:

| Register | Type | Description |
|---|---|---|
| `I1 I2` | input | Gate/trigger inputs |
| `O1 O2 O3 O4 O5 O6 O7 O8` | output | The eight CV outputs |
| `G1 G2 G3 G4` | output | The four gate/trigger outputs |
| `G2.1 G2.2 G2.4 ... G2.8` | input/output | The eight gate jacks of the *first* G8 expander. Use `G3.X`, `G4.X` and `G5.X` for the 2ⁿᵈ, 3ʳᵈ and 4ᵗʰ G8. |
| `G9 G10 G11 G12` | output | The four gate jacks of the X7 expander. These are always outputs. |
| `R1 R2 R3 R4` | output | The colored diagnostic LEDs on the back of the module |
| `R17 ... R48` | output | The colored LED squares on the first, second, third and fourth G8 expander |
| `R49 ... R56` | output | The colored LED squares on the X7 expander |

Registers on the controllers:

| Register | Type | Description |
|---|---|---|
| `P1.1 P1.2 P2.1 P2.2 P3.1 P3.2 …` | input | The pots, sliders and faders on your controllers. `P3.2` is the 2ⁿᵈ pot on your 3ʳᵈ controller. |
| `E1.1 E1.2 E1.3 E1.4 E2.1 …` | special | The encoders of your E4 or DB8E controllers. These registers can only be used in junction with the circuits [`encoder`](circuits/encoder.md) (see page 213), [`encoderbank`](circuits/encoderbank.md) (see page 208) and [`encoquencer`](circuits/encoquencer.md) (see page 223). |
| `B1.1 B1.2 B2.1 … B2.1 B2.2 B2.3 …` | input | The push or touch buttons on your controllers. `B3.6` is the 6ᵗʰ push button on your 3ʳᵈ controller. |
| `L1.1 L1.2 L2.1 … L2.1 L2.2 L2.3 …` | output | The LEDs in these buttons |
| `R1.1 R1.2 R1.3 R1.4 R2.1 …` | output | The color of the LEDs in the touch buttons of the M4 controllers |

### 5.6 Specifying numbers in your patch

Note: you always need to write the numbers in "plain" format, for example `0.01` or `12345.67` or `-5.0`. Scientific notations like `3.4^-10` are not allowed. It's also not allowed to write just `.5` instead of `0.5`.

There are two suffixes that you can attach to a number: `%` and `V`. Appending a percent sign basically divides the number by 100, so …

```droid
    pulsewidth = 45%
```

… is just the same as

```droid
    pulsewidth = 0.45
```

Appending a `V` divides the number by 10, which is exactly what you need in order to convert a number to a voltage to be output at a jack. So:

```droid
    pitch = 2V
```

… is just the same as

```droid
    pitch = 0.2
```

Sometimes this is easier to read. Please be just aware that the `V` is applied just to the number itself. So you **could** write `1/12V`, but that is *not* 1/12 V, but is 1/(12·V), which is – when you convert the voltage back to a number – 1/(1.2), which is 0.8333. Whereas 1/12 V would be 0.008333 – a hundred times smaller!

Some inputs or outputs behave like gates that only know 0 or 1, low or high, on or off. For your convenience you can use the words `off` – which is just a short hand for `0.0`, and `on` – which stands for `1.0`, if you like. Here is an example:

```droid
[contour]
    loop    = on
    output  = O1
```

This is exactly the same as:

```droid
[contour]
    loop    = 1.0
    output  = O1
```

### 5.7 Attenuating and offsetting inputs

#### Attenuation / Amplification / Multiplication

Each *input* of a circuit (not the outputs!) has a built-in option for attenuation and offsetting. Attenuation is done by multiplying the input with a value. Well, if you "attenuate" with a number greater than 1, the name attenuation would not really be correct, since the signal in fact gets amplified and not attenuated.

Let's assume you want to control the `level` parameter of an LFO with the first pot of your first controller (see page [267](circuits/lfo.md) for details on the LFO circuit). That pot can be addressed with `P1.1`:

```droid
[lfo]
    level  = P1.1
    output = O1
```

The pot has a range from 0 to 1, which corresponds to 0 V … 10 V. That's maybe too much for you application. So let's limit the range to 5 V, which is the same as 0.5. This is done by multiplying the pot with 0.5:

```droid
    level = P1.1 * 0.5
```

Now `level` will range from 0 V to 5 V.

The attenuation does not need to be a fixed number. Let's CV control the level of the LFO with the external input `I1`. Now we multiply that with the pot `P1.1`, which makes the latter an attenuator for the CV. How cool is that?

```droid
    level = I1 * P1.1
```

Fixed numbers can also be negative. The following line basically *inverts* the LFO's output since its output voltage is negated:

```droid
    level = P1.1 * -1
```

If you like, you can use a short hand for that:

```droid
    level = -P1.1
```

But that is really just an abbreviation for `-1 * P1.1`. From that follows, that `-P1.1 * I1` is **not** possible, since this would be `-1 * P1.1 * I1`, which would be *two* multiplications!

#### Division

There is another shorthand: It is allowed to use division, if the thing you divide by is a *fixed number*. So instead of `pitch = I1 * 0.0833333` you can write:

```droid
    pitch = I1 / 12
```

Again, this is a short hand for `I1 * 0.0833333` and this its treated as a multiplication. For that reason you cannot write `I1 / P1.1` or anything similar, since here the DROID would really have to do a dynamic division with the current value of `P1.1`. Use the [`math`](circuits/math.md) circuit for such things (see page 276).

#### Offsets / Summing

An *offset* is applied by adding a number. This must be written after the (optional) attenuation. Let's have the level of the LFO set by `P1.1` but at least 2 V:

```droid
[lfo]
    level = P1.1 + 0.2
```

Now the level would range from 2 V to 12 V. Since 10 V is the maximum, we could multiply the pot with 0.8 first, which results in a range from 2 V to 10 V:

```droid
    level = P1.1 * 0.8 + 0.2
```

Again you are not restricted to fixed numbers. You can also use any DROID register you like. In this example we use `P1.1` as a coarse tune and `P1.2` as a fine tune (20 times finer) for the rate of an LFO:

```droid
[lfo]
    square = O1
    rate   = 0.05 * P1.2 + P1.1
```

Using `+` can even be used for mixing together *two* input signals. The circuit [`copy`](circuits/copy.md) just copies an input to an output, but since it can be used with any register you can build a simple CV mixer:

```droid
    input = I1 + I2
```

Note: If you want to sum more than two signals, use the [`mixer`](circuits/mixer.md) circuit (see page 314 for details).

#### Subtraction

Mathematics says, that subtraction is nothing else than the addition of a negative number. So you can subtract `0.5` from `P1.1` by writing:

```droid
    input = P1.1 + -0.5
```

Since this looks clumsy, you are allowed to write as a short hand:

```droid
    input = P1.1 - 0.5
```

Note: you *can* also use the negation on a register:

```droid
    input = I1 - I2
```

But note: here this is an abbreviation for `-1 * I2 + I1`! So you already have "used up" your multiplication, even if you don't see it. The general rule is: If DROID can transform your line into the form A * B + C, everything is good.

#### Summary and Further notes

- Generally the format is A * B + C. So you are limited to one attenuation (multiplication) and one offset (addition / subtraction)
- Each of A, B and C can be a fixed number, any of the registers or an internal patch cable (for those see page [60](basics.md)).
- Attenuation must be written first, offset last.
- There are some abbreviations for subtraction and division. They work if the thing can be transformed into A * B + C.
- No other operations are allowed (no brackets, additional operations, divisions, etc.)
- If you need more complex math operations, have a look at the [`math`](circuits/math.md) circuit (see page 276).

Are you curious *why* DROID does not allow more complex operations? Why is it so restrictive? The reason is a matter of CPU performance! When your patch is parsed, everything is converted to A * B + C. If you don't use the multiplication, B is set to `1`. No offset? Then C is `0`. So when it comes to the real time computation of these values, it's just the simple A * B + C. No conditions to be tested, no if/then/elses or similar stuff. It's really super fast. And that's important because you want your DROID to have low latency and smooth envelopes.

### 5.8 Texts

A few circuits need texts as parameters. These play a role if you have a DB8E controller (see page [80](hardware.md)) and want to display certain texts in the display. One example is the parameter `header` in the circuit [`display`](circuits/display.md) (see page 199). You can use it to set a header line for your display:

```droid
[display]
    header = "Voice allocation"
    ...
```

You specify a text using double quotes. Within the quotes write whatever text you like, but:

- Only printable ASCII-characters are allowed (code 32 … 126).
- It is not possible to put a double quote into a text. There is no "escape" character like backslash. Sorry. But it makes the software simpler. If you really think you need quotes, use single quotes.

Texts are not really a new data type. When your master reads a patch, all unique texts in the patch are replaced by integer numbers. The first text is replaced by `1`, the second by `2` and so on. Consider the following rather useless example:

```droid
[display]
    value  = I1
    header = "Input"

[display]
    text   = "Hello world!"
```

After parsing the patch looks like this:

```droid
[display]
    value  = I1
    header = 1

[display]
    text   = 2
```

At the same time, the master has constructed a table of texts:

| # | Text |
|---|---|
| 1 | Input |
| 2 | Hello world! |

This means, that texts can be processed by other circuits, as well, for example by a [`switch`](circuits/switch.md) circuit:

```droid
[switch]
    input1 = "Bass drum"
    input2 = "Snare drum"
    input3 = "Open hats"
    input4 = "Closed hats"
    offset = _INSTRUMENT
    output = _INSTRUMENT_NAME

[display]
    header = "Instrument"
    text   = _INSTRUMENT_NAME
```

So if the value of `_INSTRUMENT` is `0`, `1`, `2` or `3`, the display will show the according text. This works, because the `switch` circuit does not need to know anything about texts, it just sees numbers. The `display` circuit then gets this number at its `text` input and looks into the table of texts in order to know what to display.

Theoretically you could directly specify numbers like this:

```droid
[display]
    header = 5
```

But without knowing what text number `5` is in your actual patch, this seems pretty useless and error prone. Your master will load and parse this patch happily, anyway. If you specify a text number that is not in the table, the text is considered to be empty.

Since texts are numbers, you can also use the offset and attenuation feature of the inputs. One example could be to enable a text based on a condition:

```droid
[display]
    text = "Button pressed" * B1.1
```

The text `"Button pressed"` translates to some text number. When the button is pressed, `B1.1` is `1`, otherwise `0`. So the result is the correct text number, while the button is held and `0` otherwise. `0` is not a valid text and hence will be considered as an empty text.

### 5.9 Internal patch cables

One of the fun parts is the fact, that internally you can connect several circuits without using any real inputs or outputs. Instead of an output you simply put a name of your choice that begins with an *underscore*. That same name can be used at another circuit as an input. Here is an example of an internal LFO triggering an envelope:

```droid
[lfo]
    square = _TRIGGER

[contour]
    trigger = _TRIGGER
    output  = O1
```

This patch cable is always a multiple, so it can be used by more than one circuit:

```droid
[lfo]
    square  = _TRIGGER

[contour]
    trigger = _TRIGGER
    attack  = 0.0
    release = 0.2
    output  = O1

[contour]
    trigger = _TRIGGER
    attack  = 0.5
    release = 0.8
    output  = O2
```

Note: There are two rules that are checked by the DROID. And it will show an error message in green if one of these are found to be broken (see page [51](basics.md) for an explanation of the error codes).

1. Each internal patch cable must be used as an input *and* as an output (otherwise it would be useless).
2. No internal patch cable may be used *twice as an output*. This would make no sense and is in effect a short circuit.

### 5.10 Using outputs as inputs

There is another way of connecting circuits: You can use an *output* register as an input to another circuit. The following example creates an LFO that outputs a square wave to LED `R1`, in order for it to flash in the speed of the LFO. `R1` is the LED designated for input 1, but we simply misuse that as a signal LED for our LFO. Then an euclidean rhythm is triggered with that same signal, simply by using `R1` as an input here:

```droid
[lfo]
    hz     = 2
    square = R1

[euklid]
    clock  = R1
    length = 12
    beats  = 5
    output = O1
```

### 5.11 Using inputs as outputs

Using input registers as outputs is not allowed. And it would not make any sense. If you try so, you will get a yellow blinking error message for the according line.

Look at the following example. Here - due to a copy & paste error - the LED states are sent to the button registers. That won't work. And for that reason DROID won't allow it:

```droid
[buttongroup]
    button1 = B1.1
    button2 = B1.2
    button3 = B1.3
    led1 = B1.1 # Argr. should be L1.1!
    led2 = B1.2 # Argr. should be L1.2!
    led3 = B1.3 # Argr. should be L1.3!
```

### 5.12 Parameter arrays

Some of the circuits have arrays of similar jacks, like `output1`, `output2`, `output3` and so on. Here you can always omit the digit `1` if you just want to address the first jack in the list. So `output` is just the same as `output1`.

### 5.13 Comments & spaces

You can use comments in your DROID patch by making use of `#`. Then all further text until the end of the line is being ignored:

```droid
# Here comes the envelope for the foobar voice
[contour]
    trigger = _TRIGGER # wired to sequencer
    attack  = 0.5 # another comment
    release = 0.8
    output  = O2 # wired to foobar trigger
```

### 5.14 Abbreviated parameter names

There is a limit of 64,000 bytes that a patch may be long. Since spaces and comments are removed automatically by the master when you load a patch, they do not account for. Nevertheless, you can run into this limit if you create more complex patches.

A new way to reduce the patch size has been introduced in the firmware blue-6. Now every parameter has an abbreviation. You find the complete list of all abbreviations in the firmware ZIP file in the subdirection `manual`. There is a file called `droid-cheatsheet-...pdf` (with the name of the firmware inserted). This how it looks for the circuit [`algoquencer`](circuits/algoquencer.md) (see page 127):

| Parameter | Abbreviation |
|---|---|
| `clock` | `c` |
| `reset` | `r` |
| `button1 ... button16` | `b` |
| `length` | `l` |
| `pattern` | `pt` |
| `nextpattern` | `np` |
| `prevpattern` | `pp` |

For example you can write just `c` instead of `clock`, `r` instead of `reset` and `b` instead of `button`. Some abbreviations use more characters such as `pt` for `pattern`.

So instead of

```droid
[algoquencer]
    clock = I1
    reset = I2
    pattern = _PATTERN
    button1 = B1.1
    button2 = B1.2
    ...
```

… you can write

```droid
[algoquencer]
    c = I1
    r = I2
    pt = _PATTERN
    b1 = B1.1
    b2 = B1.2
    ...
```

The Forge has an option called *Use abbreviated parameter names* in the preferences. If you work with the Forge, just tick that option and your patches will be compressed by abbreviating parameter names, automatically.

### 5.15 More than one patch on the memory card

Sometimes you might want to have more than one DROID patch on your card and switch back and forth between these without going back to your computer. This can be done if you have at least one controller with buttons, such as P2B8, P4B2 or B32.

It goes like this: Put your additional patches on the card with special filenames in the format `droidXY.ini`, where *X* is the number of the controller and *Y* the number of the button. Then for example `droid14.ini` will be loaded if you *first press and hold* the button *4* on your first controller while then pressing the load button on the master.

This way if you have one P2B8 you can choose between nine different patches. If you have a second P2B8 controller, this extends to 17 patches, because now holding button 1 on controller **2** will load `droid21.ini` and so on. A B32 gives you a total of 32 alternative patches to load and so on. And yes: if you have 10 or more controllers and some B32 amongst them, `droid124.ini` would be loaded by button 24 on controller 1, but also by button 4 on controller 12.

**Important:** It is crucial that *every* of your patch files contains the appropriate `[p2b8]` or other controller declarations! Otherwise you won't be able to switch over to the other patches since button presses will not longer be registered by the DROID master. It will instead fall back to the normal `droid.ini` in that case.

If you load a patch that way, the states of your circuits are saved in a special file that accompanies the patch. The name of that file is `DSTAXY.BIN`, so for example `DSTA14.BIN` if you load the patch `droid14.ini`. All you need to know is that each patch has separate state. So if you e.g. have an [`algoquencer`](circuits/algoquencer.md) in each of two patches, it's patterns will seperately loaded and saved.

# Adafruit DRV8833 #3297: perfboard TLDR

Scope: the assembled Adafruit DRV8833 breakout sold as product #3297, not a
generic DRV8833 carrier. Dimensions below are nominal values from Adafruit's
fabrication drawing and Eagle design. Verify that the board in hand has the
same markings and layout before drilling or soldering.

## Orientation and pin order

Use this top-view datum: components and white silkscreen face the observer,
the 8-pad control/power row is at the top, and the two mounting holes are at
the bottom corners.

```text
TOP / COMPONENT SIDE

 AIN1  AIN2  SLP  BIN2  BIN1  FLT  GND  VM
   o     o     o     o     o    o    o    o

              components                 VMotor terminal area
                                           + o  protected PWRIN
                                           - o  GND

 (M2.5) AOUT1 AOUT2 BOUT2 BOUT1 ASEN BSEN (M2.5)
    O      o     o     o     o     o    o     O
```

For the following bottom view, turn the board left-to-right while keeping the
8-pad row at the top. It is therefore the horizontal mirror of the top view.

```text
BOTTOM / SOLDER-JUMPER SIDE

 VM  GND  FLT  BIN1  BIN2  SLP  AIN2  AIN1
  o    o    o     o     o     o     o     o

           ISENSE A/B solder jumpers

 (M2.5) BSEN ASEN BOUT1 BOUT2 AOUT2 AOUT1 (M2.5)
    O      o    o     o     o     o     o     O
```

The bottom `ASEN` and `BSEN` labels are sense-node breakout pads, not extra
grounds. Do not close either bottom solder jumper for this project: closing a
jumper bypasses its 0.2 ohm sense resistor and disables that bridge's 1 A
current limit.

Sources: `adafruit-guide.pdf`, pp. 5-7 and 26; connector footprints and nets
in `adafruit-schematic.sch:537-548`, `adafruit-schematic.sch:1085-1094`, and
`adafruit-schematic.sch:5291-5555`.

## Mechanical layout

- Board outline: 25.40 x 17.78 mm (1.00 x 0.70 inch), with rounded corners.
- Every signal/output pad has a 1.00 mm plated drill and nominal 1.778 mm pad.
- Both rows use 2.54 mm (0.100 inch) pitch. Their row centerlines are 12.70 mm
  (0.500 inch) apart, so they fit a conventional 0.1-inch perfboard grid.
- The 8-pad row spans seven intervals, 17.78 mm (0.700 inch), center to center.
- In the top view, the six lower connection pads align below `AIN2`, `SLP`,
  `BIN2`, `BIN1`, `FLT`, and `GND`, respectively.
- There are exactly two mounting holes, not three. Both are plated, with a
  2.50 mm drill and 3.20 mm copper pad. Their centers are 20.32 mm (0.800 inch)
  apart and share the lower connection-row centerline.
- Each mounting-hole center is displaced by 1.27 mm (0.050 inch, half a
  perfboard pitch) outward from the adjacent 8-pad-row extent. The mounting
  holes therefore do not both land on the same 0.1-inch grid as the pins.
- Optional terminal-block footprint: 3.50 mm pitch, 1.00 mm drills. It is not
  on the 0.1-inch grid. Product #3297 is documented as including the assembled
  breakout and loose header strip, not the terminal block. In the defined top
  view its upper terminal is protected `VMotor+`/`PWRIN`, and its lower
  terminal is GND.

Adafruit's fabrication drawing is the dimensional authority:
`adafruit-guide.pdf`, p. 26. The vendored Eagle footprint confirms the drill
and pad sizes: `adafruit-schematic.sch:424-433`,
`adafruit-schematic.sch:537-548`, and `adafruit-schematic.sch:1085-1094`.

## Perfboard keepout

- Reserve at least the complete 25.40 x 17.78 mm board projection. Do not put
  tall parts beneath it; the bottom has exposed current-limit jumpers and
  through-hole solder joints.
- Around each mounting-hole center, the Eagle package uses top/bottom restrict
  geometry out to approximately 2.02 mm radius (4.03 mm diameter). Keep copper
  and perfboard pads outside that diameter at minimum.
- A screw head, nut, spacer, or washer can require more clearance than the PCB
  restrict geometry. Its keepout cannot be specified until the exact fastener
  and mounting side are selected.
- Keep motor/output and VM wiring short and away from noise-sensitive wiring.
  The module includes local bypassing, but required additional bulk
  capacitance depends on motor current, lead inductance, supply impedance,
  braking, and allowed ripple and must be established by system testing.

Sources: mounting-hole restrict and pad geometry in
`adafruit-schematic.sch:537-543`; exposed jumpers and current-limit behavior in
`adafruit-guide.pdf`, pp. 6-7; supply/bulk-capacitor guidance in
`ti-datasheet.pdf`, pp. 14-15.

## Electrical limits and required connections

- Recommended `VM`: 2.7 to 10.8 V. Absolute maximum at the DRV8833 VM pin is
  11.8 V; this is a damage limit, not an operating target.
- The header pad labelled `VM` feeds the motor rail directly and has no reverse
  polarity protection. Only the optional terminal input labelled `VMotor+`
  passes through the board's polarity-protection MOSFET. All GND points are
  common.
- For 3.3 V control, the relevant guaranteed thresholds are: `AINx` high at
  2.0 V minimum and low at 0.7 V maximum; `SLP` high at 2.5 V minimum and low
  at 0.5 V maximum. Recommended digital-input range is -0.3 to 5.75 V;
  absolute maximum is -0.5 to 7 V.
- `SLP` is required. It has an internal nominal 500 kohm pulldown, so an open
  pin leaves the driver asleep. Allow up to 1 ms after driving it high before
  expecting bridge operation.
- Channel-A minimum wiring for this project is `VM`, `GND`, `SLP`, `AIN1`,
  `AIN2`, `AOUT1`, and `AOUT2`.
- `BIN1` and `BIN2` may remain open because each chip input has an internal
  nominal 150 kohm pulldown. This leaves channel B in coast/high-impedance.
  `BOUT1` and `BOUT2` then remain unused.
- `FLT` may remain open. If used, it is an open-drain active-low fault output
  and requires an external pull-up to the desired logic voltage.
- `ASEN` and `BSEN` may remain open externally. The module already connects
  each sense node to ground through 0.2 ohm and sets nominal chopping at 1 A.
- The module is advertised for motors of about 1.2 A or less per bridge, while
  its default current chopping starts nominally at 1 A. The bare PWP chip's
  1.5 A RMS and 2 A peak ratings do not override the module's current limit or
  thermal constraints. Do not treat overcurrent or thermal shutdown as normal
  current regulation.
- Bridge logic: `00` coast, `01` reverse, `10` forward, `11` brake. PWM may be
  applied to an input; decay mode depends on the state of the other input.
- The driver has internal inductive-current paths and protection for
  undervoltage, overcurrent, and overtemperature. Maximum usable current still
  depends on board temperature, ambient temperature, VM, duty cycle, and
  cooling.

Sources: `adafruit-guide.pdf`, pp. 3-7; `ti-datasheet.pdf`, pp. 1 and 3-7 for
ratings and thresholds, pp. 9-11 for bridge logic, current control, sleep, and
fault behavior, and p. 16 for thermal/current limitations. The board's 0.2 ohm
resistors are also recorded in `adafruit-schematic.sch:5214-5216`; the
schematic's own limit formula is at `adafruit-schematic.sch:5248-5253`.

## Check against wiring/car.yml

No changes were made to `wiring/car.yml`.

- Electrically consistent: `VM` and common `GND` power the module
  (`wiring/car.yml:79-84`); 3.3 V MCU signals drive `AIN1`, `AIN2`, and `SLP`
  (`wiring/car.yml:103-105`); `AOUT1/AOUT2` drive one DC motor
  (`wiring/car.yml:109-111`).
- Safe startup behavior: if the MCU pin is high-impedance during reset, the
  internal `SLP` pulldown keeps the bridges disabled.
- Intentionally unused and valid for one motor: `BIN1`, `BIN2`, `BOUT1`,
  `BOUT2`, `FLT`, `ASEN`, `BSEN`, and the optional terminal block.
- Fabrication risk: `DRIVER.pinlabels` at `wiring/car.yml:25` is a logical
  subset, not physical pin order. It must not be used as a left-to-right
  footprint definition.
- Polarity-protection ambiguity: the `VM` endpoint at `wiring/car.yml:81`
  denotes the unprotected header pad by Adafruit's naming. If the protected
  screw-terminal input was intended, the present file does not say so.
- The YAML contains no driver-rail voltage, motor current, wire length/gauge,
  added bulk capacitance, fastener, or thermal/airflow data. Those limits
  therefore cannot be fully checked from `car.yml` alone.

## Unresolved before perfboard fabrication

- Confirm the board in hand is this exact Adafruit #3297 layout, rather than a
  clone or later PCB revision.
- Decide whether power enters the unprotected `VM` header or a separately
  sourced 3.5 mm terminal block through protected `VMotor+`.
- Measure actual PCB thickness, maximum component height, installed-header
  height, and solder-joint protrusion; Adafruit's published drawing does not
  dimension them.
- Select the M2.5 fastener, spacer, nut/washer, and mounting side before fixing
  the mechanical keepout.
- Establish peak/stall current, supply-wire impedance, voltage excursion under
  braking, and module temperature in the assembled car. These determine
  whether the default 1 A limit and onboard capacitance are adequate.

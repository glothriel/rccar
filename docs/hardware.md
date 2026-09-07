# Hardware revision 0

This document records the selected parts, their physical identification, electrical limits, and rules needed to assemble the car. `wiring/car.yml` is the logical connection source; `firmware/include/pin_config.h` is the GPIO source. Anything marked **VERIFY** must be checked on the actual part before permanent wiring.

## Before first power-on

1. Disconnect the cell and USB before soldering or changing wiring.
2. Confirm every module against the photos and markings in `docs/vendor/`; similar-looking variants can have different pinouts.
3. Inspect the 18650 wrap and ends. Do not use a dented, corroded, leaking, hot, or damaged cell.
4. With no cell installed, check for shorts and map both switches with a continuity meter.
5. Insert the cell, verify polarity at the holder leads, then remove it again before connecting the holder to the charger.
6. Bring up one stage at a time: TP4056 output, switched raw rail, 5 V regulator, XIAO, servo, then motor driver.
7. Perform the first powered motor test with the wheels raised and the steering linkage unloaded. Never deliberately stall the motor or servo.
8. Disconnect power immediately on heat, smell, unstable motion, ESP32 resets, loose contacts, or visible rail sag.

## Devices

### Protected XTAR 18650, 2600 mAh

**Identification and mechanics**

- Protected, button-top 18650; nominal dimensions are about 18.4 x 68.5 mm and mass 47 g. It is longer than an unprotected 65 mm flat-top cell.
- The positive terminal is the raised button. The can under the wrap is tied to the negative terminal; damaged insulation can cause a short.
- **VERIFY:** the exact cell revision and holder are not identified. The holder must accept a protected cell about 69 mm long without crushing its protection PCB.

**Electrical**

- Typical capacity 2600 mAh / 9.62 Wh; minimum listed capacity 2450 mAh.
- Nominal voltage 3.6-3.7 V; charge only with a 4.20 V CC/CV single-cell charger. Listed end-of-discharge voltage is 2.75 V.
- A distributor lists `PCB discharge current` as 4.5 A, but does not define whether this is continuous current or a protection threshold. The actual protection trip/release thresholds and maximum charge current are not documented.

**Use**

- Never solder directly to the cell. Remove it before soldering the holder.
- Verify holder polarity by continuity and voltage; do not trust wire colours. Connect holder positive to TP4056 `B+` and negative to `B-`.
- Do not connect the cell to both TP4056 and the XIAO battery pads. Mechanically retain the cell independently of the holder spring.

### HW-373 v1.2.1 / TP4056 USB-C charger and protection board

**Identification and mechanics**

- The top is marked `HW-373 V1.2.1 4056`; nominal PCB size is 26 x 17 mm. USB-C is on one short edge.
- Top view with USB-C to the left: `IN+` is upper-left, `IN-` lower-left; the right edge is `OUT+`, `B+`, `B-`, `OUT-` from top to bottom.
- No dimensioned drawing, pad pitch, hole diameter, thickness, or keepout is published. Do not infer a 2.54 mm footprint from photographs. Leave access for the USB plug and insulate exposed underside pads.

**Electrical**

- One 3.7 V Li-ion cell only. Input is 5 V through USB-C or `IN+`/`IN-`; charging voltage is 4.2 V +/-1%; seller-stated maximum charge current is 1 A.
- Seller-stated protection values are below 2.5 V for over-discharge and above 3 A for over-current. The 3 A value is not a documented continuous output rating.
- No power-path/load-sharing controller is documented. Charging while the car is operating can prevent correct charge termination and is not an approved mode.

**Use**

- Battery uses `B+`/`B-`; the car uses `OUT+`/`OUT-`. Set the master switch OFF while charging.
- The owned board charges from USB-A to USB-C but did not start from USB-C to USB-C PD. This is an observed board limitation, not a guaranteed property of every HW-373.

### MTS102 / Kamami 564543 SPDT ON-ON switches

Both the master switch and XIAO power switch are the same Kamami listing: SKU `564543`, EAN `5906623456031`, seller designation `MTS102`. The OEM and genuine manufacturer part number are not established.

**Identification and mechanics**

- Three solder lugs in one row; body 7.8 x 13 x 10 mm; M6 threaded bushing 8.8 mm long; 10 mm lever; 33 mm overall height.
- Lugs are listed as 2 mm wide at 4.7 mm spacing. This is a panel-mount wire-terminal switch, not a 2.54 mm perfboard part. Mount both off-board and strain-relieve and insulate every wire and unused lug.
- The listing does not provide a PCB footprint, lug thickness, eyelet diameter, panel-hole diameter, dimensional tolerances, or tool clearance. Measure the actual M6 bushing and panel hardware before drilling.

**Electrical and switching behaviour**

- Maintained SPDT **ON-ON**, not ON-OFF: `COM` connects to one throw in each stable lever position. The apparent OFF position connects `COM` to the unused live lug.
- Marking/rating is 6 A at 125 V AC. No DC, inductive-load, inrush, lifetime, or contact-resistance rating is published; do not reinterpret it as 6 A DC.
- Neither listing nor photographs identify physical `COM`. With all power removed, measure all terminal pairs in both positions; `COM` is the lug common to the two closed pairs. Record which throw corresponds to the desired ON lever direction.

**Use**

- Master: `COM` receives TP4056 `OUT+`; one verified throw feeds the DRV8833 and U3V16F5 raw rail.
- XIAO switch: `COM` receives regulated 5 V; one verified throw feeds XIAO `5V/VBUS`. Ground remains permanently common.
- The unselected throw is electrically live in the opposite position. Leave it disconnected and insulated.
- Because no DC rating is available, suitability for the measured car load remains **VERIFY**. Replace with a switch having an adequate documented DC inductive rating if necessary.

### Pololu U3V16F5 #4941 5 V step-up regulator

**Identification and mechanics**

- Fixed-5 V board, nominally 13.1 x 8.1 x about 3 mm without headers; PCB thickness 1.02 mm. Edge tolerance is +/-0.3 mm and hole-location tolerance +/-0.1 mm.
- Three 1.02 mm plated electrical holes on 2.54 mm pitch; no separate mounting hole. With the long axis vertical and pins at the bottom, bottom/label-side order is `VIN, GND, VOUT`; top/component-side order is mirrored: `VOUT, GND, VIN`.
- Mount by directly soldered wires or a straight/right-angle 1x3 header. Reserve the complete board outline, insulate the underside, and provide airflow. Pololu publishes no perfboard courtyard.

**Electrical**

- Output 5 V +/-4%. Operating input after startup is 1.3-16 V, but startup requires at least 2.7 V. Typical efficiency is 85-95%; no-load current is below 1 mA.
- The 2 A instantaneous switch limit and roughly 1.6 A sustainable **input** current are not output-current ratings. Available 5 V current depends on battery voltage and cooling; the module can become hot enough to burn.
- Thermal shutdown, cycle-by-cycle current limiting, soft-start, and undervoltage lockout are present. Reverse-polarity and short-circuit protection are absent. Input above 5 V passes through to the output.
- For long input leads or measured switching spikes, Pololu suggests 33 uF close to `VIN`/`GND` as a starting point, not as a universally validated value.

**Use**

- `VIN` receives switched TP4056 output; `VOUT` feeds the SG90 and the XIAO switch. The motor must not use this 5 V rail.
- **VERIFY:** measure 5 V under simultaneous XIAO/Wi-Fi and servo load before accepting the power budget.

### Seeed Studio XIAO ESP32-S3 Sense

**Identification and mechanics**

- Main PCB is nominally 21 x 17.8 mm; the Sense stack is listed as 21 x 17.8 x 15 mm. The two seven-pin rows use 2.54 mm pitch and are 15.24 mm, or six perfboard holes, apart.
- Top view, USB-C at the top:

  ```text
  left                         right
  D0/GPIO1                     5V/VBUS
  D1/GPIO2                     GND
  D2/GPIO3                     3V3
  D3/GPIO4                     D10/GPIO9
  D4/GPIO5/SDA                 D9/GPIO8/MISO
  D5/GPIO6/SCL                 D8/GPIO7/SCK
  D6/GPIO43/TX                 D7/GPIO44/RX
  ```

  The solder-side view is the horizontal mirror.
- USB-C is at one short end; the Sense board-to-board connector is at the opposite end. Keep access to USB-C, camera flex/field of view, U.FL connector, antenna cable, and Sense connector. No numeric RF or camera keepout is published.
- The underside also carries battery/debug pads and a thermal pad. They are not part of the two perfboard rows and must not contact perfboard copper.

**Electrical and use**

- GPIO is 3.3 V. This car uses D0/GPIO1, D1/GPIO2, D2/GPIO3, and D3/GPIO4.
- Do not connect the cell to the XIAO battery pads. Do not blindly combine external 5 V and USB on `5V/VBUS`.
- Before USB programming, turn both switches OFF. The XIAO switch interrupts only external +5 V; common ground remains connected.
- **VERIFY:** Seeed's wiki assigns SD chip-select to D2/GPIO3, while the vendored Sense v1.0 schematic shows the D2 path as DNP and GPIO21 selected. Check the owned Sense revision and R11/R12 population before using D2 for servo PWM with the SD card.

### Adafruit DRV8833 #3297 motor driver

**Identification and mechanics**

- PCB is 25.40 x 17.78 mm. Signal/output pads use 1.00 mm drills on 2.54 mm pitch; row centres are 12.70 mm, or five perfboard holes, apart.
- Top/component view with the eight-pad row at the top:

  ```text
  AIN1 AIN2 SLP BIN2 BIN1 FLT GND VM
       AOUT1 AOUT2 BOUT2 BOUT1 ASEN BSEN
  ```

  The bottom view is the horizontal mirror. The six lower pads align under `AIN2`, `SLP`, `BIN2`, `BIN1`, `FLT`, and `GND`.
- Two 2.50 mm mounting holes are 20.32 mm apart, but offset half a perfboard pitch from the pin grid. The optional motor terminal footprint is 3.50 mm pitch and is not fitted on product #3297.
- Reserve the full board outline and at least 4.03 mm diameter around each mounting-hole centre; final screw/washer keepout depends on selected hardware. Keep motor and VM wiring short and away from signal wiring.

**Electrical**

- Recommended `VM` is 2.7-10.8 V. The header `VM` pad is unprotected; only optional terminal input `VMotor+` passes through reverse-polarity protection.
- 3.3 V control is valid. `SLP` is required and has an internal pulldown; allow up to 1 ms after driving it high.
- The breakout sets nominal current chopping to 1 A per bridge through 0.2 ohm sense resistors. Do not close the underside `ISENSE A/B` jumpers; that bypasses the limit.
- For channel A this car uses `VM`, `GND`, `SLP`, `AIN1`, `AIN2`, `AOUT1`, and `AOUT2`. `BIN1`, `BIN2`, `BOUT1`, `BOUT2`, `FLT`, `ASEN`, and `BSEN` remain intentionally open.
- The motor's documented 1.1-1.5 A stall current reaches or exceeds the breakout's nominal 1 A chopping level. Over-current and thermal shutdown are fault protection, not normal regulation.

**Use**

- `VM` receives the switched raw battery rail; AOUT1/AOUT2 feed only the motor. The current layout does not use protected `VMotor+`.
- **VERIFY:** module identity, temperature, supply sag, and behaviour at motor start/stall before enclosing the assembly.

### DNG-16016 1:48 drive motor

**Identification and mechanics**

- Brushed, bidirectional 3-6 V DC gearmotor, about 200 RPM at 6 V, no encoder. Approximate overall envelope is 70 x 22.5 x 36.6 mm including protrusions; vendor dimensions are not fully consistent.
- Two bare solder lugs are provided; no factory leads or connector. Lug dimensions and spacing are not published. Solder flexible wires to the motor, add strain relief, and connect it as an external device rather than placing it on perfboard.

**Electrical and use**

- No-load current is about 150/155/160 mA at 3/4.5/6 V. Listed stall current is about 1.1/1.2/1.5 A at those voltages.
- No terminal polarity or viewing direction is specified. First test at low voltage, then mark M1/M2 for forward motion or invert it in firmware.
- No suppression component is specified. If interference occurs, establish suppression by measurement and place it close to the motor terminals; do not invent a capacitor value.

### TowerPro SG90 steering servo

**Identification and connection**

- External analog micro-servo with a 25 cm, three-wire female JR/Futaba-compatible connector on 2.54 mm pitch.
- On this car: brown is ground, red is supply, yellow is signal. The usual documentation drawing uses orange for signal; verify the owned connector before permanent assembly.
- Use a keyed three-pin board connector or clearly mark `GND / +5V / SIGNAL`. An unkeyed header allows reversal, swapping ground and signal while red remains in the centre.

**Electrical and use**

- TowerPro officially specifies 4.8 V; this car supplies regulated 5 V. Published TowerPro documentation does not state normal, peak, or stall current and does not prescribe a bulk capacitor.
- Power the servo from the regulator, never from a GPIO. Signal is XIAO D2/GPIO3. Verified mechanical command limits are 10 degrees left, 90 degrees centre, and 170 degrees right.
- **VERIFY:** current and 5 V rail sag at the actual steering end stops. Do not command the linkage against a hard stop.

### Perfboard

- Owned board: isolated-pad perfboard, 24 columns `a-X` by 18 rows `1-18`, nominal 2.54 mm pitch.
- Boardwright coordinates are zero-based: `a1` is `[0,0]`, `X18` is `[23,17]`.
- XIAO, DRV8833, and U3V16F5 pin grids match 2.54 mm. HW-373 has no documented footprint, and both switches are panel-mount parts with 4.7 mm lug spacing; those parts require insulated wires or measured custom mounting rather than forced insertion.
- Motor, battery holder, servo, and both switches remain external. Use strain relief and connectors rated for the measured load; do not use Dupont jumpers for power paths.

## Connection architecture

```text
18650 -> TP4056 B+/B-
TP4056 OUT+ -> master switch ->+-> DRV8833 VM -> motor
                               +-> U3V16F5 VIN -> 5 V -> SG90
                                                     -> XIAO switch -> XIAO 5V/VBUS
TP4056 OUT- --------------------- common ground for every module
```

- `wiring/car.yml` defines the complete logical nets; connector pin lists there are not physical left-to-right footprints.
- D0/GPIO1 -> DRV8833 AIN1; D1/GPIO2 -> AIN2; D3/GPIO4 -> SLP; D2/GPIO3 -> SG90 signal.
- Software convention is positive drive = forward. **VERIFY:** establish physical wheel direction, then document whether motor leads or firmware polarity were inverted.
- The separate XIAO switch is a manual barrier against USB and external 5 V driving `5V/VBUS` together. A Schottky power-path diode remains only a possible future change; no owned part or loaded voltage drop is confirmed.

## Bring-up and diagnostics

- Master and XIAO switches must be OFF while charging or connecting XIAO USB.
- Check each ON-ON switch without power: selected throw conducts to COM; unused throw must be insulated even though it becomes live in the other lever position.
- Before connecting XIAO or SG90, verify regulator output polarity and 4.8-5.2 V range under a representative load.
- If the ESP32 resets during movement, measure the 5 V rail under load, verify common-ground wiring, and shorten or increase the gauge of power wiring before considering a larger regulator. Do not mask a power-integrity fault in software.
- Measure motor-start and blocked-steering transients; documentation alone does not establish the combined peak-current budget.

## Vendored sources

- `docs/vendor/xiao-esp32s3/`: Seeed wiki, schematics, and pin data.
- `docs/vendor/drv8833/README.md`: Adafruit #3297 guide, TI datasheet, fabrication, and pinout TLDR.
- `docs/vendor/u3v16f5/README.md`: Pololu #4941 product page, dimensions, current graph, and photos.
- `docs/vendor/tp4056-hw373/`: HW-373 product page, photographs, safety sheet, and TP4056 datasheet.
- `docs/vendor/sg90/`: TowerPro product information and connector reference.
- `docs/vendor/motor-dng-16016/`: product page, photographs, and motor drawing.
- `docs/vendor/mts102-kamami-564543/README.md`: exact switch listing, photographs, dimensions, and documented limitations.

# Kamami 564543 SPDT toggle switch source set

Scope: the exact Kamami listing ID `564543`, EAN-13 `5906623456031`, selected
for both switches in this car. Retrieved from Kamami on 2026-09-04.

## Vendored sources

| File | Upstream source | Purpose |
|---|---|---|
| `kamami-product.html` | https://kamami.pl/przelaczniki-dzwigniowe/564543-przelacznik-dzwigniowy-spdt-on-on-6a125vac-5906623456031.html | Product identity, seller specifications, dimensions, and safety-party data |
| `product-29400.jpg` | https://kamami.pl/29400-large_default/przelacznik-dzwigniowy-spdt-on-on-6a125vac.jpg | Product photograph; three terminals and `ON ON 6A125VAC` case marking |
| `product-29401.jpg` | https://kamami.pl/29401-large_default/przelacznik-dzwigniowy-spdt-on-on-6a125vac.jpg | Product photograph; terminals and panel hardware |
| `product-29402.jpg` | https://kamami.pl/29402-large_default/przelacznik-dzwigniowy-spdt-on-on-6a125vac.jpg | Product photograph; three terminals and case marking |

The product page exposes no attachment or document link. Consequently there
was no directly linked manufacturer datasheet to vendor. The three photographs
above are the full-size product images directly enumerated by the page.

## Identity

- Seller name: `Przelacznik dzwigniowy SPDT (ON-ON) 6A/125VAC`.
- Kamami listing/SKU: `564543`; EAN-13: `5906623456031`.
- The prose ends with `MTS102`, so this is the seller's model designation.
  It is not printed on the photographed switch, and the page's machine-readable
  MPN is merely `564543`, the same as Kamami's own SKU. The page therefore does
  **not** establish a genuine OEM part number.
- The page's structured brand is `Inny` (other/unspecified). Its product-safety
  section names BTC Korporacja sp. z o.o. as both `Producent` and responsible
  person. With no maker mark or manufacturer datasheet, the actual factory/OEM
  remains unverified; do not attribute this part to a similarly named MTS-102
  from another manufacturer.

Sources: `kamami-product.html:102-109`, `kamami-product.html:2305-2308`, and
`kamami-product.html:2613-2637`.

## Function and terminals

- SPDT, maintained `ON-ON`: one common contact is connected to one throw in
  each stable lever position. There is no dedicated open/OFF lever position.
- There are exactly three physical solder-lug terminals in one row. Kamami
  describes them as 2 mm wide, eyelet-style terminals for passing wires
  through, with 4.7 mm terminal spacing.
- Neither the page nor the photographs number the terminals or identify which
  physical terminal is `COM`. Do not encode or solder a presumed physical pin
  order. With all power disconnected, use a continuity meter in both lever
  positions: the terminal present in both closed pairs is `COM`; the other two
  are the throws. Also record which throw is selected in each lever position.
- To use this ON-ON part as an apparent ON/OFF switch, connect `COM` and one
  verified throw and leave the other throw insulated. The apparent OFF
  position then connects `COM` to that unused terminal; it is not an internal
  OFF state.

Sources: `kamami-product.html:2538-2545`, `kamami-product.html:2554-2555`, and
the three vendored product photographs.

## Mechanical data

All dimensions below are nominal seller values; no drawing, datum, or tolerance
is supplied.

| Item | Published value |
|---|---:|
| Body width | 7.8 mm |
| Body length | 13 mm |
| Body height | 10 mm |
| Panel bushing thread | M6 |
| Threaded length | 8.8 mm |
| Lever length | 10 mm |
| Overall height | 33 mm |
| Terminal width | 2 mm |
| Terminal spacing | 4.7 mm |

This is a panel-mount, wire-terminal part, not a documented PCB-mount part.
Kamami supplies no PCB footprint, terminal thickness, eyelet-hole diameter,
spacing datum/tolerance, or recommended land/hole sizes. The stated 4.7 mm is
not identified explicitly as center-to-center and is not a 2.54 mm perfboard
pitch.

The required panel-hole diameter is also **not published**. `M6` identifies the
bushing thread, not a guaranteed drill size or clearance fit. Measure the actual
switch and account for the locating tab on the smooth washer visible in
`product-29401.jpg`, panel thickness, nut/washer envelope, and tool clearance
before drilling. The text lists a toothed lock washer and smooth washer in the
set but does not list the nut visible in the photograph, so verify supplied
hardware before fabrication.

Source: `kamami-product.html:2545-2563`.

## Electrical limits and warnings

- Published contact rating: maximum 6 A at 125 V AC. The case photographs are
  marked `6A125VAC`.
- No DC voltage/current rating, load category, inductive-load rating, inrush
  rating, contact resistance, lifetime, temperature range, insulation rating,
  or approvals are published.
- Do not reinterpret the 6 A / 125 V AC marking as a 6 A DC rating. DC is
  harder to interrupt because it has no periodic current zero, and the car's
  motor/driver supply can be inductive and have startup or transient current.
  Suitability for switching the car's DC rails is therefore not established
  by this listing.
- Do not use the switch for mains based only on this marketplace listing: no
  insulation system, approval, spacing, enclosure, or touch-safety data is
  available.
- Insulate the unused live throw and all solder lugs. Verify continuity only
  while fully de-energized, and provide strain relief so wire force is not
  carried by the lugs.

Source: `kamami-product.html:2538-2545`; absent values were checked against the
complete vendored product page.

## Safe WireViz representation

No change to `wiring/car.yml` was made. It is safe to record only logical,
continuity-verified terminals, for example:

```yaml
type: MTS102 (Kamami 564543) SPDT ON-ON toggle
pinlabels: [COM, SELECTED_THROW, UNUSED_THROW]
notes: No OFF position; map physical terminals by continuity. No published DC rating.
```

The existing `[COM, "ON", UNUSED]` labels are acceptable as logical net names
if `ON` means the throw selected for use, not a claimed marking or physical pin
position. It is not safe to add a physical pin number/order, `COM=center`, a
panel-hole diameter, a PCB footprint, or a DC current rating from the available
evidence. Resolve the DC rating with the seller/manufacturer or replace the part
with a switch having a documented DC rating suitable for the measured load.

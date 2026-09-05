# WAYKEEPER Technical Workshop

Checked 2026-08-16. This pack adds a staged electronics and vehicle-maintenance
curriculum to `F2 > TECHNICAL WORKSHOP`.

## Electronics

- Basics: NEETS modules 1-5 — DC, AC, protection, wiring, schematics, motors,
  and generators.
- Intermediate: NEETS modules 6-16 — power supplies, solid state, amplifiers,
  waveforms, RF, microwave, digital logic, microelectronics, servos, and test
  equipment.
- Advanced: NEETS modules 17-24 — RF communications, radar, test methods,
  computers, recording, fiber optics, and the technician handbook/glossary.

The Navy Electricity and Electronics Training Series is older public-release
training material. It is useful for durable fundamentals, but current component
data sheets, electrical codes, equipment manuals, and safety procedures control.

## Automotive and heavy equipment

- Basics: shop practice, occupational safety, and automotive system principles.
- Intermediate: wheeled-vehicle electrical systems, equipment operation, tire
  service, and brake/clutch asbestos safety.
- Advanced: hydraulics and pneumatics, heavy equipment, construction mechanics,
  aerial lifts, and bucket trucks.

The automotive shelf spans cars, SUVs, light trucks, heavy/construction
equipment, and bucket/aerial-lift systems. General military training documents
do not replace the exact year/make/model service manual. Never infer torque
specifications, lift points, fluid compatibility, load charts, wiring, or safety
procedures from a generic manual.

## Safety boundary

De-energize and verify electrical systems. Chock vehicles, use rated stands and
lifting equipment, relieve stored pressure, control hazardous dust, and follow
current manufacturer and regulatory instructions. High voltage, batteries,
capacitors, rotating machinery, hydraulics, tires, suspended loads, and elevated
work can kill.

## Rebuilding the pack

Run:

```sh
./scripts/download_technical_library.sh
```

The importer validates PDF signatures and page counts, extracts searchable text,
records SHA-256 checksums in `technical-provenance.tsv`, and updates
`catalog.tsv` atomically. Source definitions and safety notes live in
`technical-sources.tsv`.


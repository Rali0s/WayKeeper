# USB-C UART — later-release design note

Status: documentation only. No current image enables USB gadget serial or repurposes the power connector.

The Orange Pi Zero 2's USB-C connector is the 5 V power input and should not be assumed to expose a safe UART. The current board exposes a separate 3.3 V TTL debug UART on its header. A future enclosure may route that UART to a dedicated USB-C receptacle only through an explicit USB-to-UART bridge and ESD/power-path design.

## Required electrical rules

- Never connect H616 3.3 V UART pins directly to USB D+/D− or CC pins.
- Use a bridge IC that presents a standards-compliant USB CDC ACM device.
- Keep the board UART at 3.3 V logic; do not feed 5 V TTL into the SoC.
- Add ESD protection, correct USB-C CC resistors, and prevent VBUS back-powering.
- Separate the data receptacle from the power-only port unless the production schematic proves safe role and power negotiation.
- Validate ground, connector shield, cable orientation, suspend/resume, brownout, and boot-ROM behavior on hardware.

## Proposed software contract

- Device name: `/dev/ttyWK0` via a stable udev rule.
- Default console: 115200 8N1.
- Kernel console remains on the board debug UART until the bridge has passed recovery testing.
- Production mode exposes a login only after an explicit local setting; factory/recovery images may enable it by profile.
- Disable root password login. Log connection state locally and rate-limit authentication.

## Release gate

Do not enable this feature until a reviewed schematic, prototype measurements, USB compliance check, threat model, and recovery procedure exist. This note is intentionally not executable configuration.


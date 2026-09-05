# Contributing

WayKeeper is hosted at https://github.com/Rali0s/WayKeeper. Submit changes through pull requests after running the checks below.

## Required checks

```sh
cmake -S Fallout-Assistant -B Fallout-Assistant/build/dev -G Ninja
cmake --build Fallout-Assistant/build/dev
ctest --test-dir Fallout-Assistant/build/dev --output-on-failure
bash -n Fallout-Assistant/platform/scripts/*.sh
bash -n Fallout-Assistant/platform/scripts/internal/*.sh
```

Preserve plain-text behavior and ANSI/VT behavior together. Do not introduce a desktop shell, cloud dependency, telemetry upload, mandatory model, or unreviewed safety answer. Do not commit operator state, generated OS images, downloaded corpora, secrets, or build trees.

Changes to bootloader, firmware, partition layout, UART, power, or thermal behavior require an Orange Pi Zero 2 hardware test and captured serial log in addition to QEMU testing.

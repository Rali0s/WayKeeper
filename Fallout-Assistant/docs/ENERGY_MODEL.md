# Energy-Aware Question Routing

The meaningful unit is energy per useful, sourced answer—not token count alone.

For a calibrated inference profile:

```text
runtime_seconds = fixed_seconds + total_tokens / tokens_per_second
query_Wh = measured_system_watts * runtime_seconds / 3600
mWh_per_token = query_Wh * 1000 / total_tokens
solar_payback_minutes = query_Wh / net_solar_watts * 60
```

`net_solar_watts` is panel output after controller/conversion losses and simultaneous system load. If it is zero, solar payback is unknown/infinite.

## Router order

1. Reviewed static card: answer immediately.
2. Exact manual search: show the relevant page without inference.
3. Small model: synthesize retrieved passages when energy above reserve permits.
4. Larger model: only with measured solar surplus and ample stored energy.
5. Defer: tell the operator why, show the estimated cost, and offer the card/manual route.

The UI should answer both operational questions:

- **How expensive is this question?** Show projected Wh, percent of usable battery, estimated runtime, and solar payback before generation when reserve is tight.
- **Should I use the card instead?** Recommend it whenever a reviewed card or exact passage exists, or when inference would cross reserve.

## Calibration record

Do not ship invented performance constants. Each device profile records:

- computer and accelerator;
- model/GGUF quantization and checksum;
- context, prompt, and output lengths;
- runtime version, threads, batch, and GPU layers;
- idle and inference watts at the battery bus;
- tokens/second, wall-clock time, temperature, fan state, and screen state;
- battery voltage/state-of-charge estimate and solar/controller output.

Median and conservative 90th-percentile measurements should drive routing. Recalibrate after runtime, model, cooling, storage, or power-converter changes.


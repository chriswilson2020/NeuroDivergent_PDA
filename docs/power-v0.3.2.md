# Power management v0.3.2

Stable firmware, 2026-10-10.

## Light sleep

The default `tlora_pager` firmware enables ESP32-S3 light sleep while unplugged
and screen-off. The maximum maintenance interval is 30 seconds. Countdown,
daily alarm, calendar reminder, task reminder and snooze deadlines shorten
sleep when necessary. Existing task reminders occur at 09:00 on their due date.

The SX1262 stays in receive mode. Radio DIO1, keyboard, RTC and wheel inputs
can wake the processor before the maintenance timer. Pending transmissions,
ACKs, pairing, queued messages and active input prevent sleep until serviced.
Wake interrupt configuration is restored before normal handling resumes.
LVGL timing uses the sleep-compensated monotonic clock.

USB power, active serial and USB Disk Mode inhibit processor sleep. Battery
status, low-battery warnings and USB insertion detection can wait up to roughly
30 seconds plus processing time while asleep. Press a key or the wheel to wake
immediately after plugging in. Hardware charging continues independently.

## Logs

With a mounted microSD card, `/PocketPDA/logs/power.csv` records battery,
display, radio and processor sleep counters approximately once per minute.
`/PocketPDA/logs/sleep.csv` records sleep transitions and periodic diagnostics.
Each rotates at 1 MiB, retaining one previous file. USB Disk Mode pauses card
access by firmware.

The existing `experimental` CSV column and `POCKETPDA_EXPERIMENTAL_LIGHT_SLEEP`
build flag retain their names for compatibility; their value now indicates
whether processor sleep is enabled, not the release classification.

## Builds

```
pio run -e tlora_pager
```

`tlora_pager_sleep_test` remains a compatibility alias with the same 30-second
sleep behavior. For display-only troubleshooting without processor sleep:

```
pio run -e tlora_pager_awake
```

The GPIO and transition implementation is described in
[light-sleep architecture](experimental-light-sleep.md).

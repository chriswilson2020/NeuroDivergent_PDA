# GNSS time synchronisation — v0.3.3

## Use

Settings > TIME SYNC offers Auto On/Off, 1/4/8/12/24-hour intervals,
Europe/Amsterdam or UTC, SYNC NOW, CANCEL, and SAVE. Manual acquisition is
limited to 180 seconds; automatic acquisition to 60 seconds. Take the pager
outside with an unobstructed sky for an initial acquisition. Indoor failures
are quiet background outcomes, not recurring notification popups.

Automatic checks default to four hours after the last **attempt**, successful
or not. Eligibility is opportunistic: only while the display is awake or dimmed,
never an RTC wake scheduled just for GNSS. Below 20% or with an unavailable
battery reading automatic searches defer. Manual searches require at least 5%.
The elapsed/search status, last attempt, last success, signed drift and remaining
eligibility delay are shown on the screen. A previously valid RTC continues to
run if no satellites are available.

The existing stable 30-second processor light-sleep behavior is retained, as
requested. GNSS is cancelled when the display enters its sleep state and before
USB Disk Mode, shutdown, manual clock edits, backup or restore. No radio power,
receive configuration, packet format, frequency, ACK or charger settings change.

## Hardware and protocol

The target is the MIA-M10Q on the pinned LilyGoLib board implementation:

| Signal | Assignment |
| --- | --- |
| ESP UART receive | GPIO4 |
| ESP UART transmit | GPIO12 |
| PPS (not used for clock discipline) | GPIO13 |
| GNSS enable | XL9555 expander pin 4, I2C address 0x20 |
| GNSS reset | XL9555 expander pin 7, not ESP GPIO7 |
| RTC | PCF85063, I2C address 0x51 |

LilyGoLib's `initGPS()` is deliberately not used: it starts a separate probing/
reader path. PocketPDA owns Serial1 and polls UBX-NAV-TIMEUTC once a second.
It begins at the library's 38400 baud, trying 9600 after four seconds without a
checksum-valid UBX response. Parsing is capped at 256 bytes per application pass.
GNSS UART/PPS pins become inputs after power-off to avoid signal back-powering.

The parser checks UBX length/checksum, exact Gregorian date, validTOW, validWKN,
validUTC, a known UTC standard and estimated accuracy <=100 ms. Leap-second
values are deferred rather than normalized. A confirmed NAV-PVT time-only fix
can also be accepted; unsupported PVT confirmation fields are not assumed to
exist on M10 firmware. NMEA alone is never accepted, and coordinates are not
used or logged. This follows the receiver's
[M10 interface description](https://content.u-blox.com/sites/default/files/documents/u-blox-M10-SPG-5.30_InterfaceDescription_UBXDOC-304424225-20395.pdf).

Three progressing observations must agree in UTC, GPS time-of-week and elapsed
processor time. Duplicate epochs do not advance validation. Unknown RTC time or
drift exceeding five minutes requires six observations. RTC comparison/write
occurs immediately after validation, accounting for elapsed processing time.
Drift is RTC minus GNSS; correction occurs only above two whole seconds. The
RTC interface has one-second resolution: this is not a subsecond clock service
or a defense against deliberate GNSS spoofing.

Normal success/failure paths end UART, cut the enable rail and check expander
readback. Failed power-off readback is retried and vetoes processor sleep.
An independent ESP timer safety lease resets the processor if GNSS has not been
released within acquisition timeout + five seconds. The callback performs no
concurrent I2C operations; boot explicitly cuts GNSS before UI initialization.
The lease protects against a stalled application loop. It is not a separate
electrical watchdog and cannot guarantee rail shutdown if the whole MCU or I2C
hardware fails. Electrical rail-off and actual acquisition energy still need
measurement on the physical board.

## UTC and existing data

Previous firmware stored local civil time in RTC registers but used libc without
an explicit timezone. This version explicitly configures Amsterdam's CET/CEST
rules and stores UTC in the RTC, converting to local civil time at its interface.
UTC storage is necessary so an offline RTC can cross DST without satellites or
a processor wake just to change its stored offset. Calendar/task records keep
their existing local date/time fields; they are not rewritten or shifted.

Migration is versioned in device NVS (`pda-clock/format`): explicit 0 = old local
storage, 1 = migration pending, 2 = verified UTC storage. Missing/unknown markers
are untrusted, not automatically treated as legacy local time: a factory-image
flash can erase NVS while the battery-backed RTC retains UTC. Interpreting that
UTC as local time repeatedly subtracts two hours in summer. Routine updates must
preserve NVS (`tools/flash_update.py` writes only the application at `0x10000`).
Before changing an explicitly labelled legacy
RTC, firmware commits the pending marker, writes converted UTC and verifies
readback, then commits version 2. If power is interrupted in that transaction,
the next boot does **not** guess its storage basis or subtract the offset twice:
the RTC is considered untrusted until a verified manual or GNSS set recovers it.
An oscillator-stop flag, invalid BCD/calendar date or I2C error also prevents
trusting the RTC. Build time is a display fallback only, not a trusted elapsed
interval or a reason to overwrite a valid clock. The status bar shows `SET CLOCK`
rather than presenting that fallback as a correct time. On an unlabelled old
installation, set the current local date/time once or complete a GNSS sync.

Old countdown files (`TIR1`) are read as local-civil-shaped epochs and saved
atomically as UTC `TIR2`; timer presets and calendar formats are unchanged.
Existing backups can still be restored. Downgrading to old firmware is not
transparent: old releases interpret raw UTC registers as local time and do not
understand TIR2. Set local time manually on an old release; before returning to
this release, set the time manually again to establish verified UTC storage.
Legacy firmware did not record whether its plausible RTC time came from a user
or build-time fallback, so that provenance cannot be recovered retroactively.

Daily scheduled times in the autumn repeated hour use the first occurrence.
Nonexistent spring times move to the first following valid minute (02:30 becomes
03:00). Manual entry rejects a nonexistent time instead of silently changing it.
No second alarm is delivered in the repeated autumn hour.

## Reminders and clock corrections

Countdowns and snoozes use monotonic elapsed time; an RTC correction rebases
their wall-clock wake deadline without changing their remaining duration.
After a correction forward, wall-clock alarms/reminders crossed within the last
24 hours are caught up. For larger jumps the user sees a summary that older
occurrences are not replayed. Multiple same-minute items are independent.
Timezone changes preserve appointment civil times, not a permanent UTC offset.

An NVS ledger records each persistent item ID and its latest occurrence timestamp
before delivery. Calendar occurrence identity uses lesson start, not the advance
reminder offset. Backward corrections and reboots do not replay recorded
occurrences. Notification queue backpressure retries pending items instead of
marking dropped notifications delivered; storage failures are surfaced and
retried. The ledger is device-specific, not copied to another pager in backups.
As with other nontransactional UI notifications, a reset precisely between the
ledger write and rendering can interrupt presentation; it cannot provide an
exactly-once durable notification queue across arbitrary power loss.

## Persistence and diagnostics

Portable settings live separately from acquisition history. Backups contain
the validated eight-byte `@timesync` preferences entry; old archives without it
preserve the destination pager's preferences. The macOS companion accepts this
entry. Last attempts, results, success times, drift, RTC format and delivery
ledgers remain local to each device.

Attempt metadata is committed before GNSS power-on. Within a boot the interval
uses monotonic time. Across reboot, a trusted RTC determines the remaining
interval; without trustworthy elapsed time a previous attempt causes a full
interval of deferral from boot, not an immediate repeated search.

`POCKETPDA_GNSS_DIAGNOSTICS=1` enables compact Serial diagnostics and
`/PocketPDA/logs/gnss.csv` (256 KiB rotation, one previous file). It records UTC
attempt/success epochs, result, drift, duration, validation counts and battery
readings—never coordinates. Existing power/sleep logs remain unchanged.

## Verification

GNSS enable readback uses checked XL9555 configuration, output-latch and pin-level
register reads. SensorLib's `digitalRead()` explicitly rejects OUTPUT pins and
must not be used for GNSS_EN. This checks the enable signal, not the receiver's
supply voltage. Startup errors distinguish power-controller/readback, safety
timer, battery, history storage and already-active conditions.

Run the output-pin regression test with:
`c++ -std=c++17 -Isrc tests/test_gnss_power.cpp -o /tmp/pocketpda-gnss-power-test && /tmp/pocketpda-gnss-power-test`.

Native tests cover the actual acquisition engine and packet parser, UTC/local
conversion, midnight and DST boundaries, invalid/checksum/stale data, drift
thresholds, six-sample large corrections, timeout/cancel/low battery/USB/display
sleep, reboot cooldown, RTC readback errors, migration and interrupted migration.
Timer/reminder regressions cover independent simultaneous occurrences, forward
catch-up, backward deduplication, countdown elapsed time and queue backpressure.
The actual LVGL 9.6 input loop is tested for short-click edit exit, long-wheel
launcher entry, and physical Back versus text deletion.

Physical validation procedure:

1. Set the pager about ten seconds fast. Outdoors, open TIME SYNC and SYNC NOW;
   observe search progress, a successful correction and signed positive drift.
2. Repeat with a one-second offset: expect a check without correction. Cancel
   another search, and run an indoor timeout; GNSS must show OFF afterward.
3. During a search exchange messages in both directions with the other pager;
   confirm ACK/delivered state, keyboard response and a simultaneous timer.
4. Start another search and enter USB Disk Mode or allow display sleep; verify
   cancellation and subsequent normal light-sleep, messaging and input wake.
5. Observe two alarms plus calendar/task reminders at the same minute. Repeat
   after backward and forward manual clock corrections and while snoozed.
6. Measure the GNSS supply electrically between searches and compare logged
   battery consumption over equivalent awake/sleep use with Auto On and Off.

These are hardware procedures, not claims that physical tests have been run.

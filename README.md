# NeuroDivergent PDA

Firmware v0.3.3 retains deadline-aware light sleep with a 30-second
maximum maintenance interval and persistent power logs.
See [power management and usage](docs/power-v0.3.2.md).

Version v0.3.3 adds automatic GNSS clock checks, Amsterdam daylight-saving
handling, Settings > TIME SYNC, and a long-wheel launcher shortcut. Companion
v0.1.5 adds Mac clock sync and direct, verified backups without an eject/reconnect cycle.
See [GNSS time synchronisation](docs/gnss-time-sync.md).

Phone-free personal organization firmware for the [LILYGO T-LoRa Pager](https://lilygo.cc/products/t-lora-pager), inspired by the practical strengths of classic Palm and Psion handhelds.

This project is for people who benefit from a dependable external memory aid but do not want to carry—or be pulled into—a smartphone. It keeps the useful parts of a phone-sized organizer while leaving out social media, feeds, notifications from other people, advertising, and attention-driven apps.

## Why a dedicated PDA?

Neurodivergent people are not one group with one set of needs. For some people, however, a small single-purpose device can help with:

- **Time blindness:** the Today screen, dated tasks, lesson times, and advance reminders make upcoming transitions visible.
- **Working-memory load:** appointments, tasks, and notes live in one predictable place instead of needing to be remembered.
- **Distraction control:** there is no browser, social feed, messaging stream, or app store competing for attention.
- **Predictability:** tactile controls, consistent screens, and local data avoid changing cloud interfaces and surprise notifications.
- **Lower sensory and social pressure:** alerts are simple and intentional, using a screen and vibration rather than a noisy phone environment.
- **Independence and privacy:** core features work offline and personal data stays on the microSD card.

This is an assistive organizer, not a medical device, treatment, or substitute for professional support. The best tools are personal; the firmware is designed to be adapted.

## Current features

- **Today:** current date, upcoming calendar events, and task summary
- **Transition:** a large live countdown to the next event, with Pack Up and Leave Now phases
- **Quick Capture:** save a thought immediately as a task or note
- **Calendar:** day agenda, create/edit/delete, reminders, flexible recurrence, and bulk CSV import
- **Tasks:** priorities, due dates, optional 09:00 reminders, and daily, weekday, weekly, or monthly recurrence
- **Assignment planner:** break homework into up to six small actions, estimate effort, prioritize it, and see one clear next step
- **Packing lists:** automatically match reusable checklists to the next lesson and reset them for each occurrence
- **Routines:** reusable, one-step-at-a-time checklists for predictable transitions and daily activities
- **Notes:** compact note list and editor with separate note-body storage
- **Timers & Alarms:** editable named countdown presets and daily alarms with gentle, focus, or urgent vibration patterns
- **Habits + Pet:** editable daily habits, streaks, and an animated companion that reacts to progress
- **Messages:** encrypted, phone-free LoRa conversations between explicitly paired PocketPDAs, with delivery acknowledgements, retries, unread counts, and notifications
- **Files:** microSD folder browser, text preview, and deletion
- **Settings:** clock, brightness, display timeouts, storage status, haptic test, USB Disk Mode, and shutdown
- **Offline computer editor:** build calendar, task, and routine CSV files without an account or internet connection
- **macOS companion:** automatically detect PocketPDA in USB Disk Mode, synchronize selected macOS/Outlook calendars, set its clock, create direct verified backups, stage verified restores, and safely eject the device
- **Portable backup:** one checksummed file for calendar, tasks, routines, habits, notes, files, and device settings
- Persistent status bar with charging state, launcher, haptic notifications, low-battery warnings, and dim/display-off power states; display and keyboard illumination shut down together while alerts remain active
- Versioned, checksummed microSD data and internal nonvolatile settings

Calendar events stay on the card. Only the current and following calendar week are cached in PSRAM; the on-disk format supports up to 65,535 events.

## Habits and Mochi

Open **HABITS + PET** from the launcher. Press a habit to mark it complete for today; press it again to undo. The `...` button renames or deletes that habit, and **+ HABIT** creates another one. Up to eight daily habits are supported.

Mochi reacts immediately to progress: calm at the start of the day, proud after some progress, and golden, bouncing, and surrounded by a floating heart when everything is complete. If the evening arrives with nothing checked, Mochi becomes blue and droopy—but is always ready to cheer up again. Consecutive-day streaks appear beside each habit, and all progress is saved on the microSD card.

## Hardware

- LILYGO T-LoRa Pager
- FAT32-formatted microSD card
- USB-C data cable for flashing and USB Disk Mode

The card is required for Calendar, Tasks, Notes, and Files. Insert or remove it only while the Pager is off. PocketPDA creates its directory structure on first boot.

## Build and flash

The local virtual environment avoids the externally-managed Python restriction used by current Homebrew Python installations:

```sh
python3 -m venv .venv
source .venv/bin/activate
python3 -m pip install platformio
pio run
pio run -t upload
```

For subsequent builds:

```sh
source .venv/bin/activate
pio run -t upload
```

Keep the Pager on and connected with a USB data cable. If upload does not start, hold **BOOT**, tap **RESET**, release **BOOT**, and retry. The build pins its LilyGoLib revision and uses the pioarduino Arduino-ESP32 3.x platform.

For an explicit application-only update (using a Python environment with esptool):

```sh
python tools/flash_update.py --port /dev/cu.usbmodemYOUR_DEVICE
```

Do **not** update an existing pager by writing `firmware.factory.bin` at `0x0`.
The merged image contains padding over NVS at `0x9000–0xDFFF`, erasing settings,
GNSS history and the RTC's UTC-format marker. Use the application-only updater
above or PlatformIO's segmented upload instead. An unknown RTC basis now requires
a verified manual time set or GNSS sync; firmware will not guess and subtract
another timezone offset. Organizer files on microSD are separate from NVS.

## Controls

- Rotate the wheel to move focus or scroll.
- Press the wheel to activate the focused control or enter/leave text-editing mode.
- In a text field, the red border means editing: turn the wheel to move the cursor, then click it again to return to field navigation.
- Hold the wheel button for 0.7 seconds to open the launcher from any app.
- The physical Back key deletes inside an input field; outside an input it returns to the launcher or dismisses a dialog.
- Hold **Space** plus `Q` through `P` for numbers `1` through `0`; other letter keys produce their printed symbols.
- Hold **CAP** plus a letter for uppercase.
- The orange **Alt** key mirrors the number/symbol layer while editing.
- Outside text fields, **Alt+L** opens the launcher, **Alt+T** opens Today, **Alt+N** opens Transition, and **Alt+C** opens Quick Capture.
- Inside text fields, those same keys type their printed symbols instead of launching an app.

## Storage layout

```text
/PocketPDA/calendar/events.dat
/PocketPDA/tasks/tasks.dat
/PocketPDA/assignments/assignments.dat
/PocketPDA/packing/templates.dat
/PocketPDA/packing/state.dat
/PocketPDA/routines/routines.dat
/PocketPDA/timers/presets.dat
/PocketPDA/notes/index.dat
/PocketPDA/notes/00000001.txt
/PocketPDA/habits/habits.dat
/PocketPDA/messages/messages.dat
/PocketPDA/files/
```

Tasks are bounded at 64 records, Habits at 8 daily habits, Notes at 32 records, note bodies at 2047 bytes, and Files displays up to 32 entries per folder.

## Copying files without removing the card

1. Connect the running Pager to the computer.
2. Open **Settings** and choose **USB DISK**.
3. Wait for the microSD volume to appear on the computer.
4. Copy files normally.
5. Eject the SD volume on the computer before disconnecting or resetting the Pager.

PocketPDA gives the computer exclusive control of the card during USB Disk Mode. After a safe eject, it remounts the card and reloads all organizer data automatically.

## Backup and restore

Open **Settings** and choose **BACKUP**. PocketPDA streams all organizer data into `/PocketPDA/backups/PocketPDA-Backup.ppb` without loading it all into RAM. Then use **USB DISK** to copy that single file to a computer. With companion v0.1.5 and matching updated firmware, **Back Up to Mac…** instead builds and verifies a fresh restore-compatible archive directly on the Mac while USB Disk Mode stays open. No manual on-device backup, ejection, or reconnection is needed to create it.

To restore, copy `PocketPDA-Backup.ppb` from the computer back into `/PocketPDA/backups/`, safely eject the volume, leave USB Disk Mode, and choose **RESTORE** in Settings. The complete archive and every file checksum are validated first. Data is extracted into staging directories and swapped into place only after validation succeeds; invalid or incomplete backups are rejected.

## Offline computer editor and bulk import

Open [`tools/pocketpda-editor.html`](tools/pocketpda-editor.html) in any modern browser. Add calendar events, tasks, and routines, then download the CSV for each section you used. The editor is one self-contained file, works offline, and does not upload personal data.

In **USB DISK** mode, copy the downloaded files using these names:

```text
calendar-import.csv  -> /PocketPDA/calendar/import.csv
tasks-import.csv     -> /PocketPDA/tasks/import.csv
routines-import.csv  -> /PocketPDA/routines/import.csv
```

Safely eject the volume and leave USB Disk Mode. PocketPDA validates each complete file, converts it to the checksummed native format, and renames it to `last-import.csv`. Calendar and task imports replace those lists; a routine import replaces the routine list. Do not put commas in fields; the computer editor automatically converts them to semicolons.

Calendar CSVs can also be created manually. A generic example is included at [`examples/calendar-import.csv`](examples/calendar-import.csv).

```csv
date,start,end,title,location,reminder_minutes,repeat
2026-10-08,08:30,09:30,First lesson,Room 12,5,weekdays
```

The optional calendar `repeat` value is `once`, `daily`, `weekdays`, `weekly`, or `monthly`. The calendar supports up to 65,535 on-disk events while caching only the current and following week in memory. Tasks support up to 64 entries and routines support up to 12 routines with eight steps each.

## macOS companion

Build the native companion with `companion/macos/build.sh`, then open `dist/PocketPDA Companion.app`. The app detects the Pager after **Settings > USB DISK**, can synchronize calendars already available through macOS Calendar, set the pager clock from the Mac (with matching updated firmware), request and download validated backup archives, validate and stage restores, and safely eject the microSD volume. See [`companion/README.md`](companion/README.md) for details.

## Reminder behavior

- Calendar reminders fire the configured number of minutes before an event, vibrate, and show its title, room, and time.
- Task reminders are optional and currently fire at 09:00 on the due date.
- Every calendar and task reminder offers **DISMISS** and **SNOOZE 5**. Snoozing schedules the alert again five minutes later, including through the RTC alarm.
- A reminder value of `0` means the event start time; new calendar events default to `5` minutes beforehand.

## Battery alerts

The status bar shows a lightning bolt while the battery is actively charging. During discharge, PocketPDA gives one warning as the battery crosses 20%, 10%, and 5%. Connecting USB power resets the warning cycle.

## LoRa messaging

Open **MESSAGES > SETUP** on both pagers, give each one a recognizable name, enable messaging, and save. Enter the same 6–16 character pairing code on both units and press **PAIR** within two minutes. Once each pager lists the other as paired, open that contact to send or reply. Messages are limited to 160 UTF-8 bytes.

Messages use AES-256-GCM authenticated encryption, device and message identifiers, acknowledgements, three bounded delivery attempts, and persistent duplicate suppression. The default EU868 profile is 868.300 MHz, 125 kHz bandwidth, SF8, CR 4/5, and 14 dBm. Firmware enforces a conservative 1% transmit airtime budget. Pair only in a trusted place: the human-entered pairing code prevents accidental pairing, but a short code is not resistant to a determined attacker who records the pairing exchange. Secret contact keys stay in the pager's NVS and are deliberately excluded from microSD backups; restored message history remains, but a replacement pager must be paired again.

The initial release keeps the SX1262 in continuous receive while messaging is enabled, including while the display is off. Reliability comes first; no aggressive receive sleep schedule is enabled until real battery tests establish the cost. Every ten minutes the USB serial log emits a `[PocketPDA][radio-energy]` line containing cumulative RX/TX/sleep time, packets, battery percentage, voltage, current, and USB state. See [`docs/messaging.md`](docs/messaging.md) for the repeatable battery test and protocol details.

## Power off

Open **Settings**, choose **SHUT DOWN**, and confirm. USB-C must be disconnected because external power keeps the board alive. Press the hardware PWR button to wake it.

## Project status

### Next release

- Distinguish **USB connected**, **actively charging**, and **charge complete/idle** in the status bar instead of showing the charging symbol only while current is flowing.

## Timers and alarms

Open **TIMERS** from the launcher. The first run creates **Focus 15**, **Break 5**, and **Leave in 10** presets. Press a timer to start it; its finish time is saved on the microSD card, so display sleep or an accidental reboot does not lose it. Press **EDIT** to change its name, duration, or vibration style.

Choose **+ NEW** and select **Daily alarm** to add a clock alarm. Enter a 24-hour time such as `07:30`. Pressing an alarm in the list toggles it on or off. Timers and alarms continue to be checked while the display is off, but a fully powered-down pager cannot sound until it is switched on again.

## Assignment planner

Open **ASSIGNMENTS** and choose **+ NEW**. Give the assignment a due date, estimated total effort, priority, and up to six small actions—one action per line. The planner automatically puts the most urgent unfinished assignment in the large **NEXT** card. Press **DONE STEP** to advance; only the next actionable step is emphasized. Finished assignments remain available for review or deletion.

## Event-linked packing lists

Open **PACKING** to see the checklist for the next calendar event within 14 days. A template matches when its lesson word appears anywhere in the event title, so a template with the keyword `Physical` automatically appears for “Physical and health education.” The first run includes PE, Design, and Music examples.

Choose **TEMPLATES** to create or edit up to 12 reusable lists with six items each. Checks are saved on the microSD card and reset automatically for the next occurrence of that lesson.

This is early hardware-specific firmware. Back up important data before testing new builds. GPS, NFC, motion sensing, and audio remain deliberately uninitialized; LoRa is initialized only when messaging is enabled.

Contributions and device-testing reports are welcome, especially improvements that make the interface calmer, clearer, and easier to operate without a phone.

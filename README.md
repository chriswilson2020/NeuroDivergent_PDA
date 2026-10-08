# NeuroDivergent PDA

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
- **Routines:** reusable, one-step-at-a-time checklists for predictable transitions and daily activities
- **Notes:** compact note list and editor with separate note-body storage
- **Clock:** RTC-backed date/time and stopwatch
- **Habits + Pet:** editable daily habits, streaks, and an animated companion that reacts to progress
- **Files:** microSD folder browser, text preview, and deletion
- **Settings:** clock, brightness, display timeouts, storage status, haptic test, USB Disk Mode, and shutdown
- **Offline computer editor:** build calendar, task, and routine CSV files without an account or internet connection
- **Portable backup:** one checksummed file for calendar, tasks, routines, habits, notes, files, and device settings
- Persistent status bar with charging state, launcher, haptic notifications, low-battery warnings, and dim/display-off power states
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

## Controls

- Rotate the wheel to move focus or scroll.
- Press the wheel to activate the focused control or enter/leave text-editing mode.
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
/PocketPDA/routines/routines.dat
/PocketPDA/notes/index.dat
/PocketPDA/notes/00000001.txt
/PocketPDA/habits/habits.dat
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

Open **Settings** and choose **BACKUP**. PocketPDA streams all organizer data into `/PocketPDA/backups/PocketPDA-Backup.ppb` without loading it all into RAM. Then use **USB DISK** to copy that single file to a computer.

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

## Reminder behavior

- Calendar reminders fire the configured number of minutes before an event, vibrate, and show its title, room, and time.
- Task reminders are optional and currently fire at 09:00 on the due date.
- Every calendar and task reminder offers **DISMISS** and **SNOOZE 5**. Snoozing schedules the alert again five minutes later, including through the RTC alarm.
- A reminder value of `0` means the event start time; new calendar events default to `5` minutes beforehand.

## Battery alerts

The status bar shows a lightning bolt while the battery is actively charging. During discharge, PocketPDA gives one warning as the battery crosses 20%, 10%, and 5%. Connecting USB power resets the warning cycle.

## Power off

Open **Settings**, choose **SHUT DOWN**, and confirm. USB-C must be disconnected because external power keeps the board alive. Press the hardware PWR button to wake it.

## Project status

This is early hardware-specific firmware. Back up important data before testing new builds. LoRa, GPS, NFC, motion sensing, and audio are deliberately not initialized in v0.2.0.

Contributions and device-testing reports are welcome, especially improvements that make the interface calmer, clearer, and easier to operate without a phone.

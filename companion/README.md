# PocketPDA Companion for macOS

PocketPDA Companion is a native macOS application for a Pager running PocketPDA. Version 0.2 adds a Palm/Psion-style desktop organizer while retaining the sync and backup tools. It detects the microSD volume exposed by **Settings > USB DISK** and provides:

- a launcher-style sidebar, Today agenda and transition countdown;
- viewing, creating, editing and deleting calendar events, tasks, assignments, notes, routines, packing templates, timer/alarm presets and habits;
- task completion, assignment steps, routine walkthroughs and habit completion with a cheerful pet;
- read-only message history and composition for paired contacts with matching desktop-bridge firmware;
- file import, export and small text-file previews;

- one-way synchronization from selected macOS calendars, including Outlook calendars added to macOS;
- a selectable 4, 12, or 26 week synchronization window;
- preservation of a previous CSV-imported base schedule;
- manual PocketPDA calendar CSV import;
- setting the pager's date and time from the Mac, preserving its timezone;
- backup requests initiated from the Mac, verified backup download, and verified restore staging;
- safe device ejection; and
- access to the offline organizer editor.

## Desktop organizer workflow

Calendar has **Day**, **Week** (Monday–Sunday) and **Month** views, previous/next navigation, Today, a date jump and title/location search. Day/week events are positioned by their start/end times; overlapping appointments appear side by side. Click an event to edit it, double-click an empty time slot to add one, or use a month's **+** button. Month overflow links open that day's full schedule. Repeating occurrences are displayed automatically; editing or deleting one edits or deletes the stored series, not a single-occurrence exception. All views use the pager's timezone and preserve its existing data format.

Version 0.2.2 separates the calendar heading/view switch from navigation and search, uses aligned fixed-width time columns with horizontal scrolling on narrow windows, and adapts event details to their height. Month grids show only the weeks needed for that month, retain readable minimum cell heights, and scroll vertically rather than clipping busy days.

Version 0.2.4 applies responsive layout across the companion, with a minimum supported window size of 820 × 560. Content fills the available detail area; the status footer is inset at the bottom. Organizer and Files pages use side-by-side panes when the detail area is at least 680 points wide and a list → detail → Back flow below that width. Action rows wrap with consistent gaps, Sync/Backup cards adapt from two columns to one, and longer capture/settings content scrolls rather than expanding beyond the window. No pager firmware change is required for these layout updates.

Version 0.2.5 replaces the nested native split/list controls with explicitly sized SwiftUI panes and scrollable selectable rows, addressing clipped item text in organizer pages. Empty sections now have a single full-page empty state. `tests/render_companion_layout.swift`, compiled with `POCKETPDA_LAYOUT_QA`, renders the actual organizer views on temporary demo data at multiple widths, including selected-item details; it does not access attached pagers. This complements data-format and pane/wrapping geometry tests, rather than replacing physical-device testing.

Version 0.2.6 separates **Settings** (Mac appearance, row density, default calendar view and attached-pager information) from **Sync & Backup**. Hardware brightness/sleep settings still live on the pager. Existing-item editors now include a visible, confirmed Delete action; repeating calendar events are labelled **Delete Series** because there is no single-occurrence exception format. Calendar also offers an **Agenda** list/details view alongside Day, Week and Month. No firmware update is required.

Connect the pager, enter **Settings > USB DISK**, and select an app in the Mac sidebar. Edits are explicit: click **Save** to write them to the attached pager's microSD. Eject through the companion when finished; the pager reloads the data. Disconnecting disables editing. The companion is not an independent cloud account or a second organizer database.

The firmware and radio are paused while the Mac owns the SD card. Messages composed on the Mac are **queued**, not transmitted live: after ejection, matching firmware validates the paired recipient and passes them to the existing reliable messaging queue. Delivered history cannot be edited or retransmitted. Older firmware can expose organizer files, but must be updated for contact discovery and desktop message sending. Pairing secrets are never exported to the Mac manifest. Timers on the Mac edit pager presets; they do not start live pager countdowns. Settings contains Mac appearance and organizer defaults; clock, calendar transfer, backup and restore have their own Sync & Backup page. Hardware settings remain on the pager.

Saves validate firmware layouts and checksums, preserve record IDs and unedited bytes, reject external file changes and stale USB sessions, and retain previous file versions under `/PocketPDA/desktop-undo/`. These undo copies are recovery files, not a substitute for **Back Up to Mac…**. A staged CSV import blocks edits to that section until it has been applied or removed, preventing the import from silently replacing desktop changes. Note deletion removes its index entry but retains the text body for recovery.

For a disconnected sample organizer, launch the executable with `--demo`. Demo mode creates a temporary sample volume and does not scan or write attached pagers.

## Build

```sh
chmod +x companion/macos/build.sh
companion/macos/build.sh
```

The signed Universal application is created at `dist/PocketPDA Companion.app`. It runs on Apple Silicon and Intel Macs with macOS 14 or newer. On first calendar synchronization, macOS asks for calendar access. Outlook calendars are available after adding the Outlook account under **System Settings > Internet Accounts** and enabling Calendars.

## Safety model

Calendar synchronization in v0.3 uses a preview and a verified binary calendar write, preserving local events rather than replacing the calendar via CSV. With companion **v0.1.5 and matching updated firmware**, **Back Up to Mac…** asks where to save, then streams the organizer files directly from the mounted SD card into a new Mac archive. The pager exports a small, checksummed settings snapshot immediately before entering USB Disk Mode, bound to the current USB session. The app includes this snapshot, verifies every archived file, and only then atomically installs the destination backup. Existing Mac backups remain unchanged on copy or verification failure. No on-device backup button, ejection or reconnection is required. The app does not move or delete previous on-device archives.

## Companion v0.3

- Explicit last-save state and **Eject to Apply**. Saving writes the SD card; ejection reloads pager apps and resumes radio operation.
- A verified, complete Mac recovery point before the first mutation in each USB session. If backup fails, the mutation is blocked. Individual file versions also remain in SD `desktop-undo`. Mac recovery points live in `~/Library/Application Support/PocketPDA/Backups/<hardware-ID>/`; they are not automatically deleted. They can contain personal organizer data and device settings; protect this folder like other personal backups.
- **Sync & Backup → Recovery points** browses the attached pager's backups. Restore first protects the current state, stages the selected archive, and still requires RESTORE confirmation on the pager.
- Outlook via **macOS Calendar/Internet Accounts**, not an independent Microsoft Graph login. Select calendars, preview additions/updates/duplicates/warnings, then explicitly save. Selections persist. Recurrences are expanded by EventKit over the chosen horizon. Previously imported appointments are updated only if unchanged locally; local edits/deletions produce a warning rather than being overwritten. Source-deleted events are retained until explicitly removed. Nothing is written to Outlook. Provenance is stored per pager on this Mac; on a different Mac exact duplicates are still skipped, but source updates cannot be matched until imported there.
- **Calendar → Bulk Tools**: select multiple stored events, select by starting-date range, delete with confirmation, copy a week as dated one-off lessons, and preview `.ics` imports. Offline ICS imports support timed single events, UTC/TZID/floating dates and folded/escaped text. Recurring ICS files must be expanded through macOS Calendar; unsupported all-day, multi-day or over-length records are explicitly rejected or reported, never silently truncated.
- **Device**: stable chip ID, organizer counts, volume capacity and timezone. The matching firmware also exports build identity, battery/charging and UTC clock at USB entry. These are snapshots, not live telemetry. Clock comparison includes USB connection delay; reloading much later cannot give a precise drift measurement.
- **Firmware Update**: back up, eject/leave USB Disk Mode, select the intended serial port and included firmware (or a local application image), confirm, update, then reconnect in Disk Mode and set time. PlatformIO's Python/esptool must already be installed. Application-only `0x10000` writes preserve NVS and SD; factory/merged/wrong-chip images are refused. The app does not download unsigned releases or automatically retry against a different port.
- **Power & Radio**: battery history, latest-boot CPU sleep/awake chart, recorded packet counts, message-attempt chart and delivery totals. Drain estimates require at least 30 minutes of consecutive valid unplugged samples; charging, counter resets and clock jumps are excluded. Raw current/previous power and sleep CSVs can be exported without changing the originals.

Verification uses synthetic firmware-format fixtures, calendar import/conflict tests, backup tests, application-only flash validation and offscreen rendering of actual SwiftUI views. Hardware flashing and real Outlook-account authorization remain interactive steps; building the app does not perform either.

Older companion versions used an eject/create/reconnect request workflow. Existing pending downloads can still finish if their requested archive is available; otherwise use **Cancel Pending Download** to clear the stale app state without deleting any backup or organizer data. A fresh direct backup is never blocked by a pending old download. A staged restore is never applied automatically; the user must eject the device and confirm **RESTORE** on the Pager.

## Set time from Mac (companion v0.1.4)

Install the firmware built alongside this companion update, then enter **Settings > USB DISK**. Click **Set Pager Time from Mac**. The app writes the Mac's UTC Unix timestamp and safely ejects. The pager consumes the request once, verifies its RTC write, and displays **MAC TIME SYNC**. Re-enter USB Disk Mode to read the verified success/failure result in the app. No internet, satellite fix, calendar replacement, or timezone change is involved. For Amsterdam local time, keep **Europe/Amsterdam** selected on the pager. Check that the Mac itself has correct date and time first.

This is a manual USB handoff, not subsecond network synchronization: the clock may lag by the few seconds between writing the request and applying it after ejection. Eject failures should be resolved by clicking time sync again before ejecting, rather than leaving a staged timestamp for later. A fresh device-specific random session is published before each USB handoff; the firmware rejects requests from another session or pager, malformed timestamps, and dates outside 2020–2099. It does not apply requests found at boot. Use the application-only updater (`tools/flash_update.py`), not a factory flash, to preserve existing NVS preferences.

The app uses USB Disk Mode for broad compatibility. Direct backups are streamed with bounded buffers and use the pager's PDB1 format, including its current 128-entry and 87-byte UTF-8 path limits. Hidden Mac metadata and `@eaDir` thumbnails are excluded; symbolic links and unsupported paths are rejected rather than silently followed. The firmware never accesses the card while the Mac owns it.

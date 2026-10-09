# PocketPDA LoRa messaging

## User flow

1. On each pager open **MESSAGES > SETUP**.
2. Set distinct names such as `Chris` and `Gabriel`, check **Enable LoRa messaging**, and save.
3. Enter the same 6–16 character code on both pagers and press **PAIR** on both within two minutes.
4. Confirm that each pager lists the other under `Paired`.
5. Return to the inbox, open the contact, press **REPLY**, type up to 160 UTF-8 bytes, and send.

Outgoing records visibly progress through `queued`, `sending`, `delivered`, or `FAILED`. A missing acknowledgement triggers at most three attempts using the configurable retry interval (30 seconds by default). Reboots preserve the outbox and convert an interrupted `sending` record back to `queued`.

## Protocol and threat model

PocketPDA uses a small versioned binary protocol over SX1262 LoRa P2P. Every data packet includes a protocol magic/version, type, sender and recipient IDs, message ID, persistent packet counter, and payload length. Message and acknowledgement payloads use AES-256-GCM; the header is authenticated as additional data. The 96-bit nonce consists of the sender ID plus the monotonically increasing packet counter. Counter ranges are reserved in NVS before use so a power loss cannot reuse a nonce.

Pairing is explicit and time limited. Pair advertisements contain a device ID, name, and truncated HMAC proof derived from the shared code. Both devices derive the same contact key from the code and sorted device IDs. Pairing codes stop accidental or drive-by association but remain susceptible to offline guessing if the radio exchange is captured, especially when short or predictable. Pair in a trusted location and use the longest random code practical. Once paired, forged or modified messages fail GCM authentication.

Contact keys are stored in the ESP32's NVS. They are not printed, written to the SD card, or included in the portable backup. NVS confidentiality against physical flash extraction depends on ESP32 flash/NVS encryption, which normal development builds do not enable. This feature is private against ordinary over-the-air eavesdropping, not a hardened high-risk communications system.

Incoming message identity is the `(sender device ID, message ID)` pair. It is persisted before notifying the user. Repeated packets are acknowledged again but do not create another record or alert. Message history is stored in `/PocketPDA/messages/messages.dat`, is checksummed and atomically replaced, and is included in PocketPDA backup/restore. The current bounded history is 128 records.

## Radio defaults and regional use

The shipping profile is intended for EU868 testing: 868.300 MHz, 125 kHz bandwidth, SF8, coding rate 4/5, 12-symbol preamble, CRC enabled, and 14 dBm output. A software airtime gate delays every transmission so aggregate application traffic remains at or below a conservative 1% duty cycle. Frequency, spreading factor, and power are configurable because legal limits vary by location. The operator is responsible for selecting a permitted profile and checking the current [ETSI EN 300 220-2 requirements](https://www.etsi.org/deliver/etsi_en/300200_300299/30022002/03.03.01_60/en_30022002v030301p.pdf) and national rules.

USB Disk Mode suspends the radio before the computer takes ownership of the shared microSD/SPI environment and resumes it after safe eject. Shutdown also puts the radio to sleep. Disabling messaging leaves the radio asleep.

## Battery measurement before receive sleeping

Continuous receive is the v0.3 reliability baseline. Do not introduce periodic receive windows until this A/B test has been completed on both production pagers:

1. Charge both pagers to charger-complete, leave connected for 30 minutes, then disconnect together.
2. Use the same brightness and display-off timeout, disable unrelated active timers, and leave both in the same temperature/location.
3. On pager A enable messaging; on pager B disable messaging. Do not send traffic during the idle baseline.
4. Capture USB serial output before disconnecting and again at 6, 12, and 24 hours. Record timestamp, SOC, voltage, current, and the `[radio-energy]` RX/TX/sleep counters. Brief USB sampling can perturb the test, so connect for the shortest consistent interval.
5. Repeat with roles reversed to separate radio cost from battery/gauge variation.
6. Run a traffic test with a fixed script: 10 messages in each direction per hour, recording delivery, retries, and missed notifications with the display off.
7. Compare percent/hour and, where stable, voltage/current trends. Report both individual devices and both directions; do not collapse the result into one number.

Only after the continuous-RX baseline is measured should a receive-window experiment be added behind a configuration flag. It must repeat the same run and demonstrate an acceptable missed-message latency and delivery rate before becoming a default.

## Verification matrix

- Pair two fresh devices and verify names/IDs appear only with matching codes.
- Send in both directions, reply, and verify `delivered` only after an ACK.
- Power off the receiver, send, and verify three attempts then `FAILED`.
- Reboot the sender while queued/sending and verify retry resumes.
- Replay the same captured packet and verify one stored message and one notification.
- Receive with another app open and with the display off; verify vibration/unread count without screen illumination.
- Reboot after receiving and verify history/unread state persists.
- Enter USB Disk Mode and verify radio suspension, safe eject, message reload, and receive resumption.
- Back up, delete a message, restore, and verify history returns while pairing keys remain device-local.

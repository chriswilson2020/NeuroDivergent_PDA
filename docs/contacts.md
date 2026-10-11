# Contacts

Open CONTACTS from the pager launcher. Search checks names, companies, job titles,
both phone numbers, email, address, website and notes (case-insensitive). Favourites
appear first; other entries are sorted alphabetically by name, or company when the
name is blank. Results have eight entries per page, with PREV/NEXT navigation.

Select a contact to read details, edit, mark as a favourite or delete. Delete asks
for confirmation, with CANCEL focused first. The editor scrolls; wheel navigation
reaches every field and the save/cancel controls. Drafts survive visiting another
app and returning, but are not saved across a reboot until SAVE is selected.
The physical Back key still edits text; held wheel returns to the launcher.

Fields: name, company, job title, mobile, other phone, email, full multiline postal
address, website, multiline notes, and favourite status. A name or company is
required. UTF-8 byte limits are respectively 63, 63, 47, 31, 31, 95, 159, 95 and
255; oversized multibyte text produces a visible error instead of silent truncation.

Up to 512 records are held in external PSRAM, not the ESP32's internal heap.
Only one page of result controls is created at a time. Data is checksummed on
microSD at `PocketPDA/contacts/contacts.dat`. Writes stage a complete replacement
before rotating the previous file to `.bak`; interrupted final renames can recover
the previous file on load. Failed writes roll back in-memory changes. Corrupt data
is not silently overwritten: restore a backup. A working microSD is required.

Contacts are included in firmware and updated companion backup/restore support.
Restoring an older full backup without contacts resets the contact list, consistent
with replacing organizer data. The existing installed companion must be rebuilt
to accept backups containing the new contacts folder. This change does not add
desktop contact editing, phone calls, SMS, or alter LoRa pairing/addressing.
The normal passcode lock covers contacts; backups/microSD remain unencrypted.

Tests use the production store and LVGL app with in-memory SD/PSRAM adapters:
search, sorting, persistence, write/rename failures, rollback, interrupted-save
recovery, corrupt-file protection, draft navigation, editing, deletion confirmation
and focus recovery. Companion backup tests verify inclusion of the contacts folder.

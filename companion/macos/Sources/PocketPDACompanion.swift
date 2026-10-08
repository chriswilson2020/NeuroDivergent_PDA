import AppKit
import EventKit
import Foundation
import SwiftUI

private let backupMagic: UInt32 = 0x50444231

struct CalendarChoice: Identifiable {
    let id: String
    let calendar: EKCalendar
    var selected: Bool
}

@MainActor
final class CompanionModel: ObservableObject {
    @Published var deviceRoot: URL?
    @Published var status = "Looking for PocketPDA..."
    @Published var detail = "Connect the Pager, then choose Settings > USB DISK."
    @Published var calendars: [CalendarChoice] = []
    @Published var weeks = 12
    @Published var preserveBaseSchedule = true
    @Published var busy = false

    private let eventStore = EKEventStore()
    private var scanTimer: Timer?

    init() {
        scan()
        scanTimer = Timer.scheduledTimer(withTimeInterval: 2, repeats: true) { [weak self] _ in
            Task { @MainActor in self?.scan() }
        }
    }

    var connected: Bool { deviceRoot != nil }
    var pocketRoot: URL? { deviceRoot?.appendingPathComponent("PocketPDA", isDirectory: true) }

    func scan() {
        let volumes = URL(fileURLWithPath: "/Volumes", isDirectory: true)
        let candidates = (try? FileManager.default.contentsOfDirectory(at: volumes, includingPropertiesForKeys: [.volumeAvailableCapacityForImportantUsageKey, .volumeTotalCapacityKey], options: [.skipsHiddenFiles])) ?? []
        let found = candidates.first { FileManager.default.fileExists(atPath: $0.appendingPathComponent("PocketPDA", isDirectory: true).path) }
        guard found != deviceRoot else { return }
        deviceRoot = found
        if let found {
            status = "PocketPDA connected"
            let values = try? found.resourceValues(forKeys: [.volumeAvailableCapacityForImportantUsageKey, .volumeTotalCapacityKey])
            let free = ByteCountFormatter.string(fromByteCount: Int64(values?.volumeAvailableCapacityForImportantUsage ?? 0), countStyle: .file)
            let total = ByteCountFormatter.string(fromByteCount: Int64(values?.volumeTotalCapacity ?? 0), countStyle: .file)
            detail = "\(found.lastPathComponent)  •  \(free) free of \(total)"
        } else {
            status = "PocketPDA not connected"
            detail = "On the Pager choose Settings > USB DISK, then wait for it to appear."
        }
    }

    func chooseDevice() {
        let panel = NSOpenPanel()
        panel.title = "Choose the PocketPDA microSD volume"
        panel.canChooseDirectories = true
        panel.canChooseFiles = false
        panel.allowsMultipleSelection = false
        guard panel.runModal() == .OK, let root = panel.url else { return }
        guard FileManager.default.fileExists(atPath: root.appendingPathComponent("PocketPDA").path) else {
            showError("That folder does not contain a PocketPDA directory.")
            return
        }
        deviceRoot = root
        status = "PocketPDA connected"
        detail = root.path
    }

    func requestCalendars() {
        busy = true
        Task {
            do {
                let granted: Bool
                if #available(macOS 14.0, *) {
                    granted = try await eventStore.requestFullAccessToEvents()
                } else {
                    granted = try await withCheckedThrowingContinuation { continuation in
                        eventStore.requestAccess(to: .event) { allowed, error in
                            if let error { continuation.resume(throwing: error) }
                            else { continuation.resume(returning: allowed) }
                        }
                    }
                }
                guard granted else { throw CompanionError.message("Calendar access was not granted. Enable it in System Settings > Privacy & Security > Calendars.") }
                calendars = eventStore.calendars(for: .event)
                    .sorted { ($0.source.title, $0.title) < ($1.source.title, $1.title) }
                    .map { CalendarChoice(id: $0.calendarIdentifier, calendar: $0, selected: false) }
                detail = calendars.isEmpty ? "No macOS calendars were found." : "Choose the Outlook or other calendars to synchronize."
            } catch { showError(error.localizedDescription) }
            busy = false
        }
    }

    func toggleCalendar(_ id: String) {
        guard let index = calendars.firstIndex(where: { $0.id == id }) else { return }
        calendars[index].selected.toggle()
    }

    func syncSelectedCalendars() {
        guard let pocketRoot else { return }
        let selected = calendars.filter(\.selected).map(\.calendar)
        guard !selected.isEmpty else { showError("Select at least one calendar first."); return }
        busy = true
        do {
            let fm = FileManager.default
            let calendarDir = pocketRoot.appendingPathComponent("calendar", isDirectory: true)
            try fm.createDirectory(at: calendarDir, withIntermediateDirectories: true)
            let start = Calendar.current.startOfDay(for: Date())
            let end = Calendar.current.date(byAdding: .day, value: weeks * 7, to: start)!
            let events = eventStore.events(matching: eventStore.predicateForEvents(withStart: start, end: end, calendars: selected))
                .sorted { $0.startDate < $1.startDate }
            var rows = ["date,start,end,title,location,reminder_minutes,repeat"]
            let base = calendarDir.appendingPathComponent("companion-base.csv")
            if preserveBaseSchedule {
                if !fm.fileExists(atPath: base.path) {
                    let previous = calendarDir.appendingPathComponent("last-import.csv")
                    if fm.fileExists(atPath: previous.path) { try fm.copyItem(at: previous, to: base) }
                }
                if let existing = try? String(contentsOf: base, encoding: .utf8) {
                    rows.append(contentsOf: existing.split(whereSeparator: \Character.isNewline).dropFirst().map(String.init))
                }
            }
            let day = DateFormatter(); day.locale = Locale(identifier: "en_US_POSIX"); day.dateFormat = "yyyy-MM-dd"
            let clock = DateFormatter(); clock.locale = Locale(identifier: "en_US_POSIX"); clock.dateFormat = "HH:mm"
            for event in events where !event.isAllDay {
                let reminder = event.alarms?.compactMap { $0.relativeOffset < 0 ? Int(abs($0.relativeOffset) / 60) : nil }.min() ?? 5
                rows.append([day.string(from: event.startDate), clock.string(from: event.startDate), clock.string(from: event.endDate), csv(event.title ?? "Untitled", maxBytes: 39), csv(event.location ?? "", maxBytes: 19), String(reminder), "once"].joined(separator: ","))
            }
            let destination = calendarDir.appendingPathComponent("import.csv")
            try (rows.joined(separator: "\n") + "\n").data(using: .utf8)!.write(to: destination, options: .atomic)
            detail = "Prepared \(events.filter { !$0.isAllDay }.count) calendar events for the next \(weeks) weeks. Eject to import them."
        } catch { showError(error.localizedDescription) }
        busy = false
    }

    func importCSV() {
        guard let pocketRoot else { return }
        let panel = NSOpenPanel(); panel.title = "Choose a PocketPDA calendar CSV"; panel.allowedContentTypes = [.commaSeparatedText, .plainText]
        guard panel.runModal() == .OK, let source = panel.url else { return }
        do {
            let data = try Data(contentsOf: source)
            guard let text = String(data: data, encoding: .utf8), text.hasPrefix("date,start,end,title,location,reminder_minutes") else {
                throw CompanionError.message("The selected file is not a PocketPDA calendar CSV.")
            }
            let calendarDir = pocketRoot.appendingPathComponent("calendar", isDirectory: true)
            try FileManager.default.createDirectory(at: calendarDir, withIntermediateDirectories: true)
            try data.write(to: calendarDir.appendingPathComponent("import.csv"), options: .atomic)
            detail = "Calendar import staged. Eject the Pager to apply it."
        } catch { showError(error.localizedDescription) }
    }

    func downloadBackup() {
        guard let pocketRoot else { return }
        let source = pocketRoot.appendingPathComponent("backups/PocketPDA-Backup.ppb")
        do {
            guard FileManager.default.fileExists(atPath: source.path) else { throw CompanionError.message("No backup is on the Pager. Exit USB Disk Mode, choose BACKUP in Settings, then return to USB Disk Mode.") }
            let data = try Data(contentsOf: source)
            try validateBackup(data)
            let formatter = DateFormatter(); formatter.dateFormat = "yyyy-MM-dd-HHmm"
            let panel = NSSavePanel(); panel.title = "Save verified PocketPDA backup"; panel.nameFieldStringValue = "PocketPDA-\(formatter.string(from: Date())).ppb"
            guard panel.runModal() == .OK, let destination = panel.url else { return }
            try data.write(to: destination, options: .atomic)
            detail = "Backup verified and saved to \(destination.lastPathComponent)."
        } catch { showError(error.localizedDescription) }
    }

    func stageRestore() {
        guard let pocketRoot else { return }
        let panel = NSOpenPanel(); panel.title = "Choose a PocketPDA backup"; panel.allowedContentTypes = [.data]
        guard panel.runModal() == .OK, let source = panel.url else { return }
        do {
            let data = try Data(contentsOf: source)
            try validateBackup(data)
            let backupDir = pocketRoot.appendingPathComponent("backups", isDirectory: true)
            try FileManager.default.createDirectory(at: backupDir, withIntermediateDirectories: true)
            try data.write(to: backupDir.appendingPathComponent("PocketPDA-Backup.ppb"), options: .atomic)
            detail = "Restore staged and verified. Eject, then choose RESTORE on the Pager and confirm there."
        } catch { showError(error.localizedDescription) }
    }

    func eject() {
        guard let root = deviceRoot else { return }
        do {
            try NSWorkspace.shared.unmountAndEjectDevice(at: root)
            deviceRoot = nil
            status = "PocketPDA safely ejected"
            detail = "The Pager can now remount the card and apply staged imports."
        } catch { showError("Could not eject the Pager: \(error.localizedDescription)") }
    }

    func openOfflineEditor() {
        guard let url = Bundle.main.url(forResource: "pocketpda-editor", withExtension: "html") else { showError("The offline editor is missing from this app build."); return }
        NSWorkspace.shared.open(url)
    }

    private func csv(_ value: String, maxBytes: Int) -> String {
        let cleaned = value.replacingOccurrences(of: ",", with: ";").replacingOccurrences(of: "\n", with: " ").replacingOccurrences(of: "\r", with: " ")
        var result = ""
        for character in cleaned {
            let candidate = result + String(character)
            if candidate.lengthOfBytes(using: .utf8) > maxBytes { break }
            result = candidate
        }
        return result
    }

    private func validateBackup(_ data: Data) throws {
        guard data.count >= 8 else { throw CompanionError.message("The backup is incomplete.") }
        let magic = data.readUInt32(at: 0), version = data.readUInt16(at: 4), entries = Int(data.readUInt16(at: 6))
        guard magic == backupMagic, version == 1, entries > 0, entries <= 128 else { throw CompanionError.message("The backup header is invalid.") }
        var offset = 8
        for _ in 0..<entries {
            guard offset + 96 <= data.count else { throw CompanionError.message("The backup directory is incomplete.") }
            let pathBytes = data[offset..<(offset + 88)]
            guard let zero = pathBytes.firstIndex(of: 0) else { throw CompanionError.message("A backup path is invalid.") }
            let path = String(decoding: pathBytes[..<zero], as: UTF8.self)
            let allowedPrefixes = ["calendar/", "tasks/", "assignments/", "packing/", "habits/", "routines/", "timers/", "notes/", "files/"]
            guard path == "@settings" || (!path.hasPrefix("/") && !path.contains("..") && allowedPrefixes.contains(where: path.hasPrefix)) else {
                throw CompanionError.message("The backup contains an unsafe path.")
            }
            let size = Int(data.readUInt32(at: offset + 88)), expected = data.readUInt32(at: offset + 92)
            offset += 96
            guard size >= 0, offset + size <= data.count else { throw CompanionError.message("A backup entry is incomplete.") }
            var checksum: UInt32 = 2_166_136_261
            for byte in data[offset..<(offset + size)] { checksum = (checksum ^ UInt32(byte)) &* 16_777_619 }
            guard checksum == expected else { throw CompanionError.message("The backup checksum does not match.") }
            offset += size
        }
        guard offset == data.count else { throw CompanionError.message("The backup contains unexpected trailing data.") }
    }

    private func showError(_ message: String) {
        detail = message
        let alert = NSAlert(); alert.alertStyle = .warning; alert.messageText = "PocketPDA Companion"; alert.informativeText = message; alert.runModal()
    }
}

private enum CompanionError: LocalizedError {
    case message(String)
    var errorDescription: String? { if case .message(let value) = self { return value }; return nil }
}

private extension Data {
    func readUInt16(at offset: Int) -> UInt16 { UInt16(self[offset]) | UInt16(self[offset + 1]) << 8 }
    func readUInt32(at offset: Int) -> UInt32 { UInt32(self[offset]) | UInt32(self[offset + 1]) << 8 | UInt32(self[offset + 2]) << 16 | UInt32(self[offset + 3]) << 24 }
}

struct ContentView: View {
    @StateObject private var model = CompanionModel()

    var body: some View {
        VStack(spacing: 16) {
            HStack(spacing: 14) {
                Circle().fill(model.connected ? Color.green : Color.orange).frame(width: 14, height: 14)
                VStack(alignment: .leading, spacing: 3) {
                    Text(model.status).font(.title2.bold())
                    Text(model.detail).font(.callout).foregroundStyle(.secondary).lineLimit(2)
                }
                Spacer()
                Button("Choose Device…") { model.chooseDevice() }
                Button("Eject") { model.eject() }.disabled(!model.connected)
            }
            .padding(16).background(.quaternary.opacity(0.5), in: RoundedRectangle(cornerRadius: 14))

            HStack(alignment: .top, spacing: 16) {
                GroupBox("Calendar sync") {
                    VStack(alignment: .leading, spacing: 10) {
                        HStack {
                            Button(model.calendars.isEmpty ? "Load macOS Calendars" : "Refresh Calendars") { model.requestCalendars() }
                            Picker("Range", selection: $model.weeks) { Text("4 weeks").tag(4); Text("12 weeks").tag(12); Text("26 weeks").tag(26) }.frame(width: 150)
                        }
                        ScrollView {
                            LazyVStack(alignment: .leading, spacing: 7) {
                                ForEach(model.calendars) { item in
                                    Toggle(isOn: Binding(get: { item.selected }, set: { _ in model.toggleCalendar(item.id) })) {
                                        VStack(alignment: .leading, spacing: 1) { Text(item.calendar.title); Text(item.calendar.source.title).font(.caption).foregroundStyle(.secondary) }
                                    }
                                }
                            }
                        }.frame(minHeight: 150)
                        Toggle("Preserve the previous imported schedule", isOn: $model.preserveBaseSchedule)
                        HStack {
                            Button("Sync Selected Calendars") { model.syncSelectedCalendars() }.buttonStyle(.borderedProminent).disabled(!model.connected || model.calendars.allSatisfy { !$0.selected })
                            Button("Import CSV…") { model.importCSV() }.disabled(!model.connected)
                        }
                        Text("Outlook calendars appear here when the Outlook account is enabled in macOS Calendar. Sync is one-way to PocketPDA.").font(.caption).foregroundStyle(.secondary)
                    }.padding(8)
                }

                VStack(spacing: 16) {
                    GroupBox("Backup and restore") {
                        VStack(alignment: .leading, spacing: 10) {
                            Button("Download Verified Backup…") { model.downloadBackup() }.frame(maxWidth: .infinity, alignment: .leading).disabled(!model.connected)
                            Button("Stage Verified Restore…") { model.stageRestore() }.frame(maxWidth: .infinity, alignment: .leading).disabled(!model.connected)
                            Text("Create a backup on the Pager before entering USB Disk Mode. Restores require confirmation on the Pager after ejecting.").font(.caption).foregroundStyle(.secondary)
                        }.padding(8)
                    }
                    GroupBox("Organizer") {
                        VStack(alignment: .leading, spacing: 10) {
                            Button("Open Offline Editor") { model.openOfflineEditor() }
                            Text("Edit events, tasks, and routines without uploading personal data.").font(.caption).foregroundStyle(.secondary)
                        }.padding(8)
                    }
                    Spacer()
                }.frame(width: 260)
            }
            if model.busy { ProgressView().controlSize(.small) }
        }
        .padding(20)
        .frame(minWidth: 760, minHeight: 540)
    }
}

@main
struct PocketPDACompanionApp: App {
    var body: some Scene {
        WindowGroup("PocketPDA Companion") { ContentView() }
            .defaultSize(width: 820, height: 590)
    }
}

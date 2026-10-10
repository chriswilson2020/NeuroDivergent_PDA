import Foundation

// Compatible with the pager's PDB1 restore format; streams file contents rather
// than allocating an entire SD card backup in memory.
enum CompanionBackup {
    enum Failure: LocalizedError {
        case invalid(String)
        var errorDescription: String? { if case .invalid(let s) = self { return s }; return nil }
    }
    static let directories = ["calendar", "tasks", "assignments", "packing", "habits", "routines", "timers", "notes", "messages", "files"]
    static func u16(_ data: Data, _ at: Int) -> Int { Int(data[at]) | Int(data[at + 1]) << 8 }
    static func u32(_ data: Data, _ at: Int) -> UInt32 {
        UInt32(data[at]) | UInt32(data[at+1]) << 8 | UInt32(data[at+2]) << 16 | UInt32(data[at+3]) << 24
    }
    static func append32(_ value: UInt32, to data: inout Data) {
        for shift in stride(from: 0, to: 32, by: 8) { data.append(UInt8(truncatingIfNeeded: value >> shift)) }
    }
    static func checksum(_ data: Data, starting: UInt32 = 2_166_136_261) -> UInt32 {
        data.reduce(starting) { ($0 ^ UInt32($1)) &* 16_777_619 }
    }
    static func allowed(_ path: String) -> Bool {
        path == "@settings" || path == "@timesync" ||
        (!path.hasPrefix("/") && !path.contains("..") && directories.contains { path.hasPrefix($0 + "/") })
    }
    static func exact(_ handle: FileHandle, _ count: Int) throws -> Data {
        let data = try handle.read(upToCount: count) ?? Data()
        guard data.count == count else { throw Failure.invalid("Backup is incomplete.") }
        return data
    }
    @discardableResult
    static func validate(_ url: URL, settingsOnly: Bool = false) throws -> Int {
        let handle = try FileHandle(forReadingFrom: url); defer { try? handle.close() }
        let header = try exact(handle, 8), count = u16(header, 6)
        guard u32(header, 0) == 0x50444231, u16(header, 4) == 1, (1...128).contains(count),
              !settingsOnly || count == 2 else { throw Failure.invalid("Invalid backup header.") }
        var paths = Set<String>()
        for _ in 0..<count {
            let entry = try exact(handle, 96)
            guard let zero = entry.prefix(88).firstIndex(of: 0),
                  let path = String(data: entry.prefix(zero), encoding: .utf8), allowed(path),
                  paths.insert(path).inserted,
                  !settingsOnly || path == "@settings" || path == "@timesync" else {
                throw Failure.invalid("Invalid or duplicate backup path.")
            }
            var remaining = Int(u32(entry, 88)), hash: UInt32 = 2_166_136_261
            while remaining > 0 {
                let chunk = try exact(handle, min(remaining, 524_288))
                hash = checksum(chunk, starting: hash); remaining -= chunk.count
            }
            guard hash == u32(entry, 92) else { throw Failure.invalid("Backup checksum does not match.") }
        }
        if settingsOnly && paths != Set(["@settings", "@timesync"]) { throw Failure.invalid("Settings snapshot is incomplete.") }
        guard (try handle.read(upToCount: 1) ?? Data()).isEmpty else { throw Failure.invalid("Unexpected data after backup.") }
        return count
    }
    static func create(pocketRoot: URL, destination: URL) throws {
        let resolvedRoot = pocketRoot.resolvingSymlinksInPath()
        let fm = FileManager.default, commands = resolvedRoot.appendingPathComponent("commands")
        guard fm.fileExists(atPath: commands.appendingPathComponent("backup-settings.session").path),
              fm.fileExists(atPath: commands.appendingPathComponent("backup-settings.ppb").path) else {
            throw Failure.invalid("Update the pager firmware, then re-enter USB Disk Mode to enable direct Mac backup.")
        }
        let session = try String(contentsOf: commands.appendingPathComponent("time.session"), encoding: .utf8)
        let snapshotSession = try String(contentsOf: commands.appendingPathComponent("backup-settings.session"), encoding: .utf8)
        guard !session.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty, session == snapshotSession else {
            throw Failure.invalid("No current settings snapshot. Update the pager firmware, then re-enter USB Disk Mode.")
        }
        let snapshotURL = commands.appendingPathComponent("backup-settings.ppb")
        try validate(snapshotURL, settingsOnly: true)
        var snapshot = try Data(contentsOf: snapshotURL)
        var files: [(URL, String)] = []
        for directory in directories {
            let root = resolvedRoot.appendingPathComponent(directory)
            guard fm.fileExists(atPath: root.path) else { continue }
            var enumerationError: Error?
            guard let enumerator = fm.enumerator(at: root, includingPropertiesForKeys: [.isRegularFileKey, .isSymbolicLinkKey, .isDirectoryKey], options: [.skipsHiddenFiles], errorHandler: { _, error in enumerationError = error; return false }) else {
                throw Failure.invalid("Cannot read \(directory) for backup.")
            }
            for case let url as URL in enumerator {
                if url.lastPathComponent == "@eaDir" { enumerator.skipDescendants(); continue }
                let values = try url.resourceValues(forKeys: [.isRegularFileKey, .isSymbolicLinkKey, .isDirectoryKey])
                guard values.isSymbolicLink != true else { throw Failure.invalid("Backup cannot follow symbolic links.") }
                guard values.isRegularFile == true else { continue }
                let resolvedURL = url.resolvingSymlinksInPath()
                guard resolvedURL.path.hasPrefix(resolvedRoot.path + "/") else { throw Failure.invalid("Backup path escaped the organizer directory.") }
                let path = String(resolvedURL.path.dropFirst(resolvedRoot.path.count + 1))
                guard allowed(path), path.utf8.count < 88 else { throw Failure.invalid("Unsupported backup path: \(path)") }
                files.append((resolvedURL, path))
            }
            if let error = enumerationError { throw error }
        }
        guard files.count + 2 <= 128 else { throw Failure.invalid("The current pager archive format supports at most 128 files.") }
        let count = files.count + 2
        snapshot[6] = UInt8(count & 255); snapshot[7] = UInt8(count >> 8)
        let temporary = destination.deletingLastPathComponent().appendingPathComponent(".PocketPDA-\(UUID().uuidString).partial")
        guard fm.createFile(atPath: temporary.path, contents: nil) else { throw Failure.invalid("Cannot create the backup at this location.") }
        defer { try? fm.removeItem(at: temporary) }
        let output = try FileHandle(forWritingTo: temporary)
        do {
            try output.write(contentsOf: snapshot)
            for (url, path) in files.sorted(by: { $0.1 < $1.1 }) {
                let input = try FileHandle(forReadingFrom: url); defer { try? input.close() }
                var hash: UInt32 = 2_166_136_261, size: UInt64 = 0
                while let chunk = try input.read(upToCount: 524_288), !chunk.isEmpty {
                    size += UInt64(chunk.count); hash = checksum(chunk, starting: hash)
                }
                guard size <= UInt64(UInt32.max) else { throw Failure.invalid("File is too large for a pager backup.") }
                var entry = Data(path.utf8); entry.append(Data(repeating: 0, count: 88 - entry.count))
                append32(UInt32(size), to: &entry); append32(hash, to: &entry)
                try output.write(contentsOf: entry)
                try input.seek(toOffset: 0)
                while let chunk = try input.read(upToCount: 524_288), !chunk.isEmpty { try output.write(contentsOf: chunk) }
            }
            try output.synchronize(); try output.close()
            try validate(temporary) // Detect short reads or files changing while copied.
            if fm.fileExists(atPath: destination.path) {
                _ = try fm.replaceItemAt(destination, withItemAt: temporary)
            } else { try fm.moveItem(at: temporary, to: destination) }
        } catch { try? output.close(); throw error }
    }
}

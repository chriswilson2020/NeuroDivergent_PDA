import Foundation

@main
struct BackupTests {
    static func main() throws {
        let fm = FileManager.default
        let root = fm.temporaryDirectory.appendingPathComponent("PocketPDA-backup-test-\(UUID().uuidString)")
        try fm.createDirectory(at: root, withIntermediateDirectories: true)
        defer { try? fm.removeItem(at: root) }
        let pocket = root.appendingPathComponent("PocketPDA"), commands = pocket.appendingPathComponent("commands")
        try fm.createDirectory(at: commands, withIntermediateDirectories: true)
        var settings = Data([0x31, 0x42, 0x44, 0x50, 1, 0, 2, 0])
        for (path, body) in [("@settings", Data([1,2,3,4])), ("@timesync", Data(repeating: 0, count: 8))] {
            var entry = Data(path.utf8); entry.append(Data(repeating: 0, count: 88 - entry.count))
            CompanionBackup.append32(UInt32(body.count), to: &entry)
            CompanionBackup.append32(CompanionBackup.checksum(body), to: &entry)
            settings.append(entry); settings.append(body)
        }
        let snapshot = commands.appendingPathComponent("backup-settings.ppb")
        try settings.write(to: snapshot)
        let session = commands.appendingPathComponent("time.session"), marker = commands.appendingPathComponent("backup-settings.session")
        try Data("test-session\n".utf8).write(to: session)
        try Data("test-session\n".utf8).write(to: marker)
        let calendar = pocket.appendingPathComponent("calendar")
        try fm.createDirectory(at: calendar, withIntermediateDirectories: true)
        let body = Data(repeating: 42, count: 600_000)
        try body.write(to: calendar.appendingPathComponent("calendar.bin"))
        try Data("ignore metadata".utf8).write(to: calendar.appendingPathComponent("._calendar.bin"))
        let output = root.appendingPathComponent("backup.ppb")
        try CompanionBackup.create(pocketRoot: pocket, destination: output)
        let entryCount = try CompanionBackup.validate(output)
        assert(entryCount == 3)
        let archive = try Data(contentsOf: output)
        assert(archive.suffix(body.count) == body)
        // Existing destination can be atomically replaced with another verified archive.
        try CompanionBackup.create(pocketRoot: pocket, destination: output)
        func unchanged() throws { let saved = try Data(contentsOf: output); assert(saved == archive) }
        try unchanged()
        func rejects(_ work: () throws -> Void) {
            do { try work(); fatalError("Expected validation failure") } catch {}
        }
        try Data("another-pager\n".utf8).write(to: marker)
        rejects { try CompanionBackup.create(pocketRoot: pocket, destination: output) }
        try unchanged()
        try Data("test-session\n".utf8).write(to: marker)
        var corrupt = archive; corrupt[corrupt.count - 1] ^= 1
        let bad = root.appendingPathComponent("bad.ppb"); try corrupt.write(to: bad)
        rejects { try CompanionBackup.validate(bad) }
        let link = calendar.appendingPathComponent("link")
        try fm.createSymbolicLink(at: link, withDestinationURL: output)
        rejects { try CompanionBackup.create(pocketRoot: pocket, destination: output) }
        try unchanged()
        assert(CompanionBackup.allowed("calendar/calendar.bin"))
        assert(!CompanionBackup.allowed("calendar/../secrets"))
        assert(!CompanionBackup.allowed("/calendar/calendar.bin"))
        assert(!CompanionBackup.allowed("backups/old.ppb"))
        print("Direct companion backup tests passed: streaming copy, verification, replacement, stale session, corruption, symlinks and safe paths")
    }
}

import Foundation

// Mac-side recovery survives a lost or damaged microSD card. No automatic pruning.
enum CompanionVault {
    static var testDirectory:URL?
    static var base: URL {
        if let testDirectory {return testDirectory}
        return FileManager.default.urls(for:.applicationSupportDirectory,in:.userDomainMask)[0]
            .appendingPathComponent("PocketPDA/Backups",isDirectory:true)
    }
    static func deviceID(_ lease: PagerWriteLease) -> String {
        let prefix=lease.session.trimmingCharacters(in:.whitespacesAndNewlines).split(separator:"-").first.map(String.init) ?? "unknown"
        return prefix.allSatisfy(\.isHexDigit) && prefix.count == 12 ? prefix : String(format:"%08x",CompanionBackup.checksum(Data(lease.root.path.utf8)))
    }
    static func directory(_ lease: PagerWriteLease) -> URL {base.appendingPathComponent(deviceID(lease),isDirectory:true)}
    static func beforeChanges(_ lease: PagerWriteLease) throws {
        try lease.check()
        let directory=directory(lease)
        try FileManager.default.createDirectory(at:directory,withIntermediateDirectories:true)
        let token=lease.session.utf8.map {String(format:"%02x",$0)}.joined()
        // One full recovery point before the first change in each USB session;
        // the existing SD undo copies preserve individual subsequent edits.
        let marker=directory.appendingPathComponent("session-\(token).json")
        if FileManager.default.fileExists(atPath:marker.path) {
            let path=try JSONDecoder().decode(String.self,from:Data(contentsOf:marker))
            _ = try CompanionBackup.validate(directory.appendingPathComponent(path))
            return
        }
        let name="\(Int(Date().timeIntervalSince1970))-\(UUID().uuidString).ppb"
        let destination=directory.appendingPathComponent(name)
        try CompanionBackup.create(pocketRoot:lease.root,destination:destination)
        try lease.check()
        try JSONEncoder().encode(name).write(to:marker,options:.atomic)
    }
    static func archives(_ lease: PagerWriteLease) throws -> [URL] {
        let directory=directory(lease)
        guard FileManager.default.fileExists(atPath:directory.path) else {return []}
        return try FileManager.default.contentsOfDirectory(at:directory,includingPropertiesForKeys:[.creationDateKey])
            .filter {$0.pathExtension == "ppb"}.sorted {$0.lastPathComponent > $1.lastPathComponent}
    }
}

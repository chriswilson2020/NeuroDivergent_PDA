import Foundation

enum PagerDataError: LocalizedError {
    case invalid(String)
    var errorDescription: String? { if case .invalid(let value) = self { return value }; return nil }
}

enum PagerFieldKind {
    case text(Int), number(Int, Int, Int), toggle, choice([(Int, String)])
    case date(Int, Int), time, lines(Int, Int, Int)
}
struct PagerField: Identifiable {
    let label: String, offset: Int, kind: PagerFieldKind
    var id: String { label }
}
struct PagerSchema: Identifiable {
    let id: String, title: String, icon: String, path: String
    let magic: UInt32, size: Int, capacity: Int, titleOffset: Int, titleSize: Int
    let fields: [PagerField]
    let readOnly: Bool
    init(_ id: String, _ title: String, _ icon: String, _ path: String, _ magic: UInt32,
         _ size: Int, _ capacity: Int, _ titleOffset: Int, _ titleSize: Int,
         _ fields: [PagerField], readOnly: Bool = false) {
        self.id=id; self.title=title; self.icon=icon; self.path=path; self.magic=magic
        self.size=size; self.capacity=capacity; self.titleOffset=titleOffset; self.titleSize=titleSize
        self.fields=fields; self.readOnly=readOnly
    }
    static let recurrence = [(0,"Once"),(1,"Weekly"),(2,"Daily"),(3,"Weekdays"),(4,"Monthly")]
    static let all: [PagerSchema] = [
        .init("calendar", "Calendar", "calendar", "calendar/events.dat", 0x43414C31, 76, 65535, 15, 40, [
            .init(label:"Title",offset:15,kind:.text(40)), .init(label:"Location",offset:55,kind:.text(20)),
            .init(label:"Date",offset:4,kind:.date(6,7)), .init(label:"Start",offset:8,kind:.time), .init(label:"End",offset:10,kind:.time),
            .init(label:"Reminder (minutes)",offset:12,kind:.number(2,0,1440)),
            .init(label:"Repeats",offset:14,kind:.choice([(0,"Once"),(1,"Weekly"),(2,"Monthly"),(255,"Daily"),(190,"Weekdays"),(193,"Weekends")]))]),
        .init("tasks", "Tasks", "checklist", "tasks/tasks.dat", 0x54415331, 156, 64, 12, 48, [
            .init(label:"Title",offset:12,kind:.text(48)), .init(label:"Notes",offset:60,kind:.text(96)),
            .init(label:"Completed",offset:4,kind:.toggle), .init(label:"Priority",offset:5,kind:.choice([(0,"Low"),(1,"Normal"),(2,"High")])),
            .init(label:"Due date",offset:8,kind:.date(10,11)), .init(label:"Remind at 09:00",offset:7,kind:.toggle),
            .init(label:"Repeats",offset:6,kind:.choice(recurrence))]),
        .init("assignments", "Assignments", "list.bullet.clipboard", "assignments/assignments.dat", 0x41534731, 352, 24, 14, 48, [
            .init(label:"Title",offset:14,kind:.text(48)), .init(label:"Completed",offset:4,kind:.toggle),
            .init(label:"Priority",offset:5,kind:.choice([(0,"Low"),(1,"Normal"),(2,"High")])),
            .init(label:"Due date",offset:10,kind:.date(12,13)), .init(label:"Effort (minutes)",offset:8,kind:.number(2,1,1440)),
            .init(label:"Steps (one per line)",offset:62,kind:.lines(6,48,6))]),
        .init("packing", "Packing", "backpack", "packing/templates.dat", 0x50414331, 256, 12, 29, 32, [
            .init(label:"Title",offset:29,kind:.text(32)), .init(label:"Match event keyword",offset:5,kind:.text(24)),
            .init(label:"Items (one per line)",offset:61,kind:.lines(6,32,4))]),
        .init("routines", "Routines", "list.number", "routines/routines.dat", 0x52544E31, 428, 12, 5, 36, [
            .init(label:"Title",offset:5,kind:.text(36)), .init(label:"Steps (one per line)",offset:41,kind:.lines(8,48,4))]),
        .init("notes", "Notes", "note.text", "notes/index.dat", 0x4E4F5431, 56, 32, 8, 48, [
            .init(label:"Title",offset:8,kind:.text(48))]),
        .init("clock", "Timers & Alarms", "timer", "timers/presets.dat", 0x54494D31, 44, 12, 12, 32, [
            .init(label:"Name",offset:12,kind:.text(32)), .init(label:"Type",offset:4,kind:.choice([(0,"Countdown"),(1,"Daily alarm")])),
            .init(label:"Enabled",offset:5,kind:.toggle), .init(label:"Duration (minutes)",offset:8,kind:.number(2,1,1440)),
            .init(label:"Alarm time",offset:10,kind:.time), .init(label:"Vibration effect",offset:6,kind:.number(1,1,123))]),
        .init("habits", "Habits + Pet", "pawprint", "habits/habits.dat", 0x48414231, 48, 8, 16, 32, [
            .init(label:"Habit",offset:16,kind:.text(32))]),
        .init("messages", "Messages", "bubble.left.and.bubble.right", "messages/messages.dat", 0x4D534731, 192, 128, 24, 161, [], readOnly:true)
    ]
    static func named(_ id: String) -> PagerSchema { all.first { $0.id == id }! }
}

struct PagerRecord: Identifiable, Equatable {
    var bytes: Data
    var id: UInt32 { UInt32(integer(0, 4)) }
    func integer(_ offset: Int, _ width: Int = 1) -> UInt64 {
        (0..<width).reduce(0) { $0 | UInt64(bytes[offset+$1]) << ($1*8) }
    }
    mutating func setInteger(_ offset: Int, _ width: Int = 1, _ value: UInt64) {
        for i in 0..<width { bytes[offset+i] = UInt8(truncatingIfNeeded: value >> (i*8)) }
    }
    func text(_ offset: Int, _ size: Int) -> String {
        let value = bytes[offset..<(offset+size)].prefix { $0 != 0 }
        return String(decoding:value, as:UTF8.self)
    }
    mutating func setText(_ offset: Int, _ size: Int, _ value: String) throws {
        guard !value.contains("\0"), value.utf8.count < size else { throw PagerDataError.invalid("Text must fit in \(size-1) UTF-8 bytes. Nothing was truncated.") }
        let value = Data(value.utf8)
        bytes.replaceSubrange(offset..<(offset+size), with:value + Data(repeating:0,count:size-value.count))
    }
    func title(_ schema: PagerSchema) -> String { text(schema.titleOffset, schema.titleSize) }
    func date(_ year: Int, _ month: Int, _ day: Int, calendar: Calendar) -> Date? {
        guard integer(year,2) > 0 else { return nil }
        return calendar.date(from: DateComponents(year:Int(integer(year,2)),month:Int(integer(month)),day:Int(integer(day)),hour:12))
    }
}

enum PagerStoreCodec {
    static func decode(_ data: Data?, schema: PagerSchema) throws -> [PagerRecord] {
        guard let data else { return [] }
        guard data.count >= 16, CompanionBackup.u32(data,0) == schema.magic,
              CompanionBackup.u16(data,4) == 1, CompanionBackup.u16(data,6) == schema.size else {
            throw PagerDataError.invalid("Unsupported or damaged \(schema.title) format. Editing is disabled; no data was changed.")
        }
        let count = CompanionBackup.u16(data,8), body = Data(data.dropFirst(16))
        guard count <= schema.capacity, body.count == count*schema.size,
              CompanionBackup.checksum(body) == CompanionBackup.u32(data,12) else {
            throw PagerDataError.invalid("\(schema.title) checksum or record count is invalid. Restore a verified backup rather than overwriting it.")
        }
        let result = (0..<count).map { PagerRecord(bytes:body.subdata(in:($0*schema.size)..<(($0+1)*schema.size))) }
        guard result.allSatisfy({ $0.id != 0 }), Set(result.map(\.id)).count == result.count else {
            throw PagerDataError.invalid("\(schema.title) contains duplicate or invalid record IDs.")
        }
        return result
    }
    static func encode(_ records: [PagerRecord], schema: PagerSchema) throws -> Data {
        guard records.count <= schema.capacity, records.allSatisfy({ $0.bytes.count == schema.size && $0.id != 0 }),
              Set(records.map(\.id)).count == records.count else { throw PagerDataError.invalid("Invalid records or the pager's \(schema.capacity)-item limit was reached.") }
        let body = records.reduce(into:Data()) { $0.append($1.bytes) }
        var header = Data(); CompanionBackup.append32(schema.magic,to:&header)
        for value in [1,schema.size,records.count,0] { header.append(UInt8(value & 255)); header.append(UInt8(value >> 8)) }
        CompanionBackup.append32(CompanionBackup.checksum(body),to:&header)
        let data = header + body
        _ = try decode(data,schema:schema)
        return data
    }
    static func newRecord(schema: PagerSchema, existing: [PagerRecord], calendar: Calendar) throws -> PagerRecord {
        let maximum = existing.map(\.id).max() ?? 0
        guard maximum < UInt32.max else { throw PagerDataError.invalid("No unused record IDs remain.") }
        var record = PagerRecord(bytes:Data(repeating:0,count:schema.size))
        record.setInteger(0,4,UInt64(maximum)+1)
        if schema.id == "calendar" {
            let d=calendar.dateComponents([.year,.month,.day],from:Date())
            record.setInteger(4,2,UInt64(d.year!)); record.setInteger(6,1,UInt64(d.month!)); record.setInteger(7,1,UInt64(d.day!))
            record.setInteger(8,1,9); record.setInteger(10,1,10); record.setInteger(12,2,5)
        }
        if schema.id == "tasks" || schema.id == "assignments" { record.setInteger(5,1,1) }
        if schema.id == "assignments" { record.setInteger(8,2,30) }
        if schema.id == "clock" { record.setInteger(5,1,1); record.setInteger(6,1,47); record.setInteger(8,2,15); record.setInteger(10,1,7) }
        return record
    }
    static func occurs(_ event: PagerRecord, on date: Date, calendar: Calendar) -> Bool {
        guard let origin = event.date(4,6,7,calendar:calendar) else { return false }
        let day=calendar.startOfDay(for:date), first=calendar.startOfDay(for:origin)
        guard day >= first else { return false }
        let recurrence=event.integer(14), d=calendar.dateComponents([.day,.weekday],from:day)
        if recurrence == 0 { return day == first }
        if recurrence == 1 { return calendar.component(.weekday,from:first) == d.weekday }
        if recurrence == 2 { return calendar.component(.day,from:first) == d.day }
        return recurrence & 0x80 != 0 && recurrence & (1 << (d.weekday!-1)) != 0
    }
}

// A USB session is the write lease. Never write through a stale URL after a
// disconnect, SD swap, external edit or a staged CSV that would overwrite edits.
struct PagerWriteLease {
    let root: URL, session: String
    init(root: URL) throws {
        self.root=root.resolvingSymlinksInPath()
        session=try String(contentsOf:self.root.appendingPathComponent("commands/time.session"),encoding:.utf8)
        guard !session.isEmpty else { throw PagerDataError.invalid("Re-enter USB Disk Mode before editing.") }
    }
    func check() throws {
        guard (try? String(contentsOf:root.appendingPathComponent("commands/time.session"),encoding:.utf8)) == session else {
            throw PagerDataError.invalid("The pager disconnected or its USB session changed. Reconnect and reload before saving.")
        }
    }
    func save(_ data: Data, path: String, original: Data?) throws {
        try check()
        guard CompanionBackup.allowed(path), !path.contains("..") else { throw PagerDataError.invalid("Unsafe organizer path.") }
        let url=root.appendingPathComponent(path)
        let fm=FileManager.default
        guard url.resolvingSymlinksInPath().path.hasPrefix(root.path + "/") else {throw PagerDataError.invalid("Organizer path points outside the pager.")}
        let current=fm.fileExists(atPath:url.path) ? try Data(contentsOf:url) : nil
        guard current == original else { throw PagerDataError.invalid("This file changed outside the app. Reload before saving; no changes were overwritten.") }
        try CompanionVault.beforeChanges(self)
        try fm.createDirectory(at:url.deletingLastPathComponent(),withIntermediateDirectories:true)
        if let original {
            let undo=root.appendingPathComponent("desktop-undo/\(UUID().uuidString)/\(path)")
            try fm.createDirectory(at:undo.deletingLastPathComponent(),withIntermediateDirectories:true)
            try original.write(to:undo,options:.atomic)
        }
        try check()
        try data.write(to:url,options:.atomic)
        guard try Data(contentsOf:url) == data else { throw PagerDataError.invalid("The saved file could not be verified. The previous version is in desktop-undo.") }
    }
    func saveNote(body: Data,bodyPath: String,originalBody: Data?,index: Data,indexPath: String,originalIndex: Data?) throws {
        try check()
        let bodyURL=root.appendingPathComponent(bodyPath),indexURL=root.appendingPathComponent(indexPath),fm=FileManager.default
        guard [bodyPath,indexPath].allSatisfy({CompanionBackup.allowed($0) && !$0.contains("..")}),
              [bodyURL,indexURL].allSatisfy({$0.resolvingSymlinksInPath().path.hasPrefix(root.path + "/")}) else {throw PagerDataError.invalid("Unsafe note path.")}
        let currentBody=fm.fileExists(atPath:bodyURL.path) ? try Data(contentsOf:bodyURL) : nil
        let currentIndex=fm.fileExists(atPath:indexURL.path) ? try Data(contentsOf:indexURL) : nil
        guard currentBody == originalBody, currentIndex == originalIndex else {throw PagerDataError.invalid("The note changed outside this editor. Reload before saving.")}
        if body == originalBody {try save(index,path:indexPath,original:originalIndex);return}
        try save(body,path:bodyPath,original:originalBody)
        do {try save(index,path:indexPath,original:originalIndex)}
        catch {
            let initial=error
            do {
                try check()
                guard try Data(contentsOf:bodyURL) == body else {throw PagerDataError.invalid("Note body changed during rollback.")}
                if let originalBody {try originalBody.write(to:bodyURL,options:.atomic)} else {try fm.removeItem(at:bodyURL)}
            } catch {throw PagerDataError.invalid("The two-file note save was interrupted. Previous versions are retained in desktop-undo; reconnect before making further changes.")}
            throw initial
        }
    }
    func importFile(_ source: URL,path: String) throws {
        try check()
        guard path.hasPrefix("files/"), CompanionBackup.allowed(path) else {throw PagerDataError.invalid("Unsafe file path.")}
        let fm=FileManager.default,target=root.appendingPathComponent(path)
        let sourceValues=try source.resourceValues(forKeys:[.isRegularFileKey,.isSymbolicLinkKey])
        guard sourceValues.isRegularFile == true, sourceValues.isSymbolicLink != true else {throw PagerDataError.invalid("Choose a regular file, not a symbolic link or directory.")}
        guard target.resolvingSymlinksInPath().path.hasPrefix(root.path + "/"), !fm.fileExists(atPath:target.path) else {throw PagerDataError.invalid("A file with that name already exists, or the destination is unsafe.")}
        try CompanionVault.beforeChanges(self)
        try fm.createDirectory(at:target.deletingLastPathComponent(),withIntermediateDirectories:true)
        let temporary=target.deletingLastPathComponent().appendingPathComponent(".desktop-import-\(UUID().uuidString)")
        defer {try? fm.removeItem(at:temporary)}
        try fm.copyItem(at:source,to:temporary)
        try check()
        try fm.moveItem(at:temporary,to:target)
    }
}

import AppKit
import Foundation
import SwiftUI

struct DesktopContact: Identifiable, Decodable {
    let id: String, name: String
}
struct DesktopManifest: Decodable {
    let version: Int, deviceName: String, timezone: String, contacts: [DesktopContact]
    let radioEnabled: Bool
}

@MainActor
final class PagerDesktopModel: ObservableObject {
    @Published var records: [String:[PagerRecord]] = [:]
    @Published var errors: [String:String] = [:]
    @Published var status = "Connect your pager in USB Disk Mode."
    @Published var deviceName = "PocketPDA"
    @Published var contacts: [DesktopContact] = []
    @Published var radioEnabled=false
    @Published var calendar = Calendar(identifier:.gregorian)
    @Published var connected = false
    @Published var hasChanges = false
    @Published var lastSaved: Date?
    @Published var connectedAt=Date()
    private(set) var lease: PagerWriteLease?
    private var originals: [String:Data] = [:]
    private var nextIDs: [String:UInt32] = [:]

    func connect(_ root: URL?) {
        connectedAt=Date()
        if root != lease?.root {hasChanges=false;lastSaved=nil}
        records=[:]; errors=[:]; originals=[:]; nextIDs=[:]; contacts=[]; radioEnabled=false; lease=nil; connected=false
        calendar.timeZone=TimeZone(identifier:"Europe/Amsterdam")!
        guard let root else { status="Connect your pager, then choose Settings → USB DISK."; return }
        do {
            let lease=try PagerWriteLease(root:root); self.lease=lease; connected=true
            let manifestURL=root.appendingPathComponent("commands/desktop.json")
            if let data=try? Data(contentsOf:manifestURL), let manifest=try? JSONDecoder().decode(DesktopManifest.self,from:data), manifest.version == 1 {
                deviceName=manifest.deviceName; contacts=manifest.contacts;radioEnabled=manifest.radioEnabled
                calendar.timeZone=TimeZone(identifier:manifest.timezone) ?? calendar.timeZone
            } else { deviceName="PocketPDA" }
            for schema in PagerSchema.all {
                do {
                    let url=root.appendingPathComponent(schema.path)
                    let data = FileManager.default.fileExists(atPath:url.path) ? try Data(contentsOf:url) : nil
                    records[schema.id]=try PagerStoreCodec.decode(data,schema:schema)
                    originals[schema.id]=data
                    nextIDs[schema.id]=(records[schema.id]?.map(\.id).max() ?? 0)
                } catch { errors[schema.id]=error.localizedDescription }
            }
            status="Connected to \(deviceName). Edits save to the pager; eject when finished."
        } catch { status=error.localizedDescription }
    }
    func items(_ id: String) -> [PagerRecord] { records[id] ?? [] }
    func create(_ schema: PagerSchema) throws -> PagerRecord {
        guard connected, errors[schema.id] == nil, items(schema.id).count < schema.capacity else {
            throw PagerDataError.invalid("Connect a readable pager with room for another item first.")
        }
        var result=try PagerStoreCodec.newRecord(schema:schema,existing:items(schema.id),calendar:calendar)
        let high=nextIDs[schema.id] ?? 0
        guard high < UInt32.max else { throw PagerDataError.invalid("Record IDs exhausted.") }
        let next=max(UInt32(result.integer(0,4)),high+1)
        result.setInteger(0,4,UInt64(next)); nextIDs[schema.id]=next
        return result
    }
    func save(_ record: PagerRecord, schema: PagerSchema, body: String? = nil, originalBody: Data? = nil) throws {
        guard let lease, connected, !schema.readOnly, errors[schema.id] == nil else { throw PagerDataError.invalid("This section is not writable.") }
        guard !record.title(schema).trimmingCharacters(in:.whitespacesAndNewlines).isEmpty else { throw PagerDataError.invalid("Enter a title first.") }
        try validate(record,schema:schema)
        let importPath=String(schema.path.split(separator:"/").first!) + "/import.csv"
        guard !FileManager.default.fileExists(atPath:lease.root.appendingPathComponent(importPath).path) else {
            throw PagerDataError.invalid("A CSV import is waiting for this app. Eject to apply it, then reconnect before editing.")
        }
        var updated=items(schema.id)
        if let index=updated.firstIndex(where:{$0.id == record.id}) { updated[index]=record } else { updated.append(record) }
        if schema.id == "calendar" {
            updated.sort { a,b in
                let left=[a.integer(4,2),a.integer(6),a.integer(7),a.integer(8),a.integer(9),UInt64(a.id)]
                let right=[b.integer(4,2),b.integer(6),b.integer(7),b.integer(8),b.integer(9),UInt64(b.id)]
                return left.lexicographicallyPrecedes(right)
            }
        }
        let encoded=try PagerStoreCodec.encode(updated,schema:schema)
        // Body first: if index creation fails, only an unreferenced new body is
        // left behind, never an index pointing at a missing body.
        if schema.id == "notes", let body {
            try lease.saveNote(body:Data(body.utf8),bodyPath:notePath(record.id),originalBody:originalBody,index:encoded,indexPath:schema.path,originalIndex:originals[schema.id])
        } else {try lease.save(encoded,path:schema.path,original:originals[schema.id])}
        originals[schema.id]=encoded; records[schema.id]=updated
        status="Saved to \(deviceName). Eject to reload it on the pager."
        hasChanges=true;lastSaved=Date()
    }
    func remove(_ record: PagerRecord, schema: PagerSchema) throws {
        guard let lease, connected, !schema.readOnly, errors[schema.id] == nil else { throw PagerDataError.invalid("This section is not writable.") }
        let importPath=String(schema.path.split(separator:"/").first!) + "/import.csv"
        guard !FileManager.default.fileExists(atPath:lease.root.appendingPathComponent(importPath).path) else { throw PagerDataError.invalid("Apply the pending CSV import before editing.") }
        let updated=items(schema.id).filter { $0.id != record.id }
        let encoded=try PagerStoreCodec.encode(updated,schema:schema)
        try lease.save(encoded,path:schema.path,original:originals[schema.id])
        originals[schema.id]=encoded; records[schema.id]=updated
        // Keep deleted note bodies recoverable; removing the index entry hides
        // the note on the pager without destroying its text.
        status="Item removed. Its previous index is retained in desktop-undo."
        hasChanges=true;lastSaved=Date()
    }
    func replaceCalendar(_ updated: [PagerRecord]) throws {
        guard let lease,errors["calendar"] == nil else {throw PagerDataError.invalid("Connect a readable calendar first.")}
        guard !FileManager.default.fileExists(atPath:lease.root.appendingPathComponent("calendar/import.csv").path) else {throw PagerDataError.invalid("Eject to apply the pending CSV before editing.")}
        for record in updated {try validate(record,schema:.named("calendar"))}
        let data=try PagerStoreCodec.encode(updated,schema:.named("calendar"))
        try lease.save(data,path:"calendar/events.dat",original:originals["calendar"])
        originals["calendar"]=data;records["calendar"]=updated;nextIDs["calendar"]=updated.map(\.id).max() ?? 0
        hasChanges=true;lastSaved=Date();status="Calendar saved to \(deviceName). Eject to apply changes."
    }
    func notePath(_ id: UInt32) -> String { String(format:"notes/%08x.txt",id) }
    func noteBody(_ id: UInt32) throws -> Data? {
        guard let lease else { return nil }; try lease.check()
        let url=lease.root.appendingPathComponent(notePath(id))
        return FileManager.default.fileExists(atPath:url.path) ? try Data(contentsOf:url) : nil
    }
    func toggleHabit(_ item: PagerRecord) throws {
        let today=dayKey(Date()), yesterday=dayKey(calendar.date(byAdding:.day,value:-1,to:Date())!)
        var record=item
        if record.integer(4,4) == today {
            record.setInteger(4,4,record.integer(8,4)); record.setInteger(12,2,record.integer(14,2))
            record.setInteger(8,4,0); record.setInteger(14,2,0)
        } else {
            record.setInteger(8,4,record.integer(4,4)); record.setInteger(14,2,record.integer(12,2))
            record.setInteger(4,4,today)
            record.setInteger(12,2,record.integer(8,4) == yesterday ? min(65535,record.integer(14,2)+1) : 1)
        }
        try save(record,schema:.named("habits"))
    }
    func completeTask(_ item: PagerRecord) throws {
        var record=item
        if record.integer(4) == 0, record.integer(6) != 0,
           let due=record.date(8,10,11,calendar:calendar) {
            let recurrence=record.integer(6)
            var next=calendar.date(byAdding:recurrence == 4 ? .month : .day,
                                   value:recurrence == 1 ? 7 : 1,to:due)!
            if recurrence == 3 {
                while [1,7].contains(calendar.component(.weekday,from:next)) { next=calendar.date(byAdding:.day,value:1,to:next)! }
            }
            let d=calendar.dateComponents([.year,.month,.day],from:next)
            record.setInteger(8,2,UInt64(d.year!));record.setInteger(10,1,UInt64(d.month!));record.setInteger(11,1,UInt64(d.day!))
            record.setInteger(4,1,0)
        } else {record.setInteger(4,1,record.integer(4) == 0 ? 1 : 0)}
        try save(record,schema:.named("tasks"))
    }
    func completeAssignment(_ item: PagerRecord) throws {
        var record=item
        if record.integer(7)+1 >= record.integer(6) {record.setInteger(4,1,1)}
        else {record.setInteger(7,1,record.integer(7)+1)}
        try save(record,schema:.named("assignments"))
    }
    func dayKey(_ date: Date) -> UInt64 {
        let d=calendar.dateComponents([.year,.month,.day],from:date)
        return UInt64(d.year!*10000+d.month!*100+d.day!)
    }
    func events(on date: Date) -> [PagerRecord] {
        items("calendar").filter { PagerStoreCodec.occurs($0,on:date,calendar:calendar) }
            .sorted { ($0.integer(8)*60+$0.integer(9)) < ($1.integer(8)*60+$1.integer(9)) }
    }
    func contactName(_ record: PagerRecord) -> String {
        let id=String(format:"%016llx",record.integer(8,8))
        return contacts.first { $0.id.lowercased() == id }?.name ?? id
    }
    func queueMessage(contact: String, text: String) throws {
        guard let lease, connected, radioEnabled, contacts.contains(where:{$0.id == contact}), let id=UInt64(contact,radix:16),
              !text.trimmingCharacters(in:.whitespacesAndNewlines).isEmpty, text.utf8.count <= 160, !text.contains("\0") else {
            throw PagerDataError.invalid("Choose a paired contact and enter 1–160 UTF-8 bytes.")
        }
        try lease.check()
        let folder=lease.root.appendingPathComponent("messages/desktop-outbox")
        try FileManager.default.createDirectory(at:folder,withIntermediateDirectories:true)
        let existing=try FileManager.default.contentsOfDirectory(at:folder,includingPropertiesForKeys:nil).filter { $0.pathExtension == "msg" }
        guard existing.count < 16 else { throw PagerDataError.invalid("16 desktop messages are already queued. Eject so the pager can process them first.") }
        var data=Data(); CompanionBackup.append32(0x444D5131,to:&data)
        data.append(contentsOf:[1,0,UInt8(text.utf8.count),0])
        for i in 0..<8 { data.append(UInt8(truncatingIfNeeded:id >> (i*8))) }
        CompanionBackup.append32(0,to:&data); CompanionBackup.append32(UInt32(Date().timeIntervalSince1970),to:&data)
        let payload=Data(text.utf8)+Data([0])
        CompanionBackup.append32(CompanionBackup.checksum(Data(data.dropFirst(8))+payload),to:&data)
        data.append(payload)
        let filename=UUID().uuidString.replacingOccurrences(of:"-",with:"").lowercased()+".msg"
        try lease.save(data,path:"messages/desktop-outbox/\(filename)",original:nil)
        status="Message queued on the SD card. Eject to hand it to the pager's radio; delivery is not yet confirmed."
        hasChanges=true;lastSaved=Date()
    }
    private func validate(_ record: PagerRecord, schema: PagerSchema) throws {
        for field in schema.fields {
            switch field.kind {
            case .date(let month,let day):
                if record.integer(field.offset,2) == 0 {
                    guard schema.id != "calendar" else { throw PagerDataError.invalid("Calendar events need a date.") }; continue
                }
                let year=Int(record.integer(field.offset,2)), m=Int(record.integer(month)), d=Int(record.integer(day))
                guard (2020...2099).contains(year), let date=calendar.date(from:DateComponents(year:year,month:m,day:d)),
                      calendar.component(.month,from:date) == m, calendar.component(.day,from:date) == d else { throw PagerDataError.invalid("Enter a valid date in 2020–2099.") }
            case .time:
                guard record.integer(field.offset) < 24, record.integer(field.offset+1) < 60 else { throw PagerDataError.invalid("Invalid time.") }
            case .number(let width,let low,let high):
                guard (UInt64(low)...UInt64(high)).contains(record.integer(field.offset,width)) else { throw PagerDataError.invalid("\(field.label) must be \(low)–\(high).") }
            case .lines(let count,_,let countOffset):
                guard record.integer(countOffset) <= count else { throw PagerDataError.invalid("Too many steps or items.") }
            default: break
            }
        }
        if schema.id == "calendar" && record.integer(10)*60+record.integer(11) <= record.integer(8)*60+record.integer(9) { throw PagerDataError.invalid("End time must be after start time on the same day.") }
        if schema.id == "assignments" && record.integer(6) > 0 && record.integer(7) >= record.integer(6) { throw PagerDataError.invalid("Current assignment step is outside the step list.") }
    }
}

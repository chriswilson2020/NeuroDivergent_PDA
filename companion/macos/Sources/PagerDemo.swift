import Foundation

enum PagerDemo {
    // --demo is an explicit QA mode; it never scans /Volumes or touches a pager.
    static func create() throws -> URL {
        let volume=FileManager.default.temporaryDirectory.appendingPathComponent("PocketPDA-Desktop-Demo-\(UUID().uuidString)")
        let root=volume.appendingPathComponent("PocketPDA")
        try FileManager.default.createDirectory(at:root.appendingPathComponent("commands"),withIntermediateDirectories:true)
        try Data("demo-session\n".utf8).write(to:root.appendingPathComponent("commands/time.session"))
        let manifest="{\"version\":1,\"deviceName\":\"Demo pager · sample data\",\"timezone\":\"Europe/Amsterdam\",\"radioEnabled\":true,\"contacts\":[{\"id\":\"1122334455667788\",\"name\":\"Demo contact\"}]}"
        try Data(manifest.utf8).write(to:root.appendingPathComponent("commands/desktop.json"))
        var calendar=Calendar(identifier:.gregorian);calendar.timeZone=TimeZone(identifier:"Europe/Amsterdam")!
        let samples:[String:[String]]=["calendar":["Design","Mathematics","Music"],"tasks":["Pack tomorrow's bag","Finish the science worksheet","Bring a water bottle"],"assignments":["Science project"],"packing":["PE kit"],"routines":["Get ready for school"],"notes":["Things to remember"],"clock":["Focus","Break"],"habits":["Drink water","Move for five minutes","Plan tomorrow"]]
        for schema in PagerSchema.all {
            var records:[PagerRecord]=[]
            for (i,title) in (samples[schema.id] ?? []).enumerated() {
                var r=try PagerStoreCodec.newRecord(schema:schema,existing:records,calendar:calendar)
                try r.setText(schema.titleOffset,schema.titleSize,title)
                if schema.id == "calendar" {r.setInteger(8,1,UInt64(8+i*2));r.setInteger(9,1,30);r.setInteger(10,1,UInt64(9+i*2));r.setInteger(11,1,30);try r.setText(55,20,"Room \(100+i)")}
                if schema.id == "routines" {r.setInteger(4,1,3);for (j,step) in ["Get dressed","Eat breakfast","Check your bag"].enumerated(){try r.setText(41+j*48,48,step)}}
                if schema.id == "packing" {r.setInteger(4,1,3);try r.setText(5,24,"Physical");for (j,item) in ["Sports clothes","Sports shoes","Water bottle"].enumerated(){try r.setText(61+j*32,32,item)}}
                if schema.id == "assignments" {r.setInteger(6,1,3);for (j,step) in ["Choose a question","Make a plan","Write the first paragraph"].enumerated(){try r.setText(62+j*48,48,step)}}
                records.append(r)
            }
            let url=root.appendingPathComponent(schema.path);try FileManager.default.createDirectory(at:url.deletingLastPathComponent(),withIntermediateDirectories:true)
            try PagerStoreCodec.encode(records,schema:schema).write(to:url)
        }
        try Data("A calm place for ideas, reminders and the little things that matter.\n\nThis is sample data, not a connected pager.".utf8).write(to:root.appendingPathComponent("notes/00000001.txt"))
        return volume
    }
}

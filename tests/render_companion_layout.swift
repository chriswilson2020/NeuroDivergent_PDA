// Render the app's actual views on synthetic data, not the user's desktop or SD.
import AppKit
import SwiftUI

@main struct CompanionLayoutRender {
    @MainActor static func main() throws {
        _ = NSApplication.shared
        let root=try PagerDemo.create().appendingPathComponent("PocketPDA")
        let model=PagerDesktopModel();model.connect(root)
        let companion=CompanionModel(demoMode:true)
        companion.deviceRoot=root.deletingLastPathComponent()
        companion.calendarPreview=[.init(id:"test",title:"New lesson",location:"Room 100",start:Date(),end:Date().addingTimeInterval(3600),reminder:5)]
        let output=URL(fileURLWithPath:CommandLine.arguments[1])
        try FileManager.default.createDirectory(at:output,withIntermediateDirectories:true)
        for size in [CGSize(width:580,height:450),CGSize(width:800,height:600),CGSize(width:1300,height:800)] {
            for section in ["tasks","packing","assignments","tasks-selected","packing-selected","notes-selected","habits","routines","clock","messages","settings","calendar-editor","device","diagnostics","bulk","sync","firmware","preview"] {
                if section == "calendar-editor" && size.width < 800 {continue}
                if section == "assignments" {model.records["assignments"]=[]}
                let name=section.replacingOccurrences(of:"-selected",with:"")
                let selection=section.hasSuffix("-selected") ? model.items(name).first?.id : nil
                let content:AnyView
                switch section {
                case "sync":content=AnyView(CompanionToolsView(companion:companion,desktop:model))
                case "firmware":if size.width < 800 {continue};content=AnyView(CompanionFirmwareView(companion:companion))
                case "preview":if size.width < 800 {continue};content=AnyView(CompanionCalendarPreview(companion:companion,desktop:model))
                case "device": content=AnyView(CompanionDashboardView(model:model))
                case "diagnostics": content=AnyView(CompanionDiagnosticsView(model:model))
                case "bulk": if size.width < 800 {continue};content=AnyView(CompanionCalendarToolsView(model:model))
                case "settings": content=AnyView(DesktopSettingsView(model:model))
                case "calendar-editor": content=AnyView(PagerRecordEditor(model:model,schema:.named("calendar"),record:model.items("calendar")[0]))
                default: content=AnyView(DesktopRecordsView(model:model,schema:.named(name),selectedRecord:selection))
                }
                let renderSize=CGSize(width:size.width,height:section == "calendar-editor" ? max(660,size.height) : size.height)
                let view=content.frame(width:renderSize.width,height:renderSize.height)
                    .background(Color.white).environment(\.colorScheme,.light)
                let hosting=NSHostingView(rootView:view)
                let window=NSWindow(contentRect:CGRect(origin:.zero,size:renderSize),styleMask:.borderless,backing:.buffered,defer:false)
                window.isReleasedWhenClosed=false;window.contentView=hosting
                hosting.frame=CGRect(origin:.zero,size:renderSize)
                hosting.layoutSubtreeIfNeeded()
                RunLoop.current.run(until:Date().addingTimeInterval(0.2))
                hosting.layoutSubtreeIfNeeded()
                guard let bitmap=hosting.bitmapImageRepForCachingDisplay(in:hosting.bounds) else {fatalError("Cannot render layout")}
                hosting.cacheDisplay(in:hosting.bounds,to:bitmap)
                guard let png=bitmap.representation(using:.png,properties:[:]) else {fatalError("Cannot encode layout")}
                try png.write(to:output.appendingPathComponent("\(section)-\(Int(size.width)).png"))
                window.close()
            }
        }
        print("Rendered organizer layouts to \(output.path)")
    }
}

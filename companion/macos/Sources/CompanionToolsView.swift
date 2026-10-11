import AppKit
import SwiftUI
import Charts

struct CompanionToolsView: View {
    @ObservedObject var companion: CompanionModel
    @ObservedObject var desktop: PagerDesktopModel
    @State private var archives:[URL]=[]
    @State private var restore:URL?
    @State private var restoreSession:String?
    @State private var issue:String?
    @State private var firmware=false
    var body: some View {
        ScrollView {
            VStack(alignment:.leading,spacing:20) {
                GroupBox("Save and apply") {
                    VStack(alignment:.leading,spacing:10) {
                        Label(desktop.hasChanges ? "Saved to \(desktop.deviceName) · eject to apply" : "Connected to \(desktop.deviceName)",systemImage:desktop.hasChanges ? "checkmark.circle.fill" : "externaldrive")
                        if let time=desktop.lastSaved {Text("Last saved \(time.formatted(date:.omitted,time:.standard))").font(.caption)}
                        Text("Edits are verified on microSD immediately. Safely eject to reload the pager and resume messaging. One full Mac backup is created before the first edit in each USB session; individual edits also keep SD undo copies.").font(.callout).foregroundStyle(.secondary)
                        DesktopFlow {
                            Button("Eject and Apply") {companion.eject()}
                            Button("Firmware Update…") {firmware=true}
                            Button("Open Backup Folder") {openBackups()}
                        }
                    }.frame(maxWidth:.infinity,alignment:.leading).padding(8)
                }
                SyncToolsView(model:companion)
                GroupBox("Recovery points for this pager") {
                    VStack(alignment:.leading,spacing:10) {
                        Button("Refresh Backups") {refresh()}
                        if archives.isEmpty {Text("Your first edit creates a recovery point here. Backups are kept on this Mac, separately for each pager.").foregroundStyle(.secondary)}
                        ForEach(archives,id:\.path) {url in
                            HStack {
                                VStack(alignment:.leading) {
                                    Text(((try? url.resourceValues(forKeys:[.creationDateKey]))?.creationDate ?? .distantPast).formatted()).font(.headline)
                                    Text(url.lastPathComponent).font(.caption).lineLimit(1)
                                }
                                Spacer()
                                Button("Stage Restore…") {restoreSession=desktop.lease?.session;restore=url}
                            }
                        }
                    }.padding(8)
                }
            }.padding(20)
        }.onAppear {refresh()}
        .sheet(isPresented:$firmware) {CompanionFirmwareView(companion:companion)}
        .confirmationDialog("Replace the pager's organizer with this backup?",isPresented:Binding(get:{restore != nil},set:{if !$0 {restore=nil}}),titleVisibility:.visible) {
            Button("Verify and Stage Restore",role:.destructive) {if let restore {perform {
                guard desktop.lease?.session == restoreSession else {throw PagerDataError.invalid("The connected pager changed. Reopen its backup browser.")}
                try companion.stageRestore(from:restore)
            }};restore=nil}
        } message:{Text("A fresh recovery point is created first. Nothing is restored until you eject and confirm RESTORE on the pager.")}
        .alert("Companion",isPresented:Binding(get:{issue != nil},set:{if !$0 {issue=nil}})) {Button("OK") {}} message:{Text(issue ?? "")}
    }
    private func perform(_ action:() throws -> Void) {do {try action()} catch {issue=error.localizedDescription}}
    private func refresh() {perform {if let lease=desktop.lease {archives=try CompanionVault.archives(lease)}}}
    private func openBackups() {perform {try FileManager.default.createDirectory(at:CompanionVault.base,withIntermediateDirectories:true);NSWorkspace.shared.open(CompanionVault.base)}}
}

struct CompanionDashboardView: View {
    @ObservedObject var model: PagerDesktopModel
    @State private var manifest:[String:Any]=[:]
    var body:some View {
        ScrollView {
            VStack(alignment:.leading,spacing:18) {
                Text(model.deviceName).font(.largeTitle.bold())
                Text("Hardware snapshots are taken when USB Disk Mode starts, not live while the disk is mounted.").foregroundStyle(.secondary)
                GroupBox("Device") {
                    VStack(alignment:.leading,spacing:12) {
                        LabeledContent("Device ID",value:model.lease.map(CompanionVault.deviceID) ?? "Unknown")
                        LabeledContent("Firmware",value:manifest["firmware"] as? String ?? "Not reported by this firmware")
                        LabeledContent("Timezone",value:model.calendar.timeZone.identifier)
                        LabeledContent("Radio",value:model.radioEnabled ? "Enabled · paused during USB Disk Mode" : "Disabled")
                        if let soc=manifest["batteryPercent"] as? Int {LabeledContent("Battery at connection",value:"\(soc)%")}
                        if let charging=manifest["charging"] as? Bool {LabeledContent("Charging at connection",value:charging ? "Yes" : "No")}
                        if let epoch=manifest["rtcEpoch"] as? Double,epoch > 0 {
                            LabeledContent("RTC at USB entry",value:Date(timeIntervalSince1970:epoch).formatted())
                            LabeledContent("Approx. RTC difference at load",value:"\(Int(epoch-model.connectedAt.timeIntervalSince1970)) seconds (includes connection delay)")
                        } else {Text("Clock comparison needs a fresh firmware snapshot and Mac connection timestamp.").font(.caption).foregroundStyle(.secondary)}
                        if let root=model.lease?.root,let values=try? root.resourceValues(forKeys:[.volumeAvailableCapacityKey,.volumeTotalCapacityKey]) {
                            LabeledContent("Storage free",value:ByteCountFormatter.string(fromByteCount:Int64(values.volumeAvailableCapacity ?? 0),countStyle:.file))
                            LabeledContent("Storage total",value:ByteCountFormatter.string(fromByteCount:Int64(values.volumeTotalCapacity ?? 0),countStyle:.file))
                        }
                    }.padding(10)
                }
                GroupBox("Organizer") {ForEach(PagerSchema.all) {schema in LabeledContent(schema.title,value:"\(model.items(schema.id).count) items").padding(4)}}
            }.padding(24)
        }.onAppear {
            if let root=model.lease?.root,let data=try? Data(contentsOf:root.appendingPathComponent("commands/desktop.json")),let object=try? JSONSerialization.jsonObject(with:data) as? [String:Any] {manifest=object}
        }
    }
}

struct PowerSample: Identifiable {
    let id:Int, epoch:Double, uptime:Double, soc:Double, usb:Bool, cpuSleep:Double, cpuAwake:Double, rx:Double, tx:Double, errors:Double
}
enum CompanionDiagnostics {
    static func parse(_ text:String)->[PowerSample] {
        let lines=text.split(whereSeparator:\.isNewline)
        guard let header=lines.first else {return []}
        let keys=header.split(separator:",",omittingEmptySubsequences:false).map(String.init)
        let required=["epoch","uptime_ms","soc","usb","cpu_sleep_ms","cpu_awake_ms","packets_rx","packets_tx","sleep_errors"]
        guard required.allSatisfy(keys.contains) else {return []}
        return lines.dropFirst().enumerated().compactMap {index,line in
            let fields=line.split(separator:",",omittingEmptySubsequences:false)
            guard fields.count == keys.count else {return nil}
            if let index=keys.firstIndex(of:"gauge_valid"),fields[index] != "1" {return nil}
            var v:[String:Double]=[:]
            for key in required {guard let value=Double(fields[keys.firstIndex(of:key)!]),value.isFinite,value >= 0 else {return nil};v[key]=value}
            guard v["soc"]! <= 100 else {return nil}
            return PowerSample(id:index,epoch:v["epoch"]!,uptime:v["uptime_ms"]!,soc:v["soc"]!,usb:v["usb"]! != 0,cpuSleep:v["cpu_sleep_ms"]!,cpuAwake:v["cpu_awake_ms"]!,rx:v["packets_rx"]!,tx:v["packets_tx"]!,errors:v["sleep_errors"]!)
        }
    }
}
struct CompanionDiagnosticsView:View {
    @ObservedObject var model:PagerDesktopModel
    @State private var samples:[PowerSample]=[]
    @State private var issue:String?
    var body:some View {
        ScrollView {
            VStack(alignment:.leading,spacing:18) {
                Text("Power & Radio").font(.title.bold())
                Text("Recorded samples, not live readings. Cumulative counters reset when the pager restarts. USB charging samples are not a battery-drain test.").foregroundStyle(.secondary)
                DesktopFlow {
                    Button("Reload Logs") {load()}
                    Button("Open Log Folder") {if let root=model.lease?.root {NSWorkspace.shared.open(root.appendingPathComponent("logs"))}}
                    Button("Export Logs…") {export()}
                }
                if let issue {Text(issue).foregroundStyle(.red)}
                if samples.isEmpty {ContentUnavailableView("No power samples",systemImage:"chart.xyaxis.line",description:Text("Keep using the pager, then reconnect in USB Disk Mode."))}
                else {
                    Chart(samples) {sample in PointMark(x:.value("Sample",sample.id),y:.value("Battery %",sample.soc)).foregroundStyle(sample.usb ? Color.orange : Color.accentColor)}
                        .chartYScale(domain:0...100).frame(height:220)
                    Text("Orange: USB connected · Blue: battery operation").font(.caption)
                    if let last=samples.last {
                        LabeledContent("Latest battery",value:"\(Int(last.soc))%")
                        let total=last.cpuSleep+last.cpuAwake
                        LabeledContent("CPU asleep (latest boot)",value:total > 0 ? String(format:"%.1f%%",last.cpuSleep/total*100) : "No timing data")
                        LabeledContent("Radio packets (latest boot)",value:"\(Int(last.rx)) received · \(Int(last.tx)) transmitted")
                        LabeledContent("Sleep errors (latest boot)",value:"\(Int(last.errors))")
                        Chart {
                            BarMark(x:.value("State","Asleep"),y:.value("Minutes",last.cpuSleep/60000))
                            BarMark(x:.value("State","Awake"),y:.value("Minutes",last.cpuAwake/60000))
                        }.frame(height:150)
                    }
                    let pairs=zip(samples,samples.dropFirst()).filter {a,b in !a.usb && !b.usb && b.uptime > a.uptime && b.epoch > a.epoch && b.epoch-a.epoch < 600 && abs((b.uptime-a.uptime)/1000-(b.epoch-a.epoch)) < 2}
                    let seconds=pairs.reduce(0.0){$0+$1.1.epoch-$1.0.epoch},drop=pairs.reduce(0.0){$0+$1.0.soc-$1.1.soc}
                    if seconds >= 1800 {LabeledContent("Observed unplugged drain",value:String(format:"%.1f percentage points/hour",drop/seconds*3600))}
                    else {Text("At least 30 minutes of consecutive unplugged samples are needed for a drain estimate.").font(.caption)}
                }
                Text("Export includes raw power and sleep logs. Message retries remain visible per message in Messages; packet counts alone do not measure delivery reliability.").font(.caption).foregroundStyle(.secondary)
                let outgoing=model.items("messages").filter {$0.integer(20) == 1}
                if !outgoing.isEmpty {
                    Text("Message delivery history").font(.headline)
                    Chart(outgoing) {message in
                        BarMark(x:.value("Local message",String(message.id)),y:.value("Attempts",Int(message.integer(23))))
                    }.frame(height:160)
                    Text("\(outgoing.filter {$0.integer(21) == 2}.count) delivered · \(outgoing.filter {$0.integer(21) == 3}.count) failed · \(outgoing.filter {$0.integer(21) < 2}.count) queued/sending").font(.callout)
                }
            }.padding(24)
        }.onAppear {load()}
    }
    private func load() {
        do {
            guard let lease=model.lease else {return};try lease.check()
            let url=lease.root.appendingPathComponent("logs/power.csv")
            guard (try url.resourceValues(forKeys:[.fileSizeKey])).fileSize ?? 0 <= 20_000_000 else {throw PagerDataError.invalid("Log exceeds 20 MB; export it instead.")}
            samples=CompanionDiagnostics.parse(try String(contentsOf:url,encoding:.utf8));issue=nil
        } catch {issue=error.localizedDescription}
    }
    private func export() {
        let panel=NSOpenPanel();panel.canChooseDirectories=true;panel.canChooseFiles=false;panel.prompt="Export Here"
        guard panel.runModal() == .OK,let destination=panel.url,let lease=model.lease else {return}
        do {try lease.check();let folder=destination.appendingPathComponent("PocketPDA-Logs-\(UUID().uuidString)");try FileManager.default.createDirectory(at:folder,withIntermediateDirectories:true)
            for name in ["power.csv","sleep.csv","power.previous.csv","sleep.previous.csv"] {let source=lease.root.appendingPathComponent("logs/\(name)");if FileManager.default.fileExists(atPath:source.path) {try FileManager.default.copyItem(at:source,to:folder.appendingPathComponent(name))}}
        } catch {issue=error.localizedDescription}
    }
}

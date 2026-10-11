import AppKit
import SwiftUI

struct CompanionFirmwareView:View {
    @ObservedObject var companion:CompanionModel
    @Environment(\.dismiss) private var dismiss
    @State private var image:URL?
    @State private var ports:[String]=[]
    @State private var port=""
    @State private var output=""
    @State private var running=false
    @State private var verified=false
    @State private var confirmed=false
    @State private var completed=false
    var body:some View {
        VStack(alignment:.leading,spacing:16) {
            Text("Safe firmware update").font(.title2.bold())
            Text("Connect only the pager you intend to update. First back up in USB Disk Mode, then eject and leave Disk Mode so its serial port becomes available.").fixedSize(horizontal:false,vertical:true)
            DesktopFlow {
                Button("1. Back Up to Mac…") {companion.requestBackup()}.disabled(!companion.connected || companion.busy)
                Button("2. Eject Pager") {companion.eject()}.disabled(!companion.connected || companion.busy)
                Button("3. Choose Application .bin…") {choose()}
                Button("Use Included Firmware") {
                    if let url=Bundle.main.url(forResource:"PocketPDA-tlora_pager",withExtension:"bin") {validate(url)}
                    else {output="No firmware is bundled with this app build. Choose an application .bin instead."}
                }
            }
            Text(image?.lastPathComponent ?? "Choose a PocketPDA application-only firmware.bin, never a factory image.").font(.caption)
            HStack {
                Picker("Serial port",selection:$port) {Text("Select port").tag("");ForEach(ports,id:\.self) {Text($0).tag($0)}}
                Button("Refresh") {refresh()}
            }
            Toggle("I have a verified backup and only the intended pager is connected",isOn:$confirmed)
            Text("Writes at 0x10000 only; NVS settings and microSD are preserved. Requires PlatformIO's Python with esptool installed. The app does not erase or factory-reset the device.").font(.caption).foregroundStyle(.secondary)
            ScrollView {Text(output.isEmpty ? "Update output will appear here." : output).font(.system(.caption,design:.monospaced)).textSelection(.enabled).frame(maxWidth:.infinity,alignment:.leading)}.frame(minHeight:140,maxHeight:240).padding(10).background(.quaternary,in:RoundedRectangle(cornerRadius:8))
            if completed {
                Text("Upload completed. Return to USB Disk Mode, then set the clock below. Reconnect once more to confirm the RTC result.").foregroundStyle(.green)
                Button("4. Set Pager Time and Eject") {companion.syncTime()}.disabled(!companion.connected)
            }
            HStack {
                Button("Close") {dismiss()}.disabled(running)
                Spacer()
                if running {ProgressView().controlSize(.small)}
                Button("Update Selected Pager") {run()}.buttonStyle(.borderedProminent)
                    .disabled(!verified || port.isEmpty || !confirmed || companion.connected || companion.busy || running)
            }
        }.padding(24).frame(width:640).disabled(running)
            .onAppear {refresh()}.interactiveDismissDisabled(running)
    }
    private func refresh() {ports=((try? FileManager.default.contentsOfDirectory(atPath:"/dev")) ?? []).filter {$0.hasPrefix("cu.usbmodem") || $0.hasPrefix("cu.usbserial")}.sorted().map {"/dev/"+$0};if !ports.contains(port) {port=""}}
    private func choose() {
        let panel=NSOpenPanel();panel.allowedContentTypes=[.data]
        guard panel.runModal() == .OK,let url=panel.url else {return}
        validate(url)
    }
    private func validate(_ url:URL) {
        do {
            let data=try Data(contentsOf:url)
            guard !url.lastPathComponent.lowercased().contains("factory"),data.count >= 40,data.count <= 0x300000,data[0] == 0xe9,data[12] == 9,data[13] == 0,Array(data[32..<36]) == [0x32,0x54,0xcd,0xab] else {throw PagerDataError.invalid("Not an application-only ESP32-S3 firmware image. Factory/merged images are refused.")}
            image=url;verified=true;completed=false;output="Image format checked. Confirm that it was built for PocketPDA T-LoRa Pager."
        } catch {verified=false;output=error.localizedDescription}
    }
    private func run() {
        guard let image,let script=Bundle.main.url(forResource:"flash_update",withExtension:"py"),ports.contains(port) else {output="Update helper or selected port unavailable.";return}
        let python=FileManager.default.homeDirectoryForCurrentUser.appendingPathComponent(".platformio/penv/bin/python")
        guard FileManager.default.isExecutableFile(atPath:python.path) else {output="PlatformIO Python was not found at \(python.path). Install PlatformIO before updating.";return}
        running=true;completed=false;output="Uploading to \(port)…\n"
        let selectedPort=port
        Task {
            let result=await Task.detached(priority:.userInitiated) {() -> (Int32,String) in
                let process=Process(),pipe=Pipe();process.executableURL=python;process.arguments=[script.path,"--port",selectedPort,"--image",image.path]
                process.standardOutput=pipe;process.standardError=pipe
                do {try process.run();let data=pipe.fileHandleForReading.readDataToEndOfFile();process.waitUntilExit();return (process.terminationStatus,String(decoding:data.suffix(200_000),as:UTF8.self))}
                catch {return (-1,error.localizedDescription)}
            }.value
            output=result.1;running=false;completed=result.0 == 0
            if !completed {output += "\nUpdate failed. Refresh ports if the pager reappeared under a ROM port. No automatic retry targets another device."}
        }
    }
}

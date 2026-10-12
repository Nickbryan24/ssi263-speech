// The app's import as the app runs it -- FirmwareInstaller.inspect and Job.commit over the real native side
// (SSI263Core's macos-arm64 slice) -- on the repository's own firmware, in a folder of its own in place of the App
// Group's: each unit's files end up in the unit folder, labelled, and SsiShared sees each voice as installed -- and
// the Mockingboard from the toolkit's disk image, in MOCKINGBOARD_DISKS (paths.local).  Then the Braille Lite unit
// tab's catalog on what was imported, and two units started (a release unit's first state made).  Skips what is not
// there.
//
//     sh src/platforms/apple/test/test_installer.sh
//     SSI263_UNIT_BREAK=factory sh ...      the control: a release unit's first state not made, so its start must FAIL
import Foundation

let repo = URL(fileURLWithPath: CommandLine.arguments[1])
let root = FileManager.default.temporaryDirectory.appendingPathComponent("ssi263-installer-\(getpid())")
try? FileManager.default.createDirectory(at: root, withIntermediateDirectories: true)
SsiShared.containerOverride = root
var failures = 0

let disks = CommandLine.arguments.count > 2 ? URL(fileURLWithPath: CommandLine.arguments[2]) : nil

func run(_ label: String, _ files: [String], expect: [String], kept: [String] = [], from base: URL? = nil) {
    let urls = files.map { (base ?? repo.appendingPathComponent("firmware")).appendingPathComponent($0) }
    guard urls.allSatisfy({ FileManager.default.fileExists(atPath: $0.path) }) else {
        print("skip  \(label): its firmware is not there")
        return
    }
    do {
        let plan = try FirmwareInstaller.inspect(urls)
        if let r = plan.refusal { throw FirmwareInstaller.Failure(message: "refused: " + r) }
        let job = FirmwareInstaller.Job(plan: plan)
        let labels = try job.commit()
        let installed = SsiShared.voices.filter(SsiShared.installed).map { $0.key }
        let unit = SsiShared.unitFolder!
        let ok = expect.allSatisfy(installed.contains)
            && kept.allSatisfy { FileManager.default.fileExists(atPath: unit.appendingPathComponent($0).path) }
        print("\(ok ? "ok   " : "FAIL ") \(label): \(labels.joined(separator: "; ")); installed: \(installed)")
        if !ok { failures += 1 }
    } catch {
        print("FAIL  \(label): \(error.localizedDescription)")
        failures += 1
    }
}

run("the Speak-Out", ["gw-micro-speakout/SPEAKOUT.HEX"], expect: ["speakout"])
run("the Accents", ["aicom-accent-sa/u2.BIN", "aicom-accent-sa/u3.BIN", "aicom-accent-sa/u4.BIN",
                    "aicom-accent-mini/SPKEMS.DVC"], expect: ["accentsa", "accentmini"])
run("the English Braille Lite", ["blazie/BL2ENG.BNS"], expect: ["braillelite"])
run("the Mockingboard's file", ["sweet-micro-mockingboard/mockingboard-tts-1.1.bin"], expect: ["mockingboard"])
run("the Slovak Braille 'n Speak 2000 (its state made with the English recipe)", ["blazie/bns2000/BS2SLL.BNS"],
    expect: ["braillenspeak-sk"])
run("the German Braille Lite", ["blazie/german/Lite2000.BNS"], expect: ["braillelite-de"])
run("the Italian Braille Lite", ["blazie/italian/Lite2000.BNS"], expect: ["braillelite-it"])
// the release table's: a release voice (its state by its own recipe), and a release kept for the unit tab alone
run("the Hungarian Braille Lite (a release voice)", ["blazie/bl2000/BL2HUN.BNS"], expect: ["braillelite-hu"],
    kept: ["BL2HUN.BNS", "bl2_hu_1999_07.state", "BL2HUN.BNS.label"])
run("the Japanese Braille Lite (the unit tab's alone, beside the Hungarian)", ["blazie/bl2000/BL2JPN.BNS"],
    expect: ["braillelite-hu"], kept: ["units/bl2_ja.BNS", "units/bl2_ja.BNS.label"])
if let d = disks {
    run("the Mockingboard from the toolkit's disk image", ["Sweet Micro Systems Mockingboard Developers toolkit 1984.dsk"],
        expect: ["mockingboard"], from: d)
    run("the early Mockingboard from Mockingboard disk 1's image", ["mockingboard1.dsk"],
        expect: ["mockingboard-early"], from: d)
} else {
    print("skip  the toolkit's disk image: MOCKINGBOARD_DISKS (paths.local) is not set")
}

// The Braille Lite unit tab on what was imported: the catalog finds each Blazie unit in the unit folder (the shipped
// ones under their flat names, the releases by content, units/ too), and a release unit's first start makes its
// state and runs: the unit boots and speaks (its PCM not silent within its first seconds).  No sound is played.
UnitCatalog.savedOverride = root.appendingPathComponent("saved", isDirectory: true)
func unitCase(_ label: String, _ ok: Bool, _ detail: String) {
    print("\(ok ? "ok   " : "FAIL ") \(label): \(detail)")
    if !ok { failures += 1 }
}
let entries = UnitCatalog.list()
let want = ["bl-en", "bl-de", "bl-it", "bns-sk", "bl2_hu_1999_07", "bl2_ja"].filter { k in
    k == "bl2_ja" || k == "bl2_hu_1999_07" ? FileManager.default.fileExists(atPath: repo.path + "/firmware/blazie/bl2000")
        : true
}
let keys = entries.map { $0.key }
unitCase("the unit tab's units", want.allSatisfy(keys.contains), keys.joined(separator: ", "))
for key in ["bl2_hu_1999_07", "bl-en"] {
    guard let e = entries.first(where: { $0.key == key }) else { continue }
    do {
        let state = try UnitCatalog.prepare(e)
        var err = [CChar](repeating: 0, count: 300)
        guard let u = emu_create(e.unit.kind, UnitCatalog.firmware(e), state, 22050, 0, &err, Int32(err.count)) else {
            throw UnitCatalog.Failure(message: String(cString: err))
        }
        var pcm = [Int16](repeating: 0, count: 22050)
        var loud = 0
        for _ in 0..<8 where loud == 0 {
            emu_render(u, &pcm, Int32(pcm.count))
            loud = pcm.reduce(0) { max($0, abs(Int($1))) }
        }
        emu_destroy(u)
        unitCase("the \(e.name) starts from \(state.map { URL(fileURLWithPath: $0).lastPathComponent } ?? "cold")",
                 loud > 1000, "peak \(loud) in its first seconds")
    } catch {
        unitCase("the \(e.name) starts", false, error.localizedDescription)
    }
}

try? FileManager.default.removeItem(at: root)
print(failures == 0 ? "the import installs every unit, and the unit tab runs them" : "FAILED: \(failures)")
exit(failures == 0 ? 0 : 1)

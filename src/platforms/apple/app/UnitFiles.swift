// UnitFiles.swift -- files from this device loaded into a Blazie unit's flash, as the Windows and GTK shells' "Load
// programs and files into flash" (main_gtk.c add_programs): on the unit's saved memory while it is switched off --
// its file system read from the state (bl_files_state.h, bl_files.h), the files added to the flash startup folder in
// one batch, all or nothing (bl_files_xfer.h blx_add_cp), checked, the state as it was kept beside it as
// STATE.before-import, the new one written.  The tab (UnitModel) switches the unit off first and on again after.
// MIT.

import Foundation

enum UnitFiles {
    /// A file to load: its name on this device (no folder), its bytes, its time.
    struct File {
        let name: String
        let data: Data
        let modified: Date
    }

    enum Outcome: Equatable {
        case loaded(added: Int, replaced: Int)
        case exists([String])               // names the unit has already: nothing done; ask, then load with replace
        case failed(String)                 // nothing done, the reason in words
    }

    /// The files the tab's picker offers first (the GTK shell's "Programs and documents"): Blazie programs, BASIC,
    /// text and braille; any file goes.
    static let kinds = ["bns", "bas", "txt", "brl", "brf"]

    /// The files into the unit saved at `state` (its names in `codepage`, emc_unit's), replacing those of the same
    /// name only when `replace`.
    static func load(_ files: [File], into state: String, codepage: Int32, replace: Bool) -> Outcome {
        guard !files.isEmpty else { return .failed("No files were chosen.") }
        var err = [CChar](repeating: 0, count: 400)
        var unit = bls_unit()
        guard bls_load(state, &unit, &err, Int32(err.count)) == 1 else {
            return .failed("The unit's memory could not be read: \(String(cString: err)).")
        }
        defer { bls_free(&unit) }
        guard let fs = blf_open(unit.model, unit.ram, unit.flash, unit.flash_size, &err, Int32(err.count)) else {
            return .failed("The unit's files could not be read: \(String(cString: err)).")
        }
        defer { blf_close(fs) }

        var report = blx_report()
        blx_report_init(&report)
        defer { blx_report_free(&report) }
        let names = files.map { strdup($0.name) }
        defer { names.forEach { free($0) } }
        let datas = files.map { f -> UnsafeMutablePointer<UInt8> in
            let p = UnsafeMutablePointer<UInt8>.allocate(capacity: max(f.data.count, 1))
            f.data.copyBytes(to: p, count: f.data.count)
            return p
        }
        defer { datas.forEach { $0.deallocate() } }
        var sizes = files.map { UInt($0.data.count) }
        var times = files.map { dos($0.modified).time }
        var dates = files.map { dos($0.modified).date }
        let namePtrs: [UnsafePointer<CChar>?] = names.map { UnsafePointer($0) }
        let dataPtrs: [UnsafePointer<UInt8>?] = datas.map { UnsafePointer($0) }
        let got = namePtrs.withUnsafeBufferPointer { n in
            dataPtrs.withUnsafeBufferPointer { d in
                blx_add_cp(fs, codepage, Int32(files.count), n.baseAddress, d.baseAddress, &sizes, &times, &dates,
                           replace ? 1 : 0, &report, &err, Int32(err.count))
            }
        }
        if got == BLX_ADD_EXISTS {
            let log = report.log.map { String(cString: $0) } ?? ""
            return .exists(log.split(separator: "\n").filter { $0.hasPrefix("exists ") }.map { String($0.dropFirst(7)) })
        }
        guard got == 1, blf_check(fs, &err, Int32(err.count)) == 1 else {
            return .failed(String(cString: err))
        }
        let before = state + ".before-import"
        try? FileManager.default.removeItem(atPath: before)
        try? FileManager.default.copyItem(atPath: state, toPath: before)
        guard bls_save(state, &unit, &err, Int32(err.count)) == 1 else {
            return .failed("The unit's memory could not be written: \(String(cString: err)).")
        }
        return .loaded(added: Int(report.added), replaced: Int(report.replaced))
    }

    /// The file's time as the unit keeps it (FAT's: two-second steps, from 1980).
    private static func dos(_ d: Date) -> (time: UInt32, date: UInt32) {
        let c = Calendar.current.dateComponents([.year, .month, .day, .hour, .minute, .second], from: d)
        let y: Int = max((c.year ?? 1980) - 1980, 0), mo: Int = c.month ?? 1, day: Int = c.day ?? 1
        let h: Int = c.hour ?? 0, mi: Int = c.minute ?? 0, sec: Int = c.second ?? 0
        let time: Int = (h << 11) | (mi << 5) | (sec / 2)
        let date: Int = (y << 9) | (mo << 5) | day
        return (UInt32(time), UInt32(date))
    }
}

// UnitModel.swift -- the Braille Lite unit tab's state: the units the imported firmware can run (UnitCatalog), the
// one running (UnitRunner) and its display, its sound's volume and the quick key response, and its memory saved when
// it is left or the app goes to the background.  The tab (UnitView) shows it.  MIT.

import SwiftUI
import os

private let log = Logger(subsystem: "com.ssi263speech.app", category: "unit")

@MainActor
final class UnitModel: ObservableObject {
    @Published private(set) var units: [UnitCatalog.Entry] = []
    @Published private(set) var running: String?            // the running unit's key
    @Published private(set) var cells = ""                  // its display, as Unicode braille
    @Published private(set) var busy = false
    @Published private(set) var status = ""
    @AppStorage("unit_last") var chosen = ""                // the app's own settings, not the App Group's
    @AppStorage("unit_volume") var volume = 100 { didSet { runner.setVolume(volume) } }
    @AppStorage("unit_quick") var quick = false { didSet { runner.setQuick(quick) } }

    private let runner = UnitRunner()
    private var timer: Timer?

    var actions: BrailleFieldActions {
        let r = runner
        return BrailleFieldActions(key: { r.key($0, down: $1) }, bars: { r.bars($0) }, released: { r.releaseKeys() })
    }

    func refresh() {
        units = UnitCatalog.list()
        if !units.contains(where: { $0.key == chosen }), let first = units.first { chosen = first.key }
    }

    /// The chosen unit started (the one running saved and stopped first); a release unit's first start makes its
    /// state, a few seconds, off the main thread.
    func start() {
        guard let e = units.first(where: { $0.key == chosen }), !busy else { return }
        busy = true
        var u = e.unit
        let first = emc_needs_factory(&u) != 0 && UnitCatalog.savedPath(e).map {
            !FileManager.default.fileExists(atPath: $0.path)
        } == true
        say(first ? "Setting up the \(e.name) for its first start; this takes a few seconds."
            : "Starting the \(e.name).")
        runner.stop()
        running = nil
        Task.detached {
            let prepared = Result { try UnitCatalog.prepare(e) }
            await MainActor.run {
                defer { self.busy = false }
                do {
                    _ = try prepared.get()
                    try self.runner.start(e, volume: self.volume, quick: self.quick)
                    self.running = e.key
                    self.say("The \(e.name) is running. The braille field has the keyboard.")
                    self.watch()
                } catch {
                    self.say(error.localizedDescription)
                }
            }
        }
    }

    /// The unit saved and stopped.
    func stop() {
        guard running != nil else { return }
        let name = runner.running?.name ?? "unit"
        runner.stop()
        running = nil
        cells = ""
        timer?.invalidate()
        say("The \(name) is switched off; its memory is kept.")
    }

    /// The unit's memory saved, as when it is left (the app going to the background).
    func save() {
        if !runner.save() { log.error("the unit's memory could not be saved") }
    }

    /// The display read 20 times a second, as the GTK shell reads it.
    private func watch() {
        timer?.invalidate()
        timer = Timer.scheduledTimer(withTimeInterval: 0.05, repeats: true) { [weak self] _ in
            Task { @MainActor in
                guard let self = self, self.running != nil else { return }
                let now = self.runner.braille()
                if now != self.cells { self.cells = now }
            }
        }
    }

    private func say(_ s: String) {
        status = s
        announce(s)
        log.notice("\(s, privacy: .public)")
    }
}

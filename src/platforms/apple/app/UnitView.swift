// UnitView.swift -- the Braille Lite unit tab: run a Blazie unit on the firmware already imported for the voices
// (Setup's), from a Mac's or a Bluetooth keyboard -- the unit, its braille field, its sound's settings.  Built for
// VoiceOver first: the field is one element holding the display, the steps are announced.  MIT.

import SwiftUI

struct UnitView: View {
    @StateObject private var model = UnitModel()
    @Environment(\.scenePhase) private var phase

    var body: some View {
        Form {
            if model.units.isEmpty {
                Section {
                    Text("Import a Braille Lite's firmware in Setup first: this tab runs the units whose firmware " +
                         "you have imported.")
                }
            } else {
                Section {
                    Picker("Unit", selection: $model.chosen) {
                        ForEach(model.units) { Text($0.name).tag($0.key) }
                    }
                    Button(model.running == nil ? "Switch on" : "Switch on the chosen unit") { model.start() }
                        .disabled(model.busy)
                    if model.running != nil {
                        Button("Switch off") { model.stop() }
                    }
                    if !model.status.isEmpty { Text(model.status) }
                } header: {
                    Text("Unit").accessibilityAddTraits(.isHeader)
                } footer: {
                    Text("The unit keeps its own memory, files and settings, saved when it is switched off or the " +
                         "app is left.")
                }

                if model.running != nil {
                    Section {
                        BrailleField(text: model.cells, actions: model.actions)
                            .frame(minHeight: 48)
                    } header: {
                        Text("Braille display").accessibilityAddTraits(.isHeader)
                    } footer: {
                        Text(keysHelp)
                    }
                }

                Section {
                    SettingSlider(title: "Volume", value: $model.volume, range: 0...100)
                    Toggle("Quick key response (faster than the real unit)", isOn: $model.quick)
                } header: {
                    Text("Sound").accessibilityAddTraits(.isHeader)
                }
            }
        }
        .navigationTitle("Braille Lite unit")
        .onAppear { model.refresh() }
        .onChange(of: phase) { p in if p != .active { model.save() } }
    }

    private var keysHelp: String {
        var s = "With the braille display field focused: F, D, S, J, K and L are dots 1 to 6, with the space bar; " +
            "semicolon and A are the advance and back bars. Alt+Shift+Right and Left hold the display's bars."
        #if os(iOS)
        s += " With VoiceOver, turn Quick Nav off (Left and Right arrows together) so the keys reach the unit."
        #endif
        return s
    }
}

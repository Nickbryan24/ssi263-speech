// UnitKeys.swift -- the Braille Lite's keyboard on a Mac's or a Bluetooth keyboard, by the keys' places (not their
// letters, so another layout's keyboard chords the same): bl_keys.c's defaults, as every emulator shell has them --
// F D S J K L dots 1 to 6, the space bar, ; the advance bar and A the back bar (the Braille Lite 2000 has two) -- and
// the braille field's Alt+Shift+Right and Left, the display's bars held as long as the arrow is.  MIT.

#if os(iOS)
import UIKit
#endif

enum UnitKeys {
    static let dots: [Int32] = (1...6).map { 1 << ($0 - 1) }      // chords.h CHORD_DOT(1...6)

    /// macOS: the keys' virtual key codes (Carbon's kVK_ANSI_*), each its chords.h bit.
    static let mac: [UInt16: Int32] = [
        3: dots[0], 2: dots[1], 1: dots[2],            // F D S
        38: dots[3], 40: dots[4], 37: dots[5],         // J K L
        49: Int32(CHORD_SPACE),                        // the space bar
        41: Int32(CHORD_ADVANCE),                      // ;
        0: Int32(CHORD_BACK),                          // A
    ]
    static let macRight: UInt16 = 124, macLeft: UInt16 = 123

    #if os(iOS)
    /// iOS: a hardware keyboard's keys by their HID usage.
    static let ios: [UIKeyboardHIDUsage: Int32] = [
        .keyboardF: dots[0], .keyboardD: dots[1], .keyboardS: dots[2],
        .keyboardJ: dots[3], .keyboardK: dots[4], .keyboardL: dots[5],
        .keyboardSpacebar: Int32(CHORD_SPACE),
        .keyboardSemicolon: Int32(CHORD_ADVANCE),
        .keyboardA: Int32(CHORD_BACK),
    ]
    #endif

    /// emu_braille_bars' bits: forward, back.
    static let forwardBar: Int32 = 1, backBar: Int32 = 2
}

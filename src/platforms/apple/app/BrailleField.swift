// BrailleField.swift -- the Braille Lite unit tab's braille field, the Windows and GTK emulators' pattern: a
// read-only element holding the unit's display as Unicode braille (U+2800 + the dots), which the screen reader shows
// on the person's braille display, and which keeps the keyboard -- the unit's six keys, space bar and advance bars
// (UnitKeys), going down and up as the hand does -- and never edits.  Alt+Shift+Right and Left are the display's
// forward and back bars, held as long as the arrow is.  Whether VoiceOver sends such a field to braille alone is for
// the testers to say.  MIT.

import SwiftUI

/// What the field does with the keys: the runner's.
struct BrailleFieldActions {
    var key: (Int32, Bool) -> Void          // a chords.h bit went down (true) or up
    var bars: (Int32) -> Void               // the bars held now (UnitKeys.forwardBar, backBar)
    var released: () -> Void                // the keyboard went elsewhere: everything comes up
}

#if os(macOS)
import AppKit

struct BrailleField: NSViewRepresentable {
    let text: String
    let actions: BrailleFieldActions

    func makeNSView(context: Context) -> BrailleFieldView {
        let v = BrailleFieldView()
        v.actions = actions
        DispatchQueue.main.async { v.window?.makeFirstResponder(v) }
        return v
    }

    func updateNSView(_ v: BrailleFieldView, context: Context) {
        v.actions = actions
        v.cells = text
    }
}

final class BrailleFieldView: NSView {
    var actions: BrailleFieldActions?
    private var held: Int32 = 0                 // the bars down from the arrows
    private let label = NSTextField(labelWithString: "")

    var cells = "" {
        didSet {
            guard cells != oldValue else { return }
            label.stringValue = cells
            NSAccessibility.post(element: self, notification: .valueChanged)
        }
    }

    override init(frame: NSRect) {
        super.init(frame: frame)
        label.font = .systemFont(ofSize: 28)
        label.setAccessibilityElement(false)
        label.translatesAutoresizingMaskIntoConstraints = false
        addSubview(label)
        NSLayoutConstraint.activate([label.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 8),
                                     label.centerYAnchor.constraint(equalTo: centerYAnchor)])
    }

    required init?(coder: NSCoder) { fatalError("not from a coder") }

    override var acceptsFirstResponder: Bool { true }
    override var intrinsicContentSize: NSSize { NSSize(width: NSView.noIntrinsicMetric, height: 48) }

    override func resignFirstResponder() -> Bool {
        held = 0
        actions?.released()
        return true
    }

    override func mouseDown(with event: NSEvent) { window?.makeFirstResponder(self) }

    private func bar(_ e: NSEvent) -> Int32? {
        guard e.modifierFlags.contains([.option, .shift]) || held != 0 else { return nil }
        return e.keyCode == UnitKeys.macRight ? UnitKeys.forwardBar : e.keyCode == UnitKeys.macLeft ? UnitKeys.backBar : nil
    }

    override func keyDown(with e: NSEvent) {
        if let b = bar(e) {
            held |= b
            actions?.bars(held)
        } else if let bit = UnitKeys.mac[e.keyCode], !e.modifierFlags.contains(.command) {
            if !e.isARepeat { actions?.key(bit, true) }
        } else {
            super.keyDown(with: e)              // the app's own shortcuts (Command-Q ...)
        }
    }

    override func keyUp(with e: NSEvent) {
        if e.keyCode == UnitKeys.macRight || e.keyCode == UnitKeys.macLeft {
            let b = e.keyCode == UnitKeys.macRight ? UnitKeys.forwardBar : UnitKeys.backBar
            if held & b != 0 {
                held &= ~b
                actions?.bars(held)
            }
        } else if let bit = UnitKeys.mac[e.keyCode] {
            actions?.key(bit, false)
        }
    }

    // the screen reader's: a read-only field holding the display
    override func isAccessibilityElement() -> Bool { true }
    override func accessibilityRole() -> NSAccessibility.Role? { .textField }
    override func accessibilityLabel() -> String? { "Braille display" }
    override func accessibilityValue() -> Any? { cells }
    override func isAccessibilityEnabled() -> Bool { true }
    override func accessibilityPerformPress() -> Bool { window?.makeFirstResponder(self) ?? false }
}

#else
import UIKit

struct BrailleField: UIViewRepresentable {
    let text: String
    let actions: BrailleFieldActions

    func makeUIView(context: Context) -> BrailleFieldView {
        let v = BrailleFieldView()
        v.actions = actions
        DispatchQueue.main.async { _ = v.becomeFirstResponder() }
        return v
    }

    func updateUIView(_ v: BrailleFieldView, context: Context) {
        v.actions = actions
        v.cells = text
    }
}

final class BrailleFieldView: UIView {
    var actions: BrailleFieldActions?
    private var held: Int32 = 0
    private let label = UILabel()

    var cells = "" {
        didSet {
            guard cells != oldValue else { return }
            label.text = cells
        }
    }

    override init(frame: CGRect) {
        super.init(frame: frame)
        label.font = .systemFont(ofSize: 28)
        label.isAccessibilityElement = false
        label.translatesAutoresizingMaskIntoConstraints = false
        addSubview(label)
        NSLayoutConstraint.activate([label.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 8),
                                     label.centerYAnchor.constraint(equalTo: centerYAnchor)])
        isAccessibilityElement = true
        accessibilityLabel = "Braille display"
        accessibilityTraits = [.updatesFrequently]
        addGestureRecognizer(UITapGestureRecognizer(target: self, action: #selector(take)))
    }

    required init?(coder: NSCoder) { fatalError("not from a coder") }

    override var intrinsicContentSize: CGSize { CGSize(width: UIView.noIntrinsicMetric, height: 48) }
    override var canBecomeFirstResponder: Bool { true }
    override var accessibilityValue: String? { get { cells } set {} }

    @objc private func take() { _ = becomeFirstResponder() }

    override func accessibilityActivate() -> Bool { becomeFirstResponder() }

    override func resignFirstResponder() -> Bool {
        held = 0
        actions?.released()
        return super.resignFirstResponder()
    }

    private func arrow(_ k: UIKey) -> Int32? {
        k.keyCode == .keyboardRightArrow ? UnitKeys.forwardBar : k.keyCode == .keyboardLeftArrow ? UnitKeys.backBar : nil
    }

    override func pressesBegan(_ presses: Set<UIPress>, with event: UIPressesEvent?) {
        var rest = Set<UIPress>()
        for p in presses {
            guard let k = p.key else { rest.insert(p); continue }
            if let b = arrow(k), k.modifierFlags.contains([.alternate, .shift]) {
                held |= b
                actions?.bars(held)
            } else if let bit = UnitKeys.ios[k.keyCode], !k.modifierFlags.contains(.command) {
                actions?.key(bit, true)
            } else {
                rest.insert(p)
            }
        }
        if !rest.isEmpty { super.pressesBegan(rest, with: event) }
    }

    override func pressesEnded(_ presses: Set<UIPress>, with event: UIPressesEvent?) {
        up(presses, event) { super.pressesEnded($0, with: $1) }
    }

    override func pressesCancelled(_ presses: Set<UIPress>, with event: UIPressesEvent?) {
        up(presses, event) { super.pressesCancelled($0, with: $1) }
    }

    private func up(_ presses: Set<UIPress>, _ event: UIPressesEvent?, _ pass: (Set<UIPress>, UIPressesEvent?) -> Void) {
        var rest = Set<UIPress>()
        for p in presses {
            guard let k = p.key else { rest.insert(p); continue }
            if let b = arrow(k), held & b != 0 {
                held &= ~b
                actions?.bars(held)
            } else if let bit = UnitKeys.ios[k.keyCode] {
                actions?.key(bit, false)
            } else {
                rest.insert(p)
            }
        }
        if !rest.isEmpty { pass(rest, event) }
    }
}
#endif

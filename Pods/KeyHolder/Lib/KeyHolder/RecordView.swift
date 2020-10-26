//
//  RecordView.swift
//
//  KeyHolder
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Copyright © 2015-2020 Clipy Project.
//

import Cocoa
import Carbon
import Magnet
import Sauce

public protocol RecordViewDelegate: AnyObject {
    func recordViewShouldBeginRecording(_ recordView: RecordView) -> Bool
    func recordView(_ recordView: RecordView, canRecordKeyCombo keyCombo: KeyCombo) -> Bool
    func recordView(_ recordView: RecordView, didChangeKeyCombo keyCombo: KeyCombo?)
    func recordViewDidEndRecording(_ recordView: RecordView)
}

@IBDesignable
open class RecordView: NSView {

    // MARK: - Properties
    @IBInspectable open var backgroundColor: NSColor = .controlColor {
        didSet { layer?.backgroundColor = backgroundColor.cgColor }
    }
    @IBInspectable open var tintColor: NSColor = .controlAccentPolyfill {
        didSet { needsDisplay = true }
    }
    @IBInspectable open var borderColor: NSColor = .controlColor {
        didSet { layer?.borderColor = borderColor.cgColor }
    }
    @IBInspectable open var borderWidth: CGFloat = 0 {
        didSet { layer?.borderWidth = borderWidth }
    }
    @IBInspectable open var cornerRadius: CGFloat = 0 {
        didSet {
            layer?.cornerRadius = cornerRadius
            noteFocusRingMaskChanged()
        }
    }
    open var clearButtonMode: RecordView.ClearButtonMode = .always {
        didSet { needsDisplay = true }
    }

    open weak var delegate: RecordViewDelegate?
    open var didChange: ((KeyCombo?) -> Void)?
    @objc dynamic open private(set) var isRecording = false
    open var keyCombo: KeyCombo? {
        didSet { needsDisplay = true }
    }
    open var isEnabled = true {
        didSet {
            needsDisplay = true
            if !isEnabled { endRecording() }
            noteFocusRingMaskChanged()
        }
    }

    private let clearButton = ClearButton()
    private let modifierEventHandler = ModifierEventHandler()
    private let validModifiers: [NSEvent.ModifierFlags] = [.shift, .control, .option, .command]
    private let validModifiersText: [NSString] = ["⇧", "⌃", "⌥", "⌘"]
    private var inputModifiers = NSEvent.ModifierFlags()
    private var fontSize: CGFloat {
        return bounds.height / 1.7
    }
    private var clearSize: CGFloat {
        return fontSize / 1.3
    }
    private var marginY: CGFloat {
        return (bounds.height - fontSize) / 2.6
    }
    private var marginX: CGFloat {
        return marginY * 1.5
    }
    private var isFirstResponder: Bool {
        return (isEnabled && window?.firstResponder == self && isRecording)
    }

    // MARK: - Override Properties
    open override var isOpaque: Bool {
        return false
    }
    open override var isFlipped: Bool {
        return true
    }
    open override var focusRingMaskBounds: NSRect {
        return (isFirstResponder) ? bounds : NSRect.zero
    }

    // MARK: - Initialize
    public override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        initView()
    }

    public required init?(coder: NSCoder) {
        super.init(coder: coder)
        initView()
    }

    private func initView() {
        // Layer
        wantsLayer = true
        layer?.backgroundColor = backgroundColor.cgColor
        layer?.borderColor = borderColor.cgColor
        layer?.borderWidth = borderWidth
        layer?.cornerRadius = cornerRadius
        // Clear Button
        clearButton.target = self
        clearButton.action = #selector(RecordView.clearAndEndRecording)
        addSubview(clearButton)
        // Double Tap
        modifierEventHandler.doubleTapped = { [weak self] modifierFlags in
            guard let strongSelf = self else { return }
            guard strongSelf.isFirstResponder else { return }
            guard let keyCombo = KeyCombo(doubledCocoaModifiers: modifierFlags) else { return }
            guard self?.delegate?.recordView(strongSelf, canRecordKeyCombo: keyCombo) ?? true else { return }
            self?.keyCombo = keyCombo
            self?.didChange?(keyCombo)
            self?.delegate?.recordView(strongSelf, didChangeKeyCombo: keyCombo)
            self?.endRecording()
        }
    }

    // MARK: - Draw
    open override func drawFocusRingMask() {
        guard isFirstResponder else { return }
        NSBezierPath(roundedRect: bounds, xRadius: cornerRadius, yRadius: cornerRadius).fill()
    }

    override open func draw(_ dirtyRect: NSRect) {
        layer?.backgroundColor = backgroundColor.cgColor
        layer?.borderColor = borderColor.cgColor
        drawModifiers(dirtyRect)
        drawKeyCode(dirtyRect)
        drawClearButton(dirtyRect)
    }

    private func drawModifiers(_ dirtyRect: NSRect) {
        let fontSize = self.fontSize
        let modifiers = keyCombo?.modifiers.convertSupportCocoaModifiers() ?? inputModifiers
        for (i, text) in validModifiersText.enumerated() {
            let rect = NSRect(x: marginX + (fontSize * CGFloat(i)), y: marginY, width: fontSize, height: bounds.height)
            text.draw(in: rect, withAttributes: modifierTextAttributes(modifiers, checkModifier: validModifiers[i]))
        }
    }

    private func drawKeyCode(_ dirtyRext: NSRect) {
        guard let keyCombo = self.keyCombo else { return }
        let fontSize = self.fontSize
        let minX = (fontSize * 4) + (marginX * 2)
        let width = bounds.width - minX - (marginX * 2) - clearSize
        guard width > 0 else { return }
        let text = (keyCombo.doubledModifiers) ? "double tap" : keyCombo.keyEquivalent.uppercased()
        text.draw(in: NSRect(x: minX, y: marginY, width: width, height: bounds.height), withAttributes: keyCodeTextAttributes())
    }

    private func drawClearButton(_ dirtyRext: NSRect) {
        let clearSize = self.clearSize
        let x = bounds.width - clearSize - marginX
        let y = (bounds.height - clearSize) / 2
        clearButton.frame = NSRect(x: x, y: y, width: clearSize, height: clearSize)
        switch clearButtonMode {
        case .always:
            clearButton.isHidden = false
        case .never:
            clearButton.isHidden = true
        case .whenRecorded:
            clearButton.isHidden = (keyCombo == nil)
        }
    }

    // MARK: - NSResponder
    override open var acceptsFirstResponder: Bool {
        return isEnabled
    }

    override open var canBecomeKeyView: Bool {
        return super.canBecomeKeyView && NSApp.isFullKeyboardAccessEnabled
    }

    override open var needsPanelToBecomeKey: Bool {
        return true
    }

    open override func becomeFirstResponder() -> Bool {
        return focusView()
    }

    override open func resignFirstResponder() -> Bool {
        unfocusView()
        return super.resignFirstResponder()
    }

    open override func cancelOperation(_ sender: Any?) {
        endRecording()
    }

    override open func keyDown(with theEvent: NSEvent) {
        guard !performKeyEquivalent(with: theEvent) else { return }
        super.keyDown(with: theEvent)
    }

    override open func performKeyEquivalent(with theEvent: NSEvent) -> Bool {
        guard isFirstResponder else { return false }
        guard let key = Sauce.shared.key(by: Int(theEvent.keyCode)) else { return false }

        if theEvent.modifierFlags.carbonModifiers() != 0 {
            let modifiers = theEvent.modifierFlags.carbonModifiers()
            guard let keyCombo = KeyCombo(key: key, carbonModifiers: modifiers) else { return false }
            guard delegate?.recordView(self, canRecordKeyCombo: keyCombo) ?? true else { return false }
            self.keyCombo = keyCombo
            didChange?(keyCombo)
            delegate?.recordView(self, didChangeKeyCombo: keyCombo)
            endRecording()
            return true
        } else if key.isFunctionKey {
            guard let keyCombo = KeyCombo(key: key, cocoaModifiers: []) else { return false }
            guard delegate?.recordView(self, canRecordKeyCombo: keyCombo) ?? true else { return false }
            self.keyCombo = keyCombo
            didChange?(keyCombo)
            delegate?.recordView(self, didChangeKeyCombo: keyCombo)
            endRecording()
            return true
        }
        return false
    }

    override open func flagsChanged(with theEvent: NSEvent) {
        guard isFirstResponder else {
            inputModifiers = NSEvent.ModifierFlags()
            needsDisplay = true
            super.flagsChanged(with: theEvent)
            return
        }
        modifierEventHandler.handleModifiersEvent(with: theEvent.modifierFlags, timestamp: theEvent.timestamp)
        inputModifiers = theEvent.modifierFlags
        needsDisplay = true
        super.flagsChanged(with: theEvent)
    }

}

// MARK: - Text Attributes
private extension RecordView {
    func modifierTextAttributes(_ modifiers: NSEvent.ModifierFlags, checkModifier: NSEvent.ModifierFlags) -> [NSAttributedString.Key: Any] {
        let paragraphStyle = NSMutableParagraphStyle()
        paragraphStyle.alignment = .center
        paragraphStyle.lineBreakMode = .byTruncatingTail
        paragraphStyle.baseWritingDirection = .leftToRight
        let textColor: NSColor
        if !isEnabled {
            textColor = .disabledControlTextColor
        } else if modifiers.contains(checkModifier) {
            textColor = tintColor
        } else {
            textColor = .lightGray
        }
        return [.font: NSFont.systemFont(ofSize: floor(fontSize)),
                .foregroundColor: textColor,
                .paragraphStyle: paragraphStyle]
    }

    func keyCodeTextAttributes() -> [NSAttributedString.Key: Any] {
        let paragraphStyle = NSMutableParagraphStyle()
        paragraphStyle.lineBreakMode = .byTruncatingTail
        paragraphStyle.baseWritingDirection = .leftToRight
        let textColor: NSColor
        if !isEnabled {
            textColor = .disabledControlTextColor
        } else {
            textColor = tintColor
        }
        return [.font: NSFont.systemFont(ofSize: floor(fontSize)),
                .foregroundColor: textColor,
                .paragraphStyle: paragraphStyle]
    }
}

// MARK: - Recording
extension RecordView {
    @discardableResult
    public func beginRecording() -> Bool {
        guard let window = self.window else { return false }
        guard isEnabled else { return false }
        guard window.firstResponder != self || !isRecording else { return true }
        return window.makeFirstResponder(self)
    }

    @discardableResult
    public func endRecording() -> Bool {
        guard let window = self.window else { return true }
        guard window.firstResponder == self || isRecording else { return true }
        return window.makeFirstResponder(nil)
    }

    private func focusView() -> Bool {
        guard isEnabled else { return false }
        if let delegate = delegate, !delegate.recordViewShouldBeginRecording(self) {
            NSSound.beep()
            return false
        }
        isRecording = true
        needsDisplay = true
        updateTrackingAreas()
        return true
    }

    private func unfocusView() {
        inputModifiers = NSEvent.ModifierFlags()
        isRecording = false
        updateTrackingAreas()
        needsDisplay = true
        delegate?.recordViewDidEndRecording(self)
    }
}

// MARK: - Clear Keys
public extension RecordView {
    func clear() {
        keyCombo = nil
        inputModifiers = NSEvent.ModifierFlags()
        needsDisplay = true
        didChange?(nil)
        delegate?.recordView(self, didChangeKeyCombo: nil)
    }

    @objc func clearAndEndRecording() {
        clear()
        endRecording()
    }
}

// MARK: - Clear Button Mode
public extension RecordView {
    enum ClearButtonMode {
        case never
        case always
        case whenRecorded
    }
}

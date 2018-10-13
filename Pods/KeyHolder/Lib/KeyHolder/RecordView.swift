//
//  RecordView.swift
//
//  KeyHolder
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2016/06/17.
//
//  Copyright © 2016-2018 Clipy Project.
//

import Cocoa
import Carbon
import Magnet

public protocol RecordViewDelegate: class {
    func recordViewShouldBeginRecording(_ recordView: RecordView) -> Bool
    func recordView(_ recordView: RecordView, canRecordKeyCombo keyCombo: KeyCombo) -> Bool
    func recordViewDidClearShortcut(_ recordView: RecordView)
    func recordView(_ recordView: RecordView, didChangeKeyCombo keyCombo: KeyCombo)
    func recordViewDidEndRecording(_ recordView: RecordView)
}

@IBDesignable open class RecordView: NSView {

    // MARK: - Properties
    @IBInspectable open var backgroundColor: NSColor = .white {
        didSet { needsDisplay = true }
    }
    @IBInspectable open var tintColor: NSColor = .controlAccentPolyfill {
        didSet { needsDisplay = true }
    }
    @IBInspectable open var borderColor: NSColor = .white {
        didSet { layer?.borderColor = borderColor.cgColor }
    }
    @IBInspectable open var borderWidth: CGFloat = 0 {
        didSet { layer?.borderWidth = borderWidth }
    }
    @IBInspectable open var cornerRadius: CGFloat = 0 {
        didSet {
            layer?.cornerRadius = cornerRadius
            needsDisplay = true
            noteFocusRingMaskChanged()
        }
    }
    @IBInspectable open var showsClearButton: Bool = true {
        didSet { needsDisplay = true }
    }

    open weak var delegate: RecordViewDelegate?
    open var didChange: ((KeyCombo?) -> Void)?
    open var isRecording = false
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

    fileprivate let clearButton = NSButton()
    fileprivate let clearNormalImage = Util.bundleImage(name: "clear-off")
    fileprivate let clearAlternateImage = Util.bundleImage(name: "clear-on")
    fileprivate let validModifiers: [NSEvent.ModifierFlags] = [.shift, .control, .option, .command]
    fileprivate let validModifiersText: [NSString] = ["⇧", "⌃", "⌥", "⌘"]
    fileprivate var inputModifiers = NSEvent.ModifierFlags(rawValue: 0)
    fileprivate var doubleTapModifier = NSEvent.ModifierFlags(rawValue: 0)
    fileprivate var multiModifiers = false
    fileprivate var fontSize: CGFloat {
        return bounds.height / 1.7
    }
    fileprivate var clearSize: CGFloat {
        return fontSize / 1.3
    }
    fileprivate var marginY: CGFloat {
        return (bounds.height - fontSize) / 2.6
    }
    fileprivate var marginX: CGFloat {
        return marginY * 1.5
    }

    // MARK: - Override Properties
    open override var isOpaque: Bool {
        return false
    }
    open override var isFlipped: Bool {
        return true
    }
    open override var focusRingMaskBounds: NSRect {
        return (isEnabled && window?.firstResponder == self) ? bounds : NSRect.zero
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

    fileprivate func initView() {
        clearButton.bezelStyle = .shadowlessSquare
        clearButton.setButtonType(.momentaryChange)
        clearButton.isBordered = false
        clearButton.title = ""
        clearButton.target = self
        clearButton.action = #selector(RecordView.clearAndEndRecording)
        addSubview(clearButton)
    }

    // MARK: - Draw
    open override func drawFocusRingMask() {
        if isEnabled && window?.firstResponder == self {
            NSBezierPath(roundedRect: bounds, xRadius: cornerRadius, yRadius: cornerRadius).fill()
        }
    }

    override open func draw(_ dirtyRect: NSRect) {
        drawBackground(dirtyRect)
        drawModifiers(dirtyRect)
        drawKeyCode(dirtyRect)
        drawClearButton(dirtyRect)
    }

    fileprivate func drawBackground(_ dirtyRect: NSRect) {
        backgroundColor.setFill()
        NSBezierPath(roundedRect: bounds, xRadius: cornerRadius, yRadius: cornerRadius).fill()

        let rect = NSRect(x: borderWidth / 2, y: borderWidth / 2, width: bounds.width - borderWidth, height: bounds.height - borderWidth)
        let path = NSBezierPath(roundedRect: rect, xRadius: cornerRadius, yRadius: cornerRadius)
        path.lineWidth = borderWidth
        borderColor.set()
        path.stroke()
    }

    fileprivate func drawModifiers(_ dirtyRect: NSRect) {
        let fontSize = self.fontSize
        let modifiers: NSEvent.ModifierFlags
        if let keyCombo = self.keyCombo {
            modifiers = KeyTransformer.cocoaFlags(from: keyCombo.modifiers)
        } else {
            modifiers = inputModifiers
        }
        for (i, text) in validModifiersText.enumerated() {
            let rect = NSRect(x: marginX + (fontSize * CGFloat(i)), y: marginY, width: fontSize, height: bounds.height)
            text.draw(in: rect, withAttributes: modifierTextAttributes(modifiers, checkModifier: validModifiers[i]))
        }
    }

    fileprivate func drawKeyCode(_ dirtyRext: NSRect) {
        guard let keyCombo = self.keyCombo else { return }
        let fontSize = self.fontSize
        let minX = (fontSize * 4) + (marginX * 2)
        let width = bounds.width - minX - (marginX * 2) - clearSize
        if width <= 0 { return }
        let text = (keyCombo.doubledModifiers) ? "double tap" : keyCombo.characters
        text.draw(in: NSRect(x: minX, y: marginY, width: width, height: bounds.height), withAttributes: keyCodeTextAttributes())
    }

    fileprivate func drawClearButton(_ dirtyRext: NSRect) {
        let clearSize = self.clearSize
        clearNormalImage?.size = CGSize(width: clearSize, height: clearSize)
        clearAlternateImage?.size = CGSize(width: clearSize, height: clearSize)
        clearButton.image = clearNormalImage
        clearButton.alternateImage = clearAlternateImage
        let x = bounds.width - clearSize - marginX
        let y = (bounds.height - clearSize) / 2
        clearButton.frame = NSRect(x: x, y: y, width: clearSize, height: clearSize)
        clearButton.isHidden = !showsClearButton
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

    override open func resignFirstResponder() -> Bool {
        endRecording()
        return super.resignFirstResponder()
    }

    override open func acceptsFirstMouse(for theEvent: NSEvent?) -> Bool {
        return true
    }

    override open func mouseDown(with theEvent: NSEvent) {
        if !isEnabled {
            super.mouseDown(with: theEvent)
            return
        }

        let locationInView = convert(theEvent.locationInWindow, from: nil)
        if isMousePoint(locationInView, in: bounds) && !isRecording {
            _ = beginRecording()
        } else {
            super.mouseDown(with: theEvent)
        }
    }

    open override func cancelOperation(_ sender: Any?) {
        endRecording()
    }

    override open func keyDown(with theEvent: NSEvent) {
        if !performKeyEquivalent(with: theEvent) { super.keyDown(with: theEvent) }
    }

    override open func performKeyEquivalent(with theEvent: NSEvent) -> Bool {
        if !isEnabled { return false }
        if window?.firstResponder != self { return false }

        let keyCodeInt = Int(theEvent.keyCode)
        if isRecording && validateModifiers(inputModifiers) {
            let modifiers = KeyTransformer.carbonFlags(from: theEvent.modifierFlags)
            if let keyCombo = KeyCombo(keyCode: keyCodeInt, carbonModifiers: modifiers) {
                if delegate?.recordView(self, canRecordKeyCombo: keyCombo) ?? true {
                    self.keyCombo = keyCombo
                    didChange?(keyCombo)
                    delegate?.recordView(self, didChangeKeyCombo: keyCombo)
                    endRecording()
                    return true
                }
            }
            return false
        } else if isRecording && KeyTransformer.containsFunctionKey(keyCodeInt) {
            if let keyCombo = KeyCombo(keyCode: keyCodeInt, carbonModifiers: 0) {
                if delegate?.recordView(self, canRecordKeyCombo: keyCombo) ?? true {
                    self.keyCombo = keyCombo
                    didChange?(keyCombo)
                    delegate?.recordView(self, didChangeKeyCombo: keyCombo)
                    endRecording()
                    return true
                }
            }
            return false
        } else if Int(theEvent.keyCode) == kVK_Space {
            return beginRecording()
        }
        return false
    }

    override open func flagsChanged(with theEvent: NSEvent) {
        if isRecording {
            inputModifiers = theEvent.modifierFlags
            needsDisplay = true

            // For dobule tap
            let commandTapped = inputModifiers.contains(.command)
            let shiftTapped = inputModifiers.contains(.shift)
            let controlTapped = inputModifiers.contains(.control)
            let optionTapped = inputModifiers.contains(.option)
            let totalHash = commandTapped.intValue + optionTapped.intValue + shiftTapped.intValue + controlTapped.intValue
            if totalHash > 1 {
                multiModifiers = true
                return
            }
            if multiModifiers || totalHash == 0 {
                multiModifiers = false
                return
            }

            if (doubleTapModifier.contains(.command) && commandTapped) ||
                (doubleTapModifier.contains(.shift) && shiftTapped)    ||
                (doubleTapModifier.contains(.control) && controlTapped) ||
                (doubleTapModifier.contains(.option) && optionTapped) {

                if let keyCombo = KeyCombo(doubledCocoaModifiers: doubleTapModifier) {
                    if delegate?.recordView(self, canRecordKeyCombo: keyCombo) ?? true {
                        self.keyCombo = keyCombo
                        didChange?(keyCombo)
                        delegate?.recordView(self, didChangeKeyCombo: keyCombo)
                        endRecording()
                    }
                }
                doubleTapModifier = NSEvent.ModifierFlags(rawValue: 0)
            } else {
                if commandTapped {
                    doubleTapModifier = .command
                } else if shiftTapped {
                    doubleTapModifier = .shift
                } else if controlTapped {
                    doubleTapModifier = .control
                } else if optionTapped {
                    doubleTapModifier = .option
                } else {
                    doubleTapModifier = NSEvent.ModifierFlags(rawValue: 0)
                }
            }

            // Clean Flag
            let delay = 0.3 * Double(NSEC_PER_SEC)
            let time  = DispatchTime.now() + Double(Int64(delay)) / Double(NSEC_PER_SEC)
            DispatchQueue.main.asyncAfter(deadline: time, execute: { [weak self] in
                self?.doubleTapModifier = NSEvent.ModifierFlags(rawValue: 0)
            })
        } else {
            inputModifiers = NSEvent.ModifierFlags(rawValue: 0)
        }
        
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
        return [.font: NSFont.systemFont(ofSize: floor(fontSize)),
                .foregroundColor: tintColor,
                .paragraphStyle: paragraphStyle]
    }
}

// MARK: - Recording
public extension RecordView {
    public func beginRecording() -> Bool {
        if !isEnabled { return false }
        if isRecording { return true }

        needsDisplay = true

        if let delegate = delegate , !delegate.recordViewShouldBeginRecording(self) {
            NSSound.beep()
            return false
        }

        willChangeValue(forKey: "recording")
        isRecording = true
        didChangeValue(forKey: "recording")

        updateTrackingAreas()

        return true
    }

    public func endRecording() {
        if !isRecording { return }

        inputModifiers = NSEvent.ModifierFlags(rawValue: 0)
        doubleTapModifier = NSEvent.ModifierFlags(rawValue: 0)
        multiModifiers = false

        willChangeValue(forKey: "recording")
        isRecording = false
        didChangeValue(forKey: "recording")

        updateTrackingAreas()
        needsDisplay = true

        if window?.firstResponder == self && !canBecomeKeyView { window?.makeFirstResponder(nil) }
        delegate?.recordViewDidEndRecording(self)
    }
}

// MARK: - Clear Keys
public extension RecordView {
    public func clear() {
        keyCombo = nil
        inputModifiers = NSEvent.ModifierFlags(rawValue: 0)
        needsDisplay = true
        didChange?(nil)
        delegate?.recordViewDidClearShortcut(self)
    }

    @objc public func clearAndEndRecording() {
        clear()
        endRecording()
    }
}

// MARK: - Modifiers
private extension RecordView {
    func validateModifiers(_ modifiers: NSEvent.ModifierFlags?) -> Bool {
        guard let modifiers = modifiers else { return false }
        return KeyTransformer.carbonFlags(from: modifiers) != 0
    }
}

// MARK: - Bool Extension
private extension Bool {
    var intValue: Int {
        return NSNumber(value: self).intValue
    }
}

// MARK: - NSColor Extensio
// nmacOS 10.14 polyfill
private extension NSColor {
    static let controlAccentPolyfill: NSColor = {
        if #available(macOS 10.14, *) {
            return NSColor.controlAccentColor
        } else {
            return NSColor(red: 0.10, green: 0.47, blue: 0.98, alpha: 1)
        }
    }()
}

//
//  RecordView.swift
//  KeyHolder
//
//  Created by 古林　俊祐　 on 2016/06/17.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import Carbon
import Magnet

public protocol RecordViewDelegate: class {
    func recordViewShouldBeginRecording(recordView: RecordView) -> Bool
    func recordView(recordView: RecordView, canRecordKeyCombo keyCombo: KeyCombo) -> Bool
    func recordViewDidClearShortcut(recordView: RecordView)
    func recordView(recordView: RecordView, didChangeKeyCombo keyCombo: KeyCombo)
    func recordViewDidEndRecording(recordView: RecordView)
}

@IBDesignable public class RecordView: NSView {

    // MARK: - Properties
    @IBInspectable public var backgroundColor: NSColor = NSColor.whiteColor() {
        didSet { needsDisplay = true }
    }
    @IBInspectable public var tintColor: NSColor = NSColor.blueColor() {
        didSet { needsDisplay = true }
    }
    @IBInspectable public var borderColor: NSColor = NSColor.whiteColor() {
        didSet { layer?.borderColor = borderColor.CGColor }
    }
    @IBInspectable public var borderWidth: CGFloat = 0 {
        didSet { layer?.borderWidth = borderWidth }
    }
    @IBInspectable public var cornerRadius: CGFloat = 0 {
        didSet {
            layer?.cornerRadius = cornerRadius
            needsDisplay = true
            noteFocusRingMaskChanged()
        }
    }
    @IBInspectable public var showsClearButton: Bool = true {
        didSet { needsDisplay = true }
    }

    public weak var delegate: RecordViewDelegate?
    public var recording = false
    public var keyCombo: KeyCombo? {
        didSet { needsDisplay = true }
    }
    public var enabled = true {
        didSet {
            needsDisplay = true
            if !enabled { endRecording() }
            noteFocusRingMaskChanged()
        }
    }

    private let clearButton = NSButton()
    private let clearNormalImage = Util.bundleImage("clear-off")
    private let clearAlternateImage = Util.bundleImage("clear-on")
    private let validModifiers: [NSEventModifierFlags] = [.ShiftKeyMask, .ControlKeyMask, .AlternateKeyMask, .CommandKeyMask]
    private let validModifiersText: [NSString] = ["⇧", "⌃", "⌥", "⌘"]
    private var inputModifiers = NSEventModifierFlags(rawValue: 0)
    private var doubleTapModifier = NSEventModifierFlags(rawValue: 0)
    private var multiModifiers = false
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

    // MARK: - Override Properties
    public override var opaque: Bool {
        return false
    }
    public override var flipped: Bool {
        return true
    }
    public override var focusRingMaskBounds: NSRect {
        return (enabled && window?.firstResponder == self) ? bounds : NSRect.zero
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
        clearButton.bezelStyle = .ThickerSquareBezelStyle
        clearButton.setButtonType(.MomentaryChangeButton)
        clearButton.bordered = false
        clearButton.title = ""
        clearButton.target = self
        clearButton.action = #selector(RecordView.clearAndEndRecording)
        addSubview(clearButton)
    }

    // MARK: - Draw
    public override func drawFocusRingMask() {
        if enabled && window?.firstResponder == self {
            NSBezierPath(roundedRect: bounds, xRadius: cornerRadius, yRadius: cornerRadius).fill()
        }
    }

    override public func drawRect(dirtyRect: NSRect) {
        drawBackground(dirtyRect)
        drawModifiers(dirtyRect)
        drawKeyCode(dirtyRect)
        drawClearButton(dirtyRect)
    }

    private func drawBackground(dirtyRect: NSRect) {
        backgroundColor.setFill()
        NSBezierPath(roundedRect: bounds, xRadius: cornerRadius, yRadius: cornerRadius).fill()

        let rect = NSRect(x: borderWidth / 2, y: borderWidth / 2, width: bounds.width - borderWidth, height: bounds.height - borderWidth)
        let path = NSBezierPath(roundedRect: rect, xRadius: cornerRadius, yRadius: cornerRadius)
        path.lineWidth = borderWidth
        borderColor.set()
        path.stroke()
    }

    private func drawModifiers(dirtyRect: NSRect) {
        let fontSize = self.fontSize
        let modifiers: NSEventModifierFlags
        if let keyCombo = self.keyCombo {
            modifiers = KeyTransformer.carbonToCocoaFlags(keyCombo.modifiers)
        } else {
            modifiers = inputModifiers ?? NSEventModifierFlags(rawValue: 0)
        }
        for (i, text) in validModifiersText.enumerate() {
            let rect = NSRect(x: marginX + (fontSize * CGFloat(i)), y: marginY, width: fontSize, height: bounds.height)
            text.drawInRect(rect, withAttributes: modifierTextAttributes(modifiers, checkModifier: validModifiers[i]))
        }
    }

    private func drawKeyCode(dirtyRext: NSRect) {
        guard let keyCombo = self.keyCombo else { return }
        let fontSize = self.fontSize
        let minX = (fontSize * 4) + (marginX * 2)
        let width = bounds.width - minX - (marginX * 2) - clearSize
        if width <= 0 { return }
        let text = (keyCombo.doubledModifiers) ? "double tap" : keyCombo.characters
        text.drawInRect(NSRect(x: minX, y: marginY, width: width, height: bounds.height), withAttributes: keyCodeTextAttributes())
    }

    private func drawClearButton(dirtyRext: NSRect) {
        let clearSize = self.clearSize
        clearNormalImage?.size = CGSize(width: clearSize, height: clearSize)
        clearAlternateImage?.size = CGSize(width: clearSize, height: clearSize)
        clearButton.image = clearNormalImage
        clearButton.alternateImage = clearAlternateImage
        let x = bounds.width - clearSize - marginX
        let y = (bounds.height - clearSize) / 2
        clearButton.frame = NSRect(x: x, y: y, width: clearSize, height: clearSize)
        clearButton.hidden = !showsClearButton
    }
}

// MARK: - Text Attributes
private extension RecordView {
    private func modifierTextAttributes(modifiers: NSEventModifierFlags, checkModifier: NSEventModifierFlags) -> [String: AnyObject] {
        let paragraphStyle = NSMutableParagraphStyle()
        paragraphStyle.alignment = NSCenterTextAlignment
        paragraphStyle.lineBreakMode = NSLineBreakMode.ByTruncatingTail
        paragraphStyle.baseWritingDirection = NSWritingDirection.LeftToRight
        let textColor: NSColor
        if !enabled {
            textColor = NSColor.disabledControlTextColor()
        } else if modifiers.contains(checkModifier) {
            textColor = tintColor
        } else {
            textColor = NSColor.lightGrayColor()
        }
        return [NSFontAttributeName: NSFont.systemFontOfSize(floor(fontSize)),
                NSForegroundColorAttributeName: textColor,
                NSParagraphStyleAttributeName: paragraphStyle]
    }

    private func keyCodeTextAttributes() -> [String: AnyObject] {
        let paragraphStyle = NSMutableParagraphStyle()
        paragraphStyle.lineBreakMode = NSLineBreakMode.ByTruncatingTail
        paragraphStyle.baseWritingDirection = NSWritingDirection.LeftToRight
        return [NSFontAttributeName: NSFont.systemFontOfSize(floor(fontSize)),
                NSForegroundColorAttributeName: tintColor,
                NSParagraphStyleAttributeName: paragraphStyle]
    }
}

// MARK: - Recording
public extension RecordView {
    public func beginRecording() -> Bool {
        if !enabled { return false }
        if recording { return true }

        needsDisplay = true

        if let delegate = delegate where !delegate.recordViewShouldBeginRecording(self) {
            NSBeep()
            return false
        }

        willChangeValueForKey("recording")
        recording = true
        didChangeValueForKey("recording")

        updateTrackingAreas()

        return true
    }

    public func endRecording() {
        if !recording { return }

        inputModifiers = NSEventModifierFlags(rawValue: 0)
        doubleTapModifier = NSEventModifierFlags(rawValue: 0)
        multiModifiers = false

        willChangeValueForKey("recording")
        recording = false
        didChangeValueForKey("recording")

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
        inputModifiers = NSEventModifierFlags(rawValue: 0)
        needsDisplay = true
        delegate?.recordViewDidClearShortcut(self)
    }

    public func clearAndEndRecording() {
        clear()
        endRecording()
    }
}

// MARK: - NSReponder
public extension RecordView {
    public override var acceptsFirstResponder: Bool {
        return enabled
    }

    public override var canBecomeKeyView: Bool {
        return super.canBecomeKeyView && NSApp.fullKeyboardAccessEnabled
    }

    public override var needsPanelToBecomeKey: Bool {
        return true
    }

    public override func resignFirstResponder() -> Bool {
        endRecording()
        return super.resignFirstResponder()
    }

    public override func acceptsFirstMouse(theEvent: NSEvent?) -> Bool {
        return true
    }

    public override func mouseDown(theEvent: NSEvent) {
        if !enabled {
            super.mouseDown(theEvent)
            return
        }

        let locationInView = convertPoint(theEvent.locationInWindow, fromView: nil)
        if mouse(locationInView, inRect: bounds) && !recording {
            beginRecording()
        } else {
            super.mouseDown(theEvent)
        }
    }

    public override func keyDown(theEvent: NSEvent) {
        if !performKeyEquivalent(theEvent) { super.keyDown(theEvent) }
    }

    public override func performKeyEquivalent(theEvent: NSEvent) -> Bool {
        if !enabled { return false }
        if window?.firstResponder != self { return false }

        let keyCodeInt = Int(theEvent.keyCode)
        if recording && validateModifiers(inputModifiers) {
            let modifiers = KeyTransformer.cocoaToCarbonFlags(theEvent.modifierFlags)
            if let keyCombo = KeyCombo(keyCode: keyCodeInt, carbonModifiers: modifiers) {
                if delegate?.recordView(self, canRecordKeyCombo: keyCombo) ?? true {
                    self.keyCombo = keyCombo
                    delegate?.recordView(self, didChangeKeyCombo: keyCombo)
                    endRecording()
                    return true
                }
            }
            return false
        } else if recording && KeyTransformer.containsFunctionKey(keyCodeInt) {
            if let keyCombo = KeyCombo(keyCode: keyCodeInt, carbonModifiers: 0) {
                if delegate?.recordView(self, canRecordKeyCombo: keyCombo) ?? true {
                    self.keyCombo = keyCombo
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

    public override func flagsChanged(theEvent: NSEvent) {
        if recording {
            inputModifiers = theEvent.modifierFlags
            needsDisplay = true

            // For dobule tap
            let commandTapped = inputModifiers.contains(.CommandKeyMask)
            let shiftTapped = inputModifiers.contains(.ShiftKeyMask)
            let controlTapped = inputModifiers.contains(.ControlKeyMask)
            let altTapped = inputModifiers.contains(.AlternateKeyMask)
            let totalHash = commandTapped.hashValue + altTapped.hashValue + shiftTapped.hashValue + controlTapped.hashValue
            if totalHash > 1 {
                multiModifiers = true
                return
            }
            if multiModifiers || totalHash == 0 {
                multiModifiers = false
                return
            }

            if (doubleTapModifier.contains(.CommandKeyMask) && commandTapped) ||
                (doubleTapModifier.contains(.ShiftKeyMask) && shiftTapped)    ||
                (doubleTapModifier.contains(.ControlKeyMask) && controlTapped) ||
                (doubleTapModifier.contains(.AlternateKeyMask) && altTapped) {

                if let keyCombo = KeyCombo(doubledCocoaModifiers: doubleTapModifier) {
                    if delegate?.recordView(self, canRecordKeyCombo: keyCombo) ?? true {
                        self.keyCombo = keyCombo
                        delegate?.recordView(self, didChangeKeyCombo: keyCombo)
                        endRecording()
                    }
                }
                doubleTapModifier = NSEventModifierFlags(rawValue: 0)
            } else {
                if commandTapped {
                    doubleTapModifier = .CommandKeyMask
                } else if shiftTapped {
                    doubleTapModifier = .ShiftKeyMask
                } else if controlTapped {
                    doubleTapModifier = .ControlKeyMask
                } else if altTapped {
                    doubleTapModifier = .AlternateKeyMask
                } else {
                    doubleTapModifier = NSEventModifierFlags(rawValue: 0)
                }
            }

            // Clean Flag
            let delay = 0.3 * Double(NSEC_PER_SEC)
            let time  = dispatch_time(DISPATCH_TIME_NOW, Int64(delay))
            dispatch_after(time, dispatch_get_main_queue(), { [weak self] in
                self?.doubleTapModifier = NSEventModifierFlags(rawValue: 0)
            })
        } else {
            inputModifiers = NSEventModifierFlags(rawValue: 0)
        }

        super.flagsChanged(theEvent)
    }
}

// MARK: - Modifiers
private extension RecordView {
    private func validateModifiers(modifiers: NSEventModifierFlags?) -> Bool {
        guard let modifiers = modifiers else { return false }
        return KeyTransformer.cocoaToCarbonFlags(modifiers) != 0
    }
}

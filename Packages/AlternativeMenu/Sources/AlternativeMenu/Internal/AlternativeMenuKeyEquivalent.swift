//
//  AlternativeMenuKeyEquivalent.swift
//  AlternativeMenu
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import CoreText

enum AlternativeMenuKeyEquivalent {
    struct Display: Equatable {
        @MainActor
        static let empty = Self(modifierSymbols: "", keyName: "", keyGlyphs: "", keyFont: AlternativeMenuStyle.keyEquivalentFont)

        let accessibilityText: String
        let attributedText: NSAttributedString
        let keyWidth: CGFloat
        let usesSharedKeyColumn: Bool
        private let modifierWidth: CGFloat

        @MainActor
        init(modifierSymbols: String, keyName: String, keyGlyphs: String, keyFont: NSFont) {
            self.accessibilityText = modifierSymbols + keyName
            // Long localized key names keep their own width instead of shifting the other keys.
            self.usesSharedKeyColumn =
                keyName.count == 1 || keyGlyphs.unicodeScalars.contains { $0.properties.generalCategory == .privateUse }

            let modifierFont = AlternativeMenuStyle.keyEquivalentFont
            let text = NSMutableAttributedString(string: modifierSymbols, attributes: [.font: modifierFont, .kern: 1])
            if !modifierSymbols.isEmpty {
                let gap: CGFloat = 2
                let space = NSAttributedString(string: " ", attributes: [.font: modifierFont])
                let spaceWidth = CTLineGetTypographicBounds(CTLineCreateWithAttributedString(space), nil, nil, nil)
                text.append(NSAttributedString(string: " ", attributes: [.font: modifierFont, .kern: gap - spaceWidth]))
            }
            self.modifierWidth = CTLineGetTypographicBounds(CTLineCreateWithAttributedString(text), nil, nil, nil)

            let paragraph = NSMutableParagraphStyle()
            paragraph.alignment = .left
            paragraph.baseWritingDirection = .leftToRight
            paragraph.lineBreakMode = .byClipping
            // AppKit already mirrors the key and supplies function-key ligatures.
            let keyText = keyGlyphs.isEmpty ? "" : "\u{2066}\(keyGlyphs)\u{2069}"
            let key = NSAttributedString(string: keyText, attributes: [.font: keyFont, .paragraphStyle: paragraph])
            text.append(key)
            text.addAttribute(.paragraphStyle, value: paragraph, range: NSRange(location: 0, length: text.length))
            self.attributedText = NSAttributedString(attributedString: text)

            let keyLine = CTLineCreateWithAttributedString(key)
            let advance = CTLineGetTypographicBounds(keyLine, nil, nil, nil)
            // Function-key ligatures have zero advance but still occupy visible space.
            let bounds = CTLineGetBoundsWithOptions(keyLine, .useGlyphPathBounds)
            self.keyWidth = ceil(max(advance, bounds.isNull ? 0 : bounds.maxX))
        }

        func width(sharedKeyColumnWidth: CGFloat) -> CGFloat {
            modifierWidth + (usesSharedKeyColumn ? max(keyWidth, sharedKeyColumnWidth) : keyWidth)
        }
    }

    @MainActor
    static func display(for item: NSMenuItem) -> Display {
        let key = item.keyEquivalent
        guard !key.isEmpty,
            let shortcut = shortcut(for: item),
            let modifierSymbols = string(shortcut, getter: "localizedModifierMaskDisplayName"),
            let localizedKeyName = string(shortcut, getter: "localizedKeyEquivalentDisplayName"),
            let drawing = drawing(for: item)
        else {
            return .empty
        }
        // Keep readable names for accessibility; drawing uses AppKit's unmodified glyphs.
        let keyName =
            drawing.text.unicodeScalars.contains { $0.properties.generalCategory == .privateUse } ? localizedKeyName : drawing.text
        let keyGlyphs =
            switch key {
            case "\u{f700}": "▲"
            case "\u{f701}": "▼"
            case "\u{f702}": "◀"
            case "\u{f703}": "▶"
            default: drawing.text
            }
        return Display(modifierSymbols: modifierSymbols, keyName: keyName, keyGlyphs: keyGlyphs, keyFont: drawing.font)
    }

    @MainActor
    private static func shortcut(for item: NSMenuItem) -> NSObject? {
        let selector = NSSelectorFromString("shortcutWithKeyEquivalent:modifierMask:")
        guard let type = NSClassFromString("NSKeyboardShortcut"),
            let method = class_getClassMethod(type, selector)
        else {
            return nil
        }
        typealias Create = @convention(c) (AnyClass, Selector, NSString, UInt) -> Unmanaged<NSObject>?
        let create = unsafeBitCast(method_getImplementation(method), to: Create.self)
        // Derive display names from the same prepared values used for native key matching.
        return create(type, selector, item.keyEquivalent as NSString, item.keyEquivalentModifierMask.rawValue)?.takeUnretainedValue()
    }

    @MainActor
    private static func drawing(for item: NSMenuItem) -> (text: String, font: NSFont)? {
        let selector = NSSelectorFromString("keyEquivalentStringForItem:withFontPtr:")
        let fontSelector = NSSelectorFromString("menuItemCommandKeyFont")
        guard let type = NSClassFromString("NSContextMenuItemView") as? NSObject.Type,
            type.responds(to: fontSelector),
            let defaultFont = type.perform(fontSelector)?.takeUnretainedValue() as? NSFont,
            let method = class_getClassMethod(type, selector)
        else {
            return nil
        }
        typealias Read =
            @convention(c) (
                AnyClass, Selector, NSMenuItem, UnsafeMutablePointer<Unmanaged<NSFont>?>
            ) -> Unmanaged<NSString>?
        let invoke = unsafeBitCast(method_getImplementation(method), to: Read.self)
        // This is an in/out argument: AppKit keeps the default or replaces it for the key.
        var font: Unmanaged<NSFont>? = .passUnretained(defaultFont)
        guard var name = invoke(type, selector, item, &font)?.takeUnretainedValue() as String?,
            let font = font?.takeUnretainedValue()
        else {
            return nil
        }
        // The view already isolates shortcut direction; omit AppKit's leading left-to-right mark.
        if name.unicodeScalars.first?.value == 0x200E {
            name.unicodeScalars.removeFirst()
        }
        return (name, font)
    }

    @MainActor
    private static func string(_ receiver: NSObject, getter: String) -> String? {
        let selector = NSSelectorFromString(getter)
        guard receiver.responds(to: selector) else {
            return nil
        }
        return receiver.perform(selector)?.takeUnretainedValue() as? String
    }
}

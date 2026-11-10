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
        static let empty = Self(
            modifierSymbols: "",
            keyName: "",
            keyGlyphs: "",
            keyFont: AlternativeMenuStyle.keyEquivalentFont,
            nativeKeyWidth: nil,
            nativeModifierWidth: nil,
            isSymbolKey: false
        )

        let accessibilityText: String
        let attributedText: NSAttributedString
        let keyWidth: CGFloat
        let modifierColumnWidth: CGFloat
        let minimumWidth: CGFloat
        let usesSharedKeyColumn: Bool
        private let modifierWidth: CGFloat
        private let keyDrawingWidth: CGFloat
        private let isSymbolKey: Bool

        @MainActor
        init(
            modifierSymbols: String,
            keyName: String,
            keyGlyphs: String,
            keyFont: NSFont,
            nativeKeyWidth: CGFloat?,
            nativeModifierWidth: CGFloat?,
            isSymbolKey: Bool
        ) {
            self.accessibilityText = modifierSymbols + keyName
            self.isSymbolKey = isSymbolKey
            // Long localized key names keep their own width instead of shifting the other keys.
            self.usesSharedKeyColumn =
                keyName.count == 1 || keyGlyphs.unicodeScalars.contains { $0.properties.generalCategory == .privateUse }

            let modifierFont = AlternativeMenuStyle.keyEquivalentFont
            let text = NSMutableAttributedString()
            for symbol in modifierSymbols {
                let string = String(symbol)
                let width = NSAttributedString(string: string, attributes: [.font: modifierFont]).size().width
                // Reserve at least one em while keeping wider modifier glyphs intact.
                text.append(NSAttributedString(string: string, attributes: [.font: modifierFont, .kern: ceil(max(modifierFont.pointSize, width)) - width]))
            }
            if !modifierSymbols.isEmpty {
                let gap = AlternativeMenuStyle.current.modifierToKeySpacing
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
            let keyLine = CTLineCreateWithAttributedString(key)
            let advance = CTLineGetTypographicBounds(keyLine, nil, nil, nil)
            // Function-key ligatures have zero advance but still occupy visible space.
            let bounds = CTLineGetBoundsWithOptions(keyLine, .useGlyphPathBounds)
            let isFunctionLigature = advance == 0 && !bounds.isNull
                && keyGlyphs.unicodeScalars.contains { $0.properties.generalCategory == .privateUse }
            self.keyDrawingWidth = advance > 0 ? advance : ceil(bounds.isNull ? 0 : bounds.width)
            let fallbackWidth = isFunctionLigature && nativeKeyWidth == 0
                ? AlternativeMenuStyle.current.legacyFunctionKeyColumnWidth : keyDrawingWidth
            let measuredWidth = nativeKeyWidth.flatMap { $0.isFinite && $0 > 0 ? $0 : nil } ?? fallbackWidth
            self.keyWidth = usesSharedKeyColumn ? ceil(measuredWidth) : ceil(measuredWidth * 2) / 2
            let modifierAdvance = modifierSymbols.isEmpty ? 0 : modifierWidth - AlternativeMenuStyle.current.modifierToKeySpacing
            let measuredModifierWidth = nativeModifierWidth.flatMap { $0.isFinite && $0 >= 0 ? $0 : nil } ?? modifierAdvance
            self.modifierColumnWidth = ceil(measuredModifierWidth)
            // Native sizing uses each item's own key, without the shared column or drawing offsets.
            self.minimumWidth = text.length == 0 ? 0 : modifierColumnWidth + ceil(keyWidth)
                + AlternativeMenuStyle.current.keyEquivalentSizingPadding
            if AlternativeMenuStyle.current.centersKeyEquivalents && isFunctionLigature {
                text.addAttribute(
                    .baselineOffset, value: AlternativeMenuStyle.current.functionKeyBaselineOffset,
                    range: NSRange(location: text.length - key.length, length: key.length)
                )
            }
            self.attributedText = NSAttributedString(attributedString: text)
        }

        func width(sharedKeyColumnWidth: CGFloat) -> CGFloat {
            modifierWidth + (usesSharedKeyColumn
                ? max(keyWidth, sharedKeyColumnWidth)
                : keyWidth + (keyWidth > 0 ? AlternativeMenuStyle.current.longKeyEquivalentPadding : 0))
        }

        func text(sharedKeyColumnWidth: CGFloat, isRightToLeft: Bool) -> NSMutableAttributedString {
            let text = NSMutableAttributedString(attributedString: attributedText)
            guard text.length > 0 else {
                return text
            }
            var keyDrawingWidth = keyDrawingWidth
            let returnRange = (text.string as NSString).range(of: "⏎")
            if isSymbolKey && isRightToLeft && returnRange.location != NSNotFound {
                // Native Return images mirror in RTL; the two text glyphs have different advances.
                text.replaceCharacters(in: returnRange, with: "↪")
                let key = text.attributedSubstring(from: returnRange)
                keyDrawingWidth = CTLineGetTypographicBounds(CTLineCreateWithAttributedString(key), nil, nil, nil)
            }
            let paragraph = NSMutableParagraphStyle()
            paragraph.alignment = .left
            paragraph.baseWritingDirection = .leftToRight
            paragraph.lineBreakMode = .byClipping
            let modifierInset = usesSharedKeyColumn && modifierWidth > 0
                ? AlternativeMenuStyle.current.centeredModifierLeadingInset : 0
            paragraph.firstLineHeadIndent = AlternativeMenuStyle.current.keyEquivalentDrawingInset + modifierInset
            let columnWidth = max(keyWidth, sharedKeyColumnWidth)
            let keyOffset: CGFloat
            if isSymbolKey && usesSharedKeyColumn {
                // Native image keys follow the menu's natural paragraph direction inside the shared column.
                keyOffset = (keyWidth - keyDrawingWidth) / 2
                    + (isRightToLeft ? columnWidth - keyWidth : 0)
                    - (AlternativeMenuStyle.current.centersKeyEquivalents ? 0 : AlternativeMenuStyle.current.legacySymbolKeyLeadingInset)
            } else if AlternativeMenuStyle.current.centersKeyEquivalents && usesSharedKeyColumn {
                keyOffset = (columnWidth - keyDrawingWidth) / 2
            } else {
                keyOffset = 0
            }
            let start = (text.string as NSString).range(of: "\u{2066}").location
            if start != NSNotFound && start > 1 {
                // Apply negative spacing to a modifier, because a space cannot have negative advance.
                let index = start - 2
                let kern = text.attribute(.kern, at: index, effectiveRange: nil) as? CGFloat ?? 0
                text.addAttribute(.kern, value: kern + keyOffset - modifierInset, range: NSRange(location: index, length: 1))
            } else {
                paragraph.firstLineHeadIndent += keyOffset
            }
            text.addAttribute(.paragraphStyle, value: paragraph, range: NSRange(location: 0, length: text.length))
            return text
        }
    }

    private struct Drawing {
        let text: String
        let font: NSFont
        let keyWidth: CGFloat?
        let modifierWidth: CGFloat?
        let isSymbol: Bool
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
        // Keep AppKit's readable key names for accessibility.
        let keyName =
            drawing.text.unicodeScalars.contains { $0.properties.generalCategory == .privateUse } ? localizedKeyName : drawing.text
        let keyGlyphs =
            switch key {
            case "\u{f700}": "▲"
            case "\u{f701}": "▼"
            case "\u{f702}": "◀"
            case "\u{f703}": "▶"
            case "\r", "\n": "⏎"
            default: drawing.text
            }
        return Display(
            modifierSymbols: modifierSymbols,
            keyName: keyName,
            keyGlyphs: keyGlyphs,
            keyFont: drawing.font,
            nativeKeyWidth: drawing.keyWidth,
            nativeModifierWidth: drawing.modifierWidth,
            isSymbolKey: drawing.isSymbol
        )
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
    private static func drawing(for item: NSMenuItem) -> Drawing? {
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
        let hasDirectionMark = name.unicodeScalars.first?.value == 0x200E
        // Native image keys omit the direction mark; function-key ligatures also omit it.
        let isSymbol = !hasDirectionMark && name.count == 1
            && !name.unicodeScalars.contains { $0.properties.generalCategory == .privateUse }
        // The view already isolates shortcut direction; omit AppKit's leading left-to-right mark.
        if hasDirectionMark {
            name.unicodeScalars.removeFirst()
        }
        // The native field uses the command font for Space despite the getter replacing it.
        let drawingFont = item.keyEquivalent == " " ? defaultFont : font
        let keyWidth = measuredKeyWidth(for: item, font: drawingFont)
        let modifierWidth = measuredModifierWidth(for: item, font: defaultFont)
        return Drawing(text: name, font: drawingFont, keyWidth: keyWidth, modifierWidth: modifierWidth, isSymbol: isSymbol)
    }

    @MainActor
    private static func measuredKeyWidth(for item: NSMenuItem, font: NSFont) -> CGFloat? {
        let selector = NSSelectorFromString("measureKeyEquivalentForItem:withFont:keStringPointer:")
        guard let type = NSClassFromString("NSContextMenuItemView"),
            let method = class_getClassMethod(type, selector)
        else {
            return nil
        }
        typealias Measure =
            @convention(c) (
                AnyClass, Selector, NSMenuItem, NSFont, UnsafeMutablePointer<Unmanaged<NSString>?>
            ) -> CGSize
        let measure = unsafeBitCast(method_getImplementation(method), to: Measure.self)
        var text: Unmanaged<NSString>?
        return measure(type, selector, item, font, &text).width
    }

    @MainActor
    private static func measuredModifierWidth(for item: NSMenuItem, font: NSFont) -> CGFloat? {
        let selector = NSSelectorFromString("measureCommandKeyModifiersForItem:withFont:")
        guard let type = NSClassFromString("NSContextMenuItemView"),
            let method = class_getClassMethod(type, selector)
        else {
            return nil
        }
        typealias Measure = @convention(c) (AnyClass, Selector, NSMenuItem, NSFont) -> CGSize
        let measure = unsafeBitCast(method_getImplementation(method), to: Measure.self)
        return measure(type, selector, item, font).width
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

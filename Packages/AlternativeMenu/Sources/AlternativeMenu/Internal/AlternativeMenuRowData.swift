//
//  AlternativeMenuRowData.swift
//  AlternativeMenu
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit

@MainActor
struct AlternativeMenuRowData {
    enum Kind {
        case item
        case disabled
        case header
        case separator
    }

    let kind: Kind
    let title: AlternativeMenuItem.Title
    let image: NSImage?
    let toolTip: String?
    let font: NSFont
    let verticalRange: Range<CGFloat>
    let titleWidthWithInsets: CGFloat
    let isSymbolImage: Bool
    let hasSubmenu: Bool
    let keyEquivalent: AlternativeMenuKeyEquivalent.Display

    var rowHeight: CGFloat {
        verticalRange.upperBound - verticalRange.lowerBound
    }

    init(item: AlternativeMenuItem, verticalOffset: CGFloat, keyEquivalent: AlternativeMenuKeyEquivalent.Display? = nil) {
        switch item.kind {
        case .sectionHeader:
            self.kind = .header
        case .separator:
            self.kind = .separator
        default:
            self.kind = item.isEnabled ? .item : .disabled
        }
        self.hasSubmenu = item.submenu != nil
        self.font = kind == .header ? AlternativeMenuStyle.headerFont : AlternativeMenuStyle.menuFont
        let configuration =
            if kind == .header {
                NSImage.SymbolConfiguration(pointSize: font.pointSize, weight: .semibold, scale: .small)
            } else {
                NSImage.SymbolConfiguration(scale: .small)
            }
        self.image = item.image.flatMap { $0.withSymbolConfiguration(configuration) ?? $0 }
        self.isSymbolImage = image?.isSymbolImage ?? false
        self.toolTip = item.toolTip

        self.keyEquivalent = keyEquivalent ?? .empty
        let titleSize: NSSize
        var maximumFontLineHeight = Self.lineHeight(for: font)
        switch item.titleContent {
        case .plain(let value):
            self.title = .plain(value)
            titleSize = (value as NSString).boundingRect(with: .zero, options: [], attributes: [.font: font]).size
        case .attributed(let source):
            let measured = NSMutableAttributedString(string: source.string, attributes: [.font: font])
            let isDisabled = kind == .disabled
            let lineEnd = (source.string as NSString).rangeOfCharacter(from: .newlines).location
            source.enumerateAttributes(in: NSRange(location: 0, length: source.length)) { attributes, range, _ in
                var attributes = attributes
                // Menu titles always use leading alignment and tail truncation.
                attributes[.paragraphStyle] = nil
                if isDisabled, let color = attributes[.foregroundColor] as? NSColor {
                    // Native menus lighten explicit foreground colors, retaining decorations.
                    attributes[.foregroundColor] = color.blended(withFraction: 0.5, of: .white) ?? color
                }
                measured.addAttributes(attributes, range: range)
                if range.location < lineEnd, let runFont = attributes[.font] as? NSFont {
                    maximumFontLineHeight = max(maximumFontLineHeight, Self.lineHeight(for: runFont))
                }
            }
            self.title = .attributed(measured)
            // Measure as a single line to match Text.lineLimit(1).
            titleSize = measured.boundingRect(with: .zero, options: []).size
        }

        let size = image?.size ?? .zero
        // Include baseline offsets in addition to the font and image bounds.
        let rowHeight: CGFloat =
            if kind == .separator {
                AlternativeMenuStyle.current.separatorRowHeight
            } else {
                ceil(
                    max(
                        AlternativeMenuStyle.current.minimumRowHeight,
                        max(size.height, titleSize.height, maximumFontLineHeight) + AlternativeMenuStyle.current.rowContentVerticalPadding
                    )
                )
            }
        self.verticalRange = verticalOffset..<(verticalOffset + rowHeight)
        self.titleWidthWithInsets = ceil(titleSize.width) + AlternativeMenuStyle.current.rowTextInset * 2
    }

    func iconWidth(symbolColumnWidth: CGFloat) -> CGFloat {
        if let image {
            return isSymbolImage ? symbolColumnWidth : image.size.width
        }
        return kind == .header || kind == .separator ? 0 : symbolColumnWidth
    }

    func trailingWidth(keyColumnWidth: CGFloat) -> CGFloat {
        if hasSubmenu {
            return AlternativeMenuStyle.current.submenuColumnWidth
        }
        guard keyEquivalent.attributedText.length > 0 else {
            return 0
        }
        return max(
            AlternativeMenuStyle.current.minimumKeyEquivalentColumnWidth,
            ceil(keyEquivalent.width(sharedKeyColumnWidth: keyColumnWidth)) + AlternativeMenuStyle.current.keyEquivalentGap
        )
    }

    private static func lineHeight(for font: NSFont) -> CGFloat {
        // Round ascent and descent separately to match measured native single-font row heights.
        ceil(font.ascender) + ceil(-font.descender) + font.leading
    }
}

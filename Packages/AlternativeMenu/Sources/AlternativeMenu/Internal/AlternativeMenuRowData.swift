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
        var lineHeight = font.ascender - font.descender + font.leading
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
                    lineHeight = max(lineHeight, runFont.ascender - runFont.descender + runFont.leading)
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
                AlternativeMenuStyle.separatorRowHeight
            } else {
                ceil(
                    max(
                        AlternativeMenuStyle.minimumRowHeight,
                        size.height + AlternativeMenuStyle.rowContentVerticalPadding,
                        lineHeight.rounded() + AlternativeMenuStyle.rowFontHeightAdjustment,
                        titleSize.height + AlternativeMenuStyle.rowContentVerticalPadding
                    )
                )
            }
        self.verticalRange = verticalOffset..<(verticalOffset + rowHeight)
        self.titleWidthWithInsets = ceil(titleSize.width) + AlternativeMenuStyle.rowTextInset * 2
    }

    func iconWidth(symbolColumnWidth: CGFloat) -> CGFloat {
        if let image {
            return isSymbolImage ? symbolColumnWidth : image.size.width
        }
        return kind == .header || kind == .separator ? 0 : symbolColumnWidth
    }

    func trailingWidth(keyColumnWidth: CGFloat) -> CGFloat {
        if hasSubmenu {
            return AlternativeMenuStyle.submenuColumnWidth
        }
        guard keyEquivalent.attributedText.length > 0 else {
            return 0
        }
        return max(
            AlternativeMenuStyle.minimumKeyEquivalentColumnWidth,
            ceil(keyEquivalent.width(sharedKeyColumnWidth: keyColumnWidth)) + AlternativeMenuStyle.keyEquivalentGap
        )
    }
}

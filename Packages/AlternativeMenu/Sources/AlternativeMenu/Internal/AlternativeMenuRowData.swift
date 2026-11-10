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
    let imageLayoutSize: NSSize
    let toolTip: String?
    let font: NSFont
    let minimumTitleHeight: CGFloat
    let titleBaselineOffset: CGFloat
    let verticalRange: Range<CGFloat>
    let leadingInset: CGFloat
    let titleWidthWithInsets: CGFloat
    let isSymbolImage: Bool
    let hasSubmenu: Bool
    let keyEquivalent: AlternativeMenuKeyEquivalent.Display

    var rowHeight: CGFloat {
        verticalRange.upperBound - verticalRange.lowerBound
    }

    init(
        item: AlternativeMenuItem,
        verticalOffset: CGFloat,
        keyEquivalent: AlternativeMenuKeyEquivalent.Display? = nil,
        hidesSeparator: Bool = false
    ) {
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
        var minimumTitleHeight = Self.lineHeight(for: font)
        let configuredImage = Self.configuredImage(item.image, font: font, isHeader: kind == .header)
        self.image = configuredImage.image
        self.imageLayoutSize = configuredImage.layoutSize
        self.leadingInset = image == nil && kind != .separator ? AlternativeMenuStyle.current.imagelessRowTextInset : AlternativeMenuStyle.current.rowTextInset
        self.isSymbolImage = image?.isSymbolImage ?? false
        self.toolTip = item.toolTip

        self.keyEquivalent = keyEquivalent ?? .empty
        let titleSize: NSSize
        let titleWidthPadding: CGFloat
        var maximumFontLineHeight = minimumTitleHeight
        var hasUniformFont = true
        switch item.titleContent {
        case .plain(let value):
            self.title = .plain(value)
            titleWidthPadding = 0
            titleSize = (value as NSString).boundingRect(with: .zero, options: [], attributes: [.font: font]).size
        case .attributed(let source):
            titleWidthPadding = source.length > 0 ? AlternativeMenuStyle.current.attributedTitleWidthPadding : 0
            let measured = NSMutableAttributedString(string: source.string, attributes: [.font: AlternativeMenuStyle.menuFont])
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
                if range.location < lineEnd {
                    let runFont = attributes[.font] as? NSFont ?? AlternativeMenuStyle.menuFont
                    maximumFontLineHeight = max(maximumFontLineHeight, Self.lineHeight(for: runFont))
                }
            }
            // Measure as a single line to match Text.lineLimit(1).
            titleSize = measured.boundingRect(with: .zero, options: []).size
            if kind == .header {
                // Native headers measure unspecified fonts as menu text, then draw them with the header font.
                source.enumerateAttribute(.font, in: NSRange(location: 0, length: source.length)) { [font] value, range, _ in
                    if value == nil {
                        measured.addAttribute(.font, value: font, range: range)
                    }
                }
            }
            let firstLineRange = NSRange(location: 0, length: min(source.length, lineEnd))
            if firstLineRange.length > 0 {
                var fontRange = NSRange()
                _ = measured.attribute(.font, at: 0, longestEffectiveRange: &fontRange, in: firstLineRange)
                hasUniformFont = fontRange == firstLineRange
            }
            self.title = .attributed(measured)
        }
        if kind == .header, hasUniformFont {
            minimumTitleHeight = max(minimumTitleHeight, titleSize.height)
        }
        self.minimumTitleHeight = minimumTitleHeight

        // Include baseline offsets in addition to the font and image bounds.
        let rowHeight: CGFloat =
            if kind == .separator {
                hidesSeparator ? 0 : AlternativeMenuStyle.current.separatorRowHeight
            } else {
                ceil(
                    max(
                        AlternativeMenuStyle.current.minimumRowHeight,
                        max(imageLayoutSize.height, titleSize.height, maximumFontLineHeight) + AlternativeMenuStyle.current.rowContentVerticalPadding
                    )
                )
            }
        if hasUniformFont, kind == .header || maximumFontLineHeight > minimumTitleHeight {
            let titleFrameHeight = max(minimumTitleHeight, titleSize.height)
            // Native headers and larger-font titles keep the bottom inset even beside a taller image.
            let baselineOffset = (rowHeight - titleFrameHeight - AlternativeMenuStyle.current.rowContentVerticalPadding) / 2
            if #available(macOS 15, *) {
                self.titleBaselineOffset = baselineOffset
            } else {
                // macOS 14 title fields retain the rounded font box, with text at its top.
                self.titleBaselineOffset = baselineOffset - max(0, maximumFontLineHeight - titleFrameHeight)
            }
        } else {
            self.titleBaselineOffset = kind == .header ? AlternativeMenuStyle.current.headerBaselineOffset : 0
        }
        self.verticalRange = verticalOffset..<(verticalOffset + rowHeight)
        self.titleWidthWithInsets = ceil(titleSize.width) + titleWidthPadding + leadingInset + AlternativeMenuStyle.current.rowTextInset
    }

    func iconWidth(symbolColumnWidth: CGFloat, bitmapColumnWidth: CGFloat) -> CGFloat {
        if let image {
            return isSymbolImage ? symbolColumnWidth : max(bitmapColumnWidth, image.size.width)
        }
        // Older native menus keep imageless titles at the leading inset, even beside symbols.
        if #available(macOS 26, *), kind != .header, kind != .separator {
            return symbolColumnWidth
        }
        return 0
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

    private static func configuredImage(_ image: NSImage?, font: NSFont, isHeader: Bool) -> (image: NSImage?, layoutSize: NSSize) {
        if #available(macOS 26, *) {
            let configuration = NSImage.SymbolConfiguration(pointSize: font.pointSize, weight: .semibold, scale: .small)
            let configured = image.flatMap { $0.withSymbolConfiguration(configuration) ?? $0 }
            return (configured, configured?.size ?? .zero)
        }
        guard isHeader else {
            return (image, image?.size ?? .zero)
        }
        let configuration = NSImage.SymbolConfiguration(pointSize: font.pointSize, weight: .semibold, scale: .medium)
        let layoutSize = image.flatMap { $0.withSymbolConfiguration(configuration.applying($0.symbolConfiguration)) ?? $0 }?.size ?? .zero
        // Fit the original image after the shared icon column width is known.
        return (image, layoutSize)
    }

    private static func lineHeight(for font: NSFont) -> CGFloat {
        // Round ascent and descent separately to match measured native single-font row heights.
        ceil(font.ascender) + ceil(-font.descender) + font.leading
    }
}

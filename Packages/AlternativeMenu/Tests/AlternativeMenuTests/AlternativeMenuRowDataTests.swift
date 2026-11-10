import AppKit
import Testing

@testable import AlternativeMenu

@MainActor
struct AlternativeMenuRowDataTests {
    @Test
    func keepsPlainAndAttributedEmptyTitlesTheSameSize() {
        let plain = AlternativeMenuRowData(item: .action("") { _ in }, verticalOffset: 0)
        let attributed = AlternativeMenuRowData(item: .action(attributedTitle: NSAttributedString(string: "")) { _ in }, verticalOffset: 0)

        #expect(plain.title.string.isEmpty)
        #expect(attributed.title.string.isEmpty)
        #expect(plain.titleWidthWithInsets == AlternativeMenuStyle.current.imagelessRowTextInset + AlternativeMenuStyle.current.rowTextInset)
        #expect(attributed.titleWidthWithInsets == plain.titleWidthWithInsets)
        #expect(attributed.verticalRange == plain.verticalRange)
    }

    @Test(arguments: ["A", " ", "Hello", "e\u{0301}"])
    func reservesNativeWidthForNonemptyAttributedTitles(_ text: String) {
        let plain = AlternativeMenuRowData(item: .action(text) { _ in }, verticalOffset: 0)
        let titles = [
            NSAttributedString(string: text),
            NSAttributedString(string: text, attributes: [.font: AlternativeMenuStyle.menuFont])
        ]

        for title in titles {
            let attributed = AlternativeMenuRowData(item: .action(attributedTitle: title) { _ in }, verticalOffset: 0)
            #expect(attributed.titleWidthWithInsets == plain.titleWidthWithInsets + 2)
            #expect(attributed.leadingInset == plain.leadingInset)
            #expect(attributed.rowHeight == plain.rowHeight)
        }
    }

    @Test
    func accountsForImagelessLeadingInsetInMeasuredWidth() {
        let title = "Menu item"
        let plain = AlternativeMenuRowData(item: .action(title) { _ in }, verticalOffset: 0)
        let image = NSImage(size: NSSize(width: 16, height: 16))
        let withImage = AlternativeMenuRowData(item: .action(title, image: image) { _ in }, verticalOffset: 0)
        #expect(withImage.leadingInset == plain.leadingInset)
        #expect(withImage.titleWidthWithInsets == plain.titleWidthWithInsets)
    }

    @Test(arguments: [false, true])
    func appliesNativeSymbolSizingWithoutChangingTheSuppliedImage(isHeader: Bool) throws {
        let symbol = try #require(NSImage(systemSymbolName: "doc.text", accessibilityDescription: nil))
        let image = try #require(symbol.withSymbolConfiguration(.init(pointSize: 20, weight: .bold, scale: .large)))
        let originalSize = image.size
        let item: AlternativeMenuItem =
            if isHeader {
                .sectionHeader("Header", image: image)
            } else {
                .action("Title", image: image) { _ in }
            }
        let row = AlternativeMenuRowData(item: item, verticalOffset: 0)

        if #available(macOS 26, *) {
            let pointSize = isHeader ? AlternativeMenuStyle.headerFont.pointSize : AlternativeMenuStyle.menuFont.pointSize
            let configured = try #require(image.withSymbolConfiguration(.init(pointSize: pointSize, weight: .semibold, scale: .small)))
            #expect(row.image?.size == configured.size)
            #expect(row.image?.size != originalSize)
        } else {
            #expect(row.image?.size == originalSize)
        }
        #expect(image.size == originalSize)
    }

    @Test
    func preservesOriginalLegacyHeaderSymbols() throws {
        let source = try #require(NSImage(systemSymbolName: "chevron.left.forwardslash.chevron.right", accessibilityDescription: nil))
        let originalSize = source.size
        let row = AlternativeMenuRowData(item: .sectionHeader("Header", image: source), verticalOffset: 0)
        let image = try #require(row.image)

        if #available(macOS 26, *) {
            #expect(image.size == row.imageLayoutSize)
        } else {
            #expect(image.size == originalSize)
            #expect(image.symbolConfiguration == source.symbolConfiguration)
        }
        #expect(source.size == originalSize)
    }

    @Test
    func removesParagraphFormattingWithoutChangingTheOriginalTitle() throws {
        let expected = NSAttributedString(string: "Paragraph title", attributes: [.font: AlternativeMenuStyle.menuFont, .kern: 2])
        let paragraph = NSMutableParagraphStyle()
        paragraph.alignment = .right
        paragraph.headIndent = 48
        paragraph.firstLineHeadIndent = 64
        paragraph.lineSpacing = 200
        let source = NSMutableAttributedString(attributedString: expected)
        source.addAttribute(.paragraphStyle, value: paragraph, range: NSRange(location: 0, length: source.length))
        let original = NSAttributedString(attributedString: source)
        let item = AlternativeMenuItem.action(attributedTitle: source) { _ in }
        let row = AlternativeMenuRowData(item: item, verticalOffset: 0)
        let unstyled = AlternativeMenuRowData(item: .action(attributedTitle: expected) { _ in }, verticalOffset: 0)

        #expect(try attributedTitle(row.title).isEqual(to: expected))
        #expect(row.titleWidthWithInsets == unstyled.titleWidthWithInsets)
        #expect(row.verticalRange == unstyled.verticalRange)
        #expect(source.isEqual(to: original))
        #expect(try attributedTitle(item.titleContent).isEqual(to: original))
    }

    @Test(arguments: [false, true])
    func preservesTitleAttributesAndOnlyLightensExplicitDisabledForeground(isEnabled: Bool) throws {
        let source = NSMutableAttributedString(
            string: "Styled plain",
            attributes: [
                .backgroundColor: NSColor.yellow, .kern: 2,
                .underlineStyle: NSUnderlineStyle.single.rawValue,
                .strikethroughStyle: NSUnderlineStyle.single.rawValue, .baselineOffset: 3
            ]
        )
        let styledRange = NSRange(location: 0, length: 6)
        let color = NSColor(calibratedRed: 0.2, green: 0.4, blue: 0.6, alpha: 1)
        source.addAttributes([.font: NSFont.systemFont(ofSize: 24), .foregroundColor: color], range: styledRange)
        let row = AlternativeMenuRowData(item: .action(attributedTitle: source, isEnabled: isEnabled) { _ in }, verticalOffset: 0)
        let expected = NSMutableAttributedString(attributedString: source)
        expected.addAttribute(
            .font,
            value: AlternativeMenuStyle.menuFont,
            range: NSRange(location: styledRange.upperBound, length: source.length - styledRange.upperBound)
        )
        if !isEnabled {
            let disabledColor = try #require(color.blended(withFraction: 0.5, of: .white))
            expected.addAttribute(.foregroundColor, value: disabledColor, range: styledRange)
        }

        #expect(try attributedTitle(row.title).isEqual(to: expected))
        #expect(source.attribute(.foregroundColor, at: 0, effectiveRange: nil) as? NSColor == color)
    }

    @Test(arguments: [CGFloat(-3), CGFloat(3)])
    func measuresTitleAttributesThatAffectWidth(_ kern: CGFloat) {
        let title = "A long attributed menu item"
        let font = NSFont.menuFont(ofSize: 0)
        let kerned = NSAttributedString(string: title, attributes: [.font: font, .kern: kern])
        let plain = AlternativeMenuRowData(item: .action(title, isEnabled: false) { _ in }, verticalOffset: 0)
        let row = AlternativeMenuRowData(item: .action(attributedTitle: kerned, isEnabled: false) { _ in }, verticalOffset: 0)

        #expect(row.titleWidthWithInsets == ceil(kerned.size().width) + AlternativeMenuStyle.current.attributedTitleWidthPadding + AlternativeMenuStyle.current.imagelessRowTextInset + AlternativeMenuStyle.current.rowTextInset)
        #expect(row.titleWidthWithInsets != plain.titleWidthWithInsets)
    }

    @Test
    func growsRowsToFitLargerFontRuns() {
        let title = "Mixed title"
        let styled = NSMutableAttributedString(string: title, attributes: [.font: NSFont.menuFont(ofSize: 0)])
        styled.addAttribute(.font, value: NSFont.systemFont(ofSize: 54), range: (title as NSString).range(of: "title"))
        let plain = AlternativeMenuRowData(item: .action(title, isEnabled: false) { _ in }, verticalOffset: 0)
        let row = AlternativeMenuRowData(item: .action(attributedTitle: styled, isEnabled: false) { _ in }, verticalOffset: 0)

        #expect(row.titleWidthWithInsets > plain.titleWidthWithInsets)
        #expect(row.titleWidthWithInsets == ceil(styled.size().width) + AlternativeMenuStyle.current.attributedTitleWidthPadding + AlternativeMenuStyle.current.imagelessRowTextInset + AlternativeMenuStyle.current.rowTextInset)
        #expect(row.rowHeight > plain.rowHeight)
        #expect(row.rowHeight >= ceil(styled.size().height))
    }

    @Test(arguments: [
        (CGFloat(18), CGFloat(28)), (20, 31), (22, 33), (24, 36), (26, 38),
        (28, 40), (30, 43), (32, 44), (36, 49), (40, 54)
    ])
    func matchesMeasuredNativeHeightsForUniformFonts(_ sample: (fontSize: CGFloat, rowHeight: CGFloat)) {
        let title = NSAttributedString(string: "Uniform title", attributes: [.font: NSFont.systemFont(ofSize: sample.fontSize)])
        let row = AlternativeMenuRowData(item: .action(attributedTitle: title) { _ in }, verticalOffset: 0)

        #expect(row.rowHeight == sample.rowHeight)
    }

    @Test(arguments: [
        BaselineExpectation(fontSize: 10, modernOffset: 0, macOS15Offset: 0, legacyOffset: 0),
        BaselineExpectation(fontSize: 13, modernOffset: 0, macOS15Offset: 0, legacyOffset: 0),
        BaselineExpectation(fontSize: 14, modernOffset: 0.5, macOS15Offset: 0, legacyOffset: 0),
        BaselineExpectation(fontSize: 20, modernOffset: 0.5, macOS15Offset: 0.5, legacyOffset: -0.5),
        BaselineExpectation(fontSize: 24, modernOffset: 1, macOS15Offset: 1, legacyOffset: -1),
        BaselineExpectation(fontSize: 28, modernOffset: 0.5, macOS15Offset: 0.5, legacyOffset: -0.5),
        BaselineExpectation(fontSize: 32, modernOffset: 0, macOS15Offset: 0, legacyOffset: 0)
    ])
    private func placesUniformTitlesAtMeasuredNativeBaselines(_ sample: BaselineExpectation) {
        let title = NSAttributedString(string: "Uniform title", attributes: [.font: NSFont.systemFont(ofSize: sample.fontSize)])
        let row = AlternativeMenuRowData(item: .action(attributedTitle: title) { _ in }, verticalOffset: 0)

        #expect(row.titleBaselineOffset == sample.expectedOffset)
    }

    @Test
    func keepsDefaultFontTitlesCenteredBesideTallImages() {
        let image = NSImage(size: NSSize(width: 56, height: 56))
        let text = "Default font"
        let items: [AlternativeMenuItem] = [
            .action(text, image: image) { _ in },
            .action(attributedTitle: NSAttributedString(string: text), image: image) { _ in },
            .action(attributedTitle: NSAttributedString(string: text, attributes: [.font: AlternativeMenuStyle.menuFont]), image: image) { _ in }
        ]

        for item in items {
            let row = AlternativeMenuRowData(item: item, verticalOffset: 0)
            #expect(row.titleBaselineOffset == 0)
            #expect(row.rowHeight == 62)
        }
    }

    @Test(arguments: [
        BaselineExpectation(fontSize: 14, modernOffset: 19.5, macOS15Offset: 19.5, legacyOffset: 19.5),
        BaselineExpectation(fontSize: 24, modernOffset: 14, macOS15Offset: 14, legacyOffset: 12),
        BaselineExpectation(fontSize: 32, modernOffset: 9, macOS15Offset: 9, legacyOffset: 9)
    ])
    private func alignsLargerTitlesAtTheNativeInsetBesideTallImages(_ sample: BaselineExpectation) {
        let title = NSAttributedString(string: "Large font", attributes: [.font: NSFont.systemFont(ofSize: sample.fontSize)])
        let image = NSImage(size: NSSize(width: 56, height: 56))
        let row = AlternativeMenuRowData(item: .action(attributedTitle: title, image: image) { _ in }, verticalOffset: 0)

        #expect(row.rowHeight == 62)
        #expect(row.titleBaselineOffset == sample.expectedOffset)
    }

    @Test(arguments: [false, true])
    func preservesUniformFontPlacementAcrossDecorationsAndHiddenLines(hasHiddenLine: Bool) {
        let visible = NSAttributedString(string: "Uniform title", attributes: [.font: NSFont.systemFont(ofSize: 24)])
        let decorated = NSMutableAttributedString(attributedString: visible)
        decorated.addAttributes([.foregroundColor: NSColor.red, .underlineStyle: NSUnderlineStyle.single.rawValue], range: NSRange(location: 0, length: 7))
        if hasHiddenLine {
            decorated.append(NSAttributedString(string: "\nHidden title", attributes: [.font: NSFont.systemFont(ofSize: 48)]))
        }
        let plain = AlternativeMenuRowData(item: .action(attributedTitle: visible) { _ in }, verticalOffset: 0)
        let row = AlternativeMenuRowData(item: .action(attributedTitle: decorated) { _ in }, verticalOffset: 0)

        #expect(row.titleBaselineOffset == plain.titleBaselineOffset)
        #expect(row.rowHeight == plain.rowHeight)
    }

    @Test(arguments: [
        HeaderBaselineExpectation(fontSize: 11, imageHeight: 0, modernOffset: 2, macOS15Offset: 1, legacyOffset: 1),
        HeaderBaselineExpectation(fontSize: 13, imageHeight: 0, modernOffset: 1, macOS15Offset: 0, legacyOffset: 0),
        HeaderBaselineExpectation(fontSize: 24, imageHeight: 0, modernOffset: 1, macOS15Offset: 1, legacyOffset: -1),
        HeaderBaselineExpectation(fontSize: 24, imageHeight: 56, modernOffset: 14, macOS15Offset: 14, legacyOffset: 12)
    ])
    private func placesUniformHeadersAtMeasuredNativeBaselines(_ sample: HeaderBaselineExpectation) {
        let title = NSAttributedString(string: "Header", attributes: [.font: NSFont.systemFont(ofSize: sample.fontSize)])
        let image = sample.imageHeight > 0 ? NSImage(size: NSSize(width: 56, height: sample.imageHeight)) : nil
        let row = AlternativeMenuRowData(item: .sectionHeader(attributedTitle: title, image: image), verticalOffset: 0)

        #expect(row.titleBaselineOffset == sample.expectedOffset)
    }

    @Test(arguments: [CGFloat(0), CGFloat(4)])
    func measuresImplicitHeaderFontsBeforeApplyingTheHeaderFont(_ baselineOffset: CGFloat) throws {
        let title = NSAttributedString(string: "A Long Implicit Attributed Header", attributes: [.baselineOffset: baselineOffset])
        let header = AlternativeMenuRowData(item: .sectionHeader(attributedTitle: title), verticalOffset: 0)
        let body = AlternativeMenuRowData(item: .action(attributedTitle: title) { _ in }, verticalOffset: 0)
        let displayed = try attributedTitle(header.title)

        #expect(header.titleWidthWithInsets == body.titleWidthWithInsets)
        #expect(header.rowHeight == body.rowHeight)
        #expect(header.minimumTitleHeight == 16 + baselineOffset)
        #expect(displayed.attribute(.font, at: 0, effectiveRange: nil) as? NSFont == AlternativeMenuStyle.headerFont)
        #expect(title.attribute(.font, at: 0, effectiveRange: nil) == nil)
        if #available(macOS 26, *) {
            #expect(header.titleBaselineOffset == (baselineOffset == 0 ? 1 : 0))
        } else {
            #expect(header.titleBaselineOffset == 0)
        }
    }

    @Test(arguments: [false, true])
    func keepsMaximumFontHeightForMixedTitles(largerFontFirst: Bool) {
        let text = "Mixed 13 pt and 28 pt"
        let title = NSMutableAttributedString(string: text, attributes: [.font: NSFont.systemFont(ofSize: largerFontFirst ? 28 : 13)])
        title.addAttribute(.font, value: NSFont.systemFont(ofSize: largerFontFirst ? 13 : 28), range: (text as NSString).range(of: "28 pt"))
        let row = AlternativeMenuRowData(item: .action(attributedTitle: title) { _ in }, verticalOffset: 0)

        // Retain maximum-font sizing; NSMenu's mixed-title layout is 1 pt shorter here.
        #expect(row.rowHeight == 40)
        #expect(row.titleBaselineOffset == 0)
    }

    @Test
    func keepsBaselineAndImageHeightContributions() {
        let title = NSAttributedString(string: "Raised title", attributes: [.font: NSFont.systemFont(ofSize: 24), .baselineOffset: 4])
        let raised = AlternativeMenuRowData(item: .action(attributedTitle: title) { _ in }, verticalOffset: 0)
        let image = NSImage(size: NSSize(width: 16, height: 56))
        let withImage = AlternativeMenuRowData(item: .action("Large image", image: image) { _ in }, verticalOffset: 0)

        #expect(raised.rowHeight == 38)
        #expect(withImage.rowHeight == 62)
        #expect(raised.titleBaselineOffset == 0)
        #expect(withImage.titleBaselineOffset == 0)
    }

    @Test(arguments: [false, true], [false, true])
    func showsSubmenuArrowsRegardlessOfContentsAndEnabledState(isEnabled: Bool, hasChildren: Bool) {
        let children: [AlternativeMenuItem] = hasChildren ? [.action("Child") { _ in }] : []
        let item = AlternativeMenuItem.submenu("Submenu", menu: .init(items: children), isEnabled: isEnabled)
        let row = AlternativeMenuRowData(item: item, verticalOffset: 0)

        #expect(row.kind == (isEnabled ? .item : .disabled))
        #expect(row.hasSubmenu)
        #expect(row.trailingWidth == AlternativeMenuStyle.current.submenuColumnWidth)
    }

    @Test(arguments: ["\n", "\r\n", "\u{2028}", "\u{2029}"])
    func measuresOnlyTheFirstTitleLine(_ lineBreak: String) {
        let firstLine = "Visible title"
        let secondLine = "Hidden second line"
        let text = firstLine + lineBreak + secondLine
        let styled = NSMutableAttributedString(string: text)
        styled.addAttributes(
            [.font: NSFont.systemFont(ofSize: 48), .baselineOffset: 50],
            range: (text as NSString).range(of: secondLine)
        )
        let plainSingleLine = AlternativeMenuRowData(item: .action(firstLine) { _ in }, verticalOffset: 0)
        let attributedSingleLine = AlternativeMenuRowData(item: .action(attributedTitle: NSAttributedString(string: firstLine)) { _ in }, verticalOffset: 0)
        let rows = [
            (AlternativeMenuRowData(item: .action(text) { _ in }, verticalOffset: 0), plainSingleLine),
            (AlternativeMenuRowData(item: .action(attributedTitle: styled) { _ in }, verticalOffset: 0), attributedSingleLine)
        ]

        for (row, singleLine) in rows {
            #expect(row.titleWidthWithInsets == singleLine.titleWidthWithInsets)
            #expect(row.verticalRange == singleLine.verticalRange)
        }
    }

    private func attributedTitle(_ content: AlternativeMenuItem.Title) throws -> NSAttributedString {
        if case .attributed(let title) = content {
            return title
        }
        return try #require(nil as NSAttributedString?)
    }
}

private struct BaselineExpectation {
    let fontSize: CGFloat
    let modernOffset: CGFloat
    let macOS15Offset: CGFloat
    let legacyOffset: CGFloat

    var expectedOffset: CGFloat {
        if #available(macOS 26, *) {
            modernOffset
        } else if #available(macOS 15, *) {
            macOS15Offset
        } else {
            legacyOffset
        }
    }
}

private struct HeaderBaselineExpectation {
    let fontSize: CGFloat
    let imageHeight: CGFloat
    let modernOffset: CGFloat
    let macOS15Offset: CGFloat
    let legacyOffset: CGFloat

    var expectedOffset: CGFloat {
        if #available(macOS 26, *) {
            modernOffset
        } else if #available(macOS 15, *) {
            macOS15Offset
        } else {
            legacyOffset
        }
    }
}

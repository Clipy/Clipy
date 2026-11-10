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

    @Test
    func accountsForImagelessLeadingInsetInMeasuredWidth() {
        let title = "Menu item"
        let plain = AlternativeMenuRowData(item: .action(title) { _ in }, verticalOffset: 0)
        let image = NSImage(size: NSSize(width: 16, height: 16))
        let withImage = AlternativeMenuRowData(item: .action(title, image: image) { _ in }, verticalOffset: 0)
        #expect(withImage.leadingInset == plain.leadingInset)
        #expect(withImage.titleWidthWithInsets == plain.titleWidthWithInsets)
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

        #expect(row.titleWidthWithInsets == ceil(kerned.size().width) + AlternativeMenuStyle.current.imagelessRowTextInset + AlternativeMenuStyle.current.rowTextInset)
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
        #expect(row.titleWidthWithInsets == ceil(styled.size().width) + AlternativeMenuStyle.current.imagelessRowTextInset + AlternativeMenuStyle.current.rowTextInset)
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

    @Test(arguments: [CGFloat(0), CGFloat(4)])
    func measuresImplicitHeaderFontsBeforeApplyingTheHeaderFont(_ baselineOffset: CGFloat) throws {
        let title = NSAttributedString(string: "A Long Implicit Attributed Header", attributes: [.baselineOffset: baselineOffset])
        let header = AlternativeMenuRowData(item: .sectionHeader(attributedTitle: title), verticalOffset: 0)
        let body = AlternativeMenuRowData(item: .action(attributedTitle: title) { _ in }, verticalOffset: 0)
        let displayed = try attributedTitle(header.title)

        #expect(header.titleWidthWithInsets == body.titleWidthWithInsets)
        #expect(header.rowHeight == body.rowHeight)
        #expect(displayed.attribute(.font, at: 0, effectiveRange: nil) as? NSFont == AlternativeMenuStyle.headerFont)
        #expect(title.attribute(.font, at: 0, effectiveRange: nil) == nil)
    }

    @Test(arguments: [false, true])
    func keepsMaximumFontHeightForMixedTitles(largerFontFirst: Bool) {
        let text = "Mixed 13 pt and 28 pt"
        let title = NSMutableAttributedString(string: text, attributes: [.font: NSFont.systemFont(ofSize: largerFontFirst ? 28 : 13)])
        title.addAttribute(.font, value: NSFont.systemFont(ofSize: largerFontFirst ? 13 : 28), range: (text as NSString).range(of: "28 pt"))
        let row = AlternativeMenuRowData(item: .action(attributedTitle: title) { _ in }, verticalOffset: 0)

        // Retain maximum-font sizing; NSMenu's mixed-title layout is 1 pt shorter here.
        #expect(row.rowHeight == 40)
    }

    @Test
    func keepsBaselineAndImageHeightContributions() {
        let title = NSAttributedString(string: "Raised title", attributes: [.font: NSFont.systemFont(ofSize: 24), .baselineOffset: 4])
        let raised = AlternativeMenuRowData(item: .action(attributedTitle: title) { _ in }, verticalOffset: 0)
        let image = NSImage(size: NSSize(width: 16, height: 56))
        let withImage = AlternativeMenuRowData(item: .action("Large image", image: image) { _ in }, verticalOffset: 0)

        #expect(raised.rowHeight == 38)
        #expect(withImage.rowHeight == 62)
    }

    @Test(arguments: [false, true], [false, true])
    func showsSubmenuArrowsRegardlessOfContentsAndEnabledState(isEnabled: Bool, hasChildren: Bool) {
        let children: [AlternativeMenuItem] = hasChildren ? [.action("Child") { _ in }] : []
        let item = AlternativeMenuItem.submenu("Submenu", menu: .init(items: children), isEnabled: isEnabled)
        let row = AlternativeMenuRowData(item: item, verticalOffset: 0)

        #expect(row.kind == (isEnabled ? .item : .disabled))
        #expect(row.hasSubmenu)
        #expect(row.trailingWidth(keyColumnWidth: 0) == AlternativeMenuStyle.current.submenuColumnWidth)
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
        let singleLine = AlternativeMenuRowData(item: .action(firstLine) { _ in }, verticalOffset: 0)
        let rows = [
            AlternativeMenuRowData(item: .action(text) { _ in }, verticalOffset: 0),
            AlternativeMenuRowData(item: .action(attributedTitle: styled) { _ in }, verticalOffset: 0)
        ]

        for row in rows {
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

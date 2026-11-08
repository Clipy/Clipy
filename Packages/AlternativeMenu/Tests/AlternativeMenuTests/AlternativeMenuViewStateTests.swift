import AppKit
import Testing

@testable import AlternativeMenu

@MainActor
struct AlternativeMenuViewStateTests {
    @Test(arguments: [NSImage.SymbolScale.small, .medium, .large])
    func alignsSymbolsAndMissingImagesRegardlessOfSuppliedScale(_ scale: NSImage.SymbolScale) throws {
        let narrow = try symbol("doc", scale: scale)
        let wide = try symbol("chevron.left.forwardslash.chevron.right", scale: scale)
        let state = makeState([
            .action("Title", image: narrow) { _ in },
            .action("Title", image: wide) { _ in },
            .action("Title", isEnabled: false, image: narrow) { _ in },
            .action("Title") { _ in }
        ])
        let smallWidth = try #require(wide.withSymbolConfiguration(.init(scale: .small))).size.width

        #expect(state.iconWidths == Array(repeating: smallWidth, count: 4))
        #expect(state.rows.prefix(3).allSatisfy { $0.isSymbolImage })
        #expect(state.rows[0].image?.size == narrow.withSymbolConfiguration(.init(scale: .small))?.size)
    }

    @Test(arguments: [false, true])
    func keepsBitmapWidthsIndependentOfTheSymbolColumn(isTemplate: Bool) throws {
        let bitmap = NSImage(size: NSSize(width: 56, height: 32))
        bitmap.isTemplate = isTemplate
        let image = try symbol("doc")
        let state = makeState([
            .action("Title", image: bitmap) { _ in },
            .action("Title", image: image) { _ in },
            .action("Title") { _ in }
        ])
        let symbolWidth = try #require(image.withSymbolConfiguration(.init(scale: .small))).size.width
        #expect(!state.rows[0].isSymbolImage)
        #expect(state.rows[0].image?.size == bitmap.size)
        #expect(symbolWidth < bitmap.size.width)
        #expect(state.iconWidths == [bitmap.size.width, symbolWidth, symbolWidth])

        let withoutSymbol = makeState([.action("Title", image: bitmap) { _ in }, .action("Title") { _ in }])
        #expect(withoutSymbol.iconWidths == [56, 0])
    }

    @Test
    func aHeaderSymbolDoesNotIndentFollowingRows() throws {
        let image = try symbol("doc")
        let state = makeState([.sectionHeader("Header", image: image), .action("Title") { _ in }])
        let size = try headerSymbolSize(image)

        #expect(state.rows[0].image?.size == size)
        #expect(state.iconWidths == [size.width, 0])
    }

    @Test
    func headerSymbolsIndentThePrecedingGroupsAtTheSharedWidth() throws {
        let narrow = try symbol("doc")
        let wide = try symbol("chevron.left.forwardslash.chevron.right")
        let state = makeState([
            .action("Before the first header") { _ in },
            .sectionHeader("First header", image: narrow),
            .action("Between headers") { _ in },
            .sectionHeader("Second header", image: wide),
            .action("After the last header") { _ in }
        ])
        let width = try headerSymbolSize(wide).width

        #expect(state.iconWidths == [width, width, width, width, 0])
    }

    @Test
    func aBodySymbolCanWidenAnEarlierHeaderColumn() throws {
        let narrow = try symbol("doc")
        let wide = try symbol("chevron.left.forwardslash.chevron.right")
        let state = makeState([
            .action("Before the header") { _ in },
            .sectionHeader("Header", image: narrow),
            .action("After the header") { _ in },
            .action("Wide symbol", image: wide) { _ in }
        ])
        let width = try #require(wide.withSymbolConfiguration(.init(scale: .small))).size.width

        #expect(state.iconWidths == [width, width, width, width])
    }

    @Test
    func indentsSymbolGroupsAndReservesWidthAcrossTheWholeMenu() throws {
        let narrow = try symbol("doc")
        let wide = try symbol("chevron.left.forwardslash.chevron.right")
        let bitmap = NSImage(size: NSSize(width: 56, height: 32))
        let state = makeState([
            .action("A deliberately long title in a group without symbols") { _ in },
            .separator,
            .action("Before") { _ in },
            .action("Disabled symbol", isEnabled: false, image: narrow) { _ in },
            .action("After") { _ in },
            .sectionHeader("Header"),
            .action("Unindented") { _ in },
            .action("Thumbnail", image: bitmap) { _ in },
            .separator,
            .action("Wide symbol", image: wide) { _ in },
            .action("Indented") { _ in }
        ])
        let width = try #require(wide.withSymbolConfiguration(.init(scale: .small))).size.width

        #expect(state.iconWidths == [0, 0, width, width, width, 0, 0, 56, 0, width, width])
        #expect(state.contentWidth == state.rows[0].titleWidthWithInsets + width + AlternativeMenuStyle.iconSpacing)
    }

    @Test
    func recomputesIndentationWhenRowsAreReplaced() throws {
        let image = try symbol("doc")
        let plain = AlternativeMenuItem.action("Plain") { _ in }
        let icon = AlternativeMenuItem.action("Symbol", image: image) { _ in }
        let state = makeState([plain, .separator, icon])
        let width = try #require(image.withSymbolConfiguration(.init(scale: .small))).size.width

        #expect(state.iconWidths == [0, 0, width])

        state.setRows(makeState([plain, icon]).rows)
        #expect(state.iconWidths == [width, width])

        state.setRows(makeState([plain]).rows)
        #expect(state.iconWidths == [0])
    }

    @Test
    func preservesTheMinimumSymbolColumnWhenReplacingRows() throws {
        let text = try symbol("text.alignleft")
        let html = try symbol("chevron.left.forwardslash.chevron.right")
        let textItem = AlternativeMenuItem.action("Title", image: text) { _ in }
        let htmlItem = AlternativeMenuItem.action("Title", image: html) { _ in }
        let minimumWidth = try #require(html.withSymbolConfiguration(.init(scale: .small))).size.width
        let state = makeState([textItem, htmlItem], minimumSymbolColumnWidth: minimumWidth)
        let contentWidth = state.rows[0].titleWidthWithInsets + minimumWidth + AlternativeMenuStyle.iconSpacing

        #expect(state.iconWidths == [minimumWidth, minimumWidth])
        #expect(state.contentWidth == contentWidth)

        state.setRows([AlternativeMenuRowData(item: textItem, verticalOffset: 0)])

        #expect(state.iconWidths == [minimumWidth])
        #expect(state.contentWidth == contentWidth)
    }

    @Test
    func accommodatesWiderSymbolsAndKeepsOtherGroupsUnindented() throws {
        let image = try symbol("chevron.left.forwardslash.chevron.right")
        let size = try #require(image.withSymbolConfiguration(.init(scale: .small))).size
        let bitmap = NSImage(size: NSSize(width: 56, height: 32))
        let plain = AlternativeMenuItem.action("Title") { _ in }
        let state = makeState(
            [
                plain,
                .action("Thumbnail", image: bitmap) { _ in },
                .separator,
                .action("Wide symbol", image: image) { _ in },
                plain
            ],
            minimumSymbolColumnWidth: size.width - 1
        )

        #expect(state.rows[3].image?.size == size)
        #expect(state.iconWidths == [0, bitmap.size.width, 0, size.width, size.width])

        state.setRows(Array(state.rows.prefix(2)))

        #expect(state.iconWidths == [0, bitmap.size.width])
    }

    @Test
    func refreshesLayoutWhenRowsAreReplacedAndCleared() throws {
        let minimumWidth: CGFloat = 8
        let state = AlternativeMenuViewState(minimumSymbolColumnWidth: minimumWidth)
        #expect(state.keyColumnWidth == 0)
        #expect(state.trailingColumnWidth == 0)
        #expect(state.contentWidth == 0)
        #expect(state.iconWidths.isEmpty)

        let image = try symbol("chevron.left.forwardslash.chevron.right")
        let item = AlternativeMenuItem.action("Title", keyEquivalent: "w", image: image) { _ in }
        let decorated = AlternativeMenuRowData(
            item: item,
            verticalOffset: 0,
            keyEquivalent: AlternativeMenuKeyEquivalent.display(for: "w", modifiers: .command)
        )
        #expect(AlternativeMenuRowData(item: item, verticalOffset: 0).keyEquivalent == .empty)
        let symbolWidth = try #require(decorated.image).size.width
        try #require(symbolWidth > minimumWidth)
        try #require(decorated.keyEquivalent.usesSharedKeyColumn)
        try #require(decorated.keyEquivalent.keyWidth > 0)
        state.setRows([decorated])
        #expect(state.keyColumnWidth == decorated.keyEquivalent.keyWidth)
        #expect(state.trailingColumnWidth > 0)
        #expect(state.iconWidths == [symbolWidth])
        let decoratedWidth = state.contentWidth

        let plain = AlternativeMenuRowData(item: .action("Title") { _ in }, verticalOffset: 0)
        state.setRows([plain])
        #expect(state.keyColumnWidth == 0)
        #expect(state.trailingColumnWidth == 0)
        #expect(state.iconWidths == [0])
        #expect(state.contentWidth == plain.titleWidthWithInsets + minimumWidth + AlternativeMenuStyle.iconSpacing)
        #expect(state.contentWidth < decoratedWidth)

        state.setRows([decorated])
        state.setRows([])
        #expect(state.rows.isEmpty)
        #expect(state.keyColumnWidth == 0)
        #expect(state.trailingColumnWidth == 0)
        #expect(state.contentWidth == 0)
        #expect(state.iconWidths.isEmpty)
    }

    @Test
    func sharesTheWidestTrailingColumnAcrossRows() {
        let longTitle = AlternativeMenuItem.action("A long clipboard title with no key equivalent", isEnabled: false) { _ in }
        let narrowShortcut = AlternativeMenuItem.action("Narrow", keyEquivalent: "1", keyEquivalentModifierMask: []) { _ in }
        let wideShortcut = AlternativeMenuItem.action(
            "Wide",
            keyEquivalent: "a",
            keyEquivalentModifierMask: [.control, .option, .shift, .command]
        ) { _ in }
        let submenu = AlternativeMenuItem.submenu("Submenu", menu: .init(items: [.action("Child") { _ in }]))
        let baseWidth = makeStateWithKeyEquivalents([longTitle]).contentWidth
        let narrowWidth = makeStateWithKeyEquivalents([longTitle, narrowShortcut]).contentWidth
        let wideWidth = makeStateWithKeyEquivalents([longTitle, wideShortcut]).contentWidth

        #expect(narrowWidth == baseWidth + AlternativeMenuStyle.minimumKeyEquivalentColumnWidth)
        #expect(wideWidth > narrowWidth)
        let combined = makeStateWithKeyEquivalents([longTitle, narrowShortcut, wideShortcut, submenu])
        #expect(combined.contentWidth == wideWidth)
    }

    @Test
    func keepsDigitColumnsNarrowWhenALongLocalizedKeyNameIsAdded() throws {
        let items: [AlternativeMenuItem] = (0...9).map { .action("Title", keyEquivalent: String($0)) { _ in } }
        let digits = makeStateWithKeyEquivalents(items)
        let function = makeStateWithKeyEquivalents([.action("Title", keyEquivalent: "\u{f70f}") { _ in }])
        let withSpace = makeStateWithKeyEquivalents(items + [.action("Title", keyEquivalent: " ") { _ in }])
        let space = try #require(withSpace.rows.last)

        #expect(digits.keyColumnWidth > 0)
        #expect(digits.keyColumnWidth < function.keyColumnWidth)
        #expect(space.keyEquivalent.keyWidth > digits.keyColumnWidth)
        #expect(withSpace.keyColumnWidth == digits.keyColumnWidth)
    }

    @Test(arguments: ["w", "\u{f70f}"])
    func reservesMoreTrailingSpaceWhenABareKeyWidensTheSharedColumn(_ key: String) {
        let modified = AlternativeMenuItem.action(
            "Title",
            keyEquivalent: "a",
            keyEquivalentModifierMask: [.control, .option, .shift, .command]
        ) { _ in }
        let original = makeStateWithKeyEquivalents([modified])
        let expanded = makeStateWithKeyEquivalents([modified, .action("Title", keyEquivalent: key, keyEquivalentModifierMask: []) { _ in }])

        #expect(expanded.keyColumnWidth > original.keyColumnWidth)
        #expect(expanded.trailingColumnWidth > original.trailingColumnWidth)
        #expect(expanded.contentWidth > original.contentWidth)
    }

    private func makeStateWithKeyEquivalents(_ items: [AlternativeMenuItem]) -> AlternativeMenuViewState {
        let state = AlternativeMenuViewState()
        let matcher = AlternativeMenuKeyEquivalentMatcher()
        matcher.updateItems(items)
        state.setRows(
            items.enumerated().map { index, item in
                AlternativeMenuRowData(item: item, verticalOffset: 0, keyEquivalent: matcher.display(at: [index]))
            }
        )
        return state
    }

    private func symbol(_ name: String, scale: NSImage.SymbolScale = .medium) throws -> NSImage {
        let image = try #require(NSImage(systemSymbolName: name, accessibilityDescription: nil))
        return try #require(image.withSymbolConfiguration(.init(pointSize: 13, weight: .regular, scale: scale)))
    }

    private func headerSymbolSize(_ image: NSImage) throws -> NSSize {
        let configuration = NSImage.SymbolConfiguration(
            pointSize: AlternativeMenuStyle.headerFont.pointSize,
            weight: .semibold,
            scale: .small
        )
        return try #require(image.withSymbolConfiguration(configuration)).size
    }

    private func makeState(_ items: [AlternativeMenuItem], minimumSymbolColumnWidth: CGFloat = 0) -> AlternativeMenuViewState {
        let state = AlternativeMenuViewState(minimumSymbolColumnWidth: minimumSymbolColumnWidth)
        state.setRows(items.map { AlternativeMenuRowData(item: $0, verticalOffset: 0) })
        return state
    }
}

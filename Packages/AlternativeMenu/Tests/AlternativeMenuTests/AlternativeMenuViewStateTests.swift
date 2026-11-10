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
        let width = try bodySymbolSize(wide).width

        if #available(macOS 26, *) {
            #expect(state.iconWidths == Array(repeating: width, count: 4))
        } else {
            #expect(state.iconWidths == [width, width, width, 0])
        }
        #expect(state.rows.prefix(3).allSatisfy { $0.isSymbolImage })
        #expect(state.rows[0].image?.size == (try bodySymbolSize(narrow)))
    }

    @Test(arguments: [false, true])
    func appliesNativeColumnSharingToSymbolsAndBitmaps(isTemplate: Bool) throws {
        let bitmap = NSImage(size: NSSize(width: 56, height: 32))
        bitmap.isTemplate = isTemplate
        let image = try symbol("doc")
        let state = makeState([
            .action("Title", image: bitmap) { _ in },
            .action("Title", image: image) { _ in },
            .action("Title") { _ in }
        ])
        let symbolWidth = try bodySymbolSize(image).width
        #expect(!state.rows[0].isSymbolImage)
        #expect(state.rows[0].image?.size == bitmap.size)
        #expect(symbolWidth < bitmap.size.width)
        if #available(macOS 27, *) {
            #expect(state.iconWidths == [bitmap.size.width, symbolWidth, 0])
        } else if #available(macOS 26, *) {
            #expect(state.iconWidths == [bitmap.size.width, symbolWidth, symbolWidth])
        } else {
            #expect(state.iconWidths == [bitmap.size.width, symbolWidth, 0])
        }

        let withoutSymbol = makeState([.action("Title", image: bitmap) { _ in }, .action("Title") { _ in }])
        #expect(withoutSymbol.iconWidths == [56, 0])
    }

    @Test(arguments: [CGFloat(0), CGFloat(0.5)])
    func sharesBitmapWidthOnlyWithinTheNativeSymbolRatio(_ excess: CGFloat) throws {
        let image = try symbol("folder")
        let symbolWidth = try bodySymbolSize(image).width
        let bitmapWidth = symbolWidth * 1.5 + excess
        let bitmap = NSImage(size: NSSize(width: bitmapWidth, height: 40))
        let state = makeState([
            .action("Symbol", image: image) { _ in },
            .separator,
            .action("Bitmap", image: bitmap) { _ in }
        ])

        #expect(state.iconWidths == [excess == 0 ? bitmapWidth : symbolWidth, 0, bitmapWidth])
    }

    @Test(arguments: [CGFloat(5), CGFloat(5.1)])
    func sharesBitmapWidthsOnlyWhenTheirSpreadFitsTheNativeTolerance(_ difference: CGFloat) throws {
        let image = try symbol("chevron.left.forwardslash.chevron.right")
        let symbolWidth = try bodySymbolSize(image).width
        let narrowerWidth = symbolWidth + 1
        let widerWidth = narrowerWidth + difference
        let state = makeState([
            .action("Symbol", image: image) { _ in },
            .action("Narrow bitmap", image: NSImage(size: NSSize(width: narrowerWidth, height: 8))) { _ in },
            .action("Wide bitmap", image: NSImage(size: NSSize(width: widerWidth, height: 40))) { _ in }
        ])

        #expect(state.iconWidths == (difference == 5 ? Array(repeating: widerWidth, count: 3) : [symbolWidth, narrowerWidth, widerWidth]))
    }

    @Test
    func includesNarrowerBitmapsInTheColumnSharingDecision() throws {
        let image = try symbol("doc")
        let symbolWidth = try bodySymbolSize(image).width
        let narrow = AlternativeMenuItem.action("Narrow", image: NSImage(size: NSSize(width: 8, height: 16))) { _ in }
        let wide = AlternativeMenuItem.action("Wide", image: NSImage(size: NSSize(width: 16, height: 16))) { _ in }
        let state = makeState([.action("Symbol", image: image) { _ in }, narrow, wide])

        #expect(state.iconWidths == [symbolWidth, symbolWidth, 16])
        #expect(makeState([narrow, wide]).iconWidths == [8, 16])
    }

    @Test
    func keepsTheSymbolMinimumFromWideningBitmaps() throws {
        let image = try symbol("doc")
        let bitmap = AlternativeMenuItem.action("Bitmap", image: NSImage(size: NSSize(width: 16, height: 16))) { _ in }
        let state = makeState([.action("Symbol", image: image) { _ in }, bitmap], minimumSymbolColumnWidth: 32)

        #expect(state.iconWidths == [32, 16])
        #expect(makeState([bitmap], minimumSymbolColumnWidth: 32).iconWidths == [16])
    }

    @Test
    func aHeaderSymbolDoesNotIndentFollowingRows() throws {
        let image = try symbol("doc")
        let state = makeState([.sectionHeader("Header", image: image), .action("Title") { _ in }])
        let size = try headerSymbolSize(image)

        #expect(state.rows[0].imageLayoutSize == size)
        #expect(state.iconWidths == [size.width, 0])
    }

    @Test
    func measuresHeaderSymbolLayoutIndependentlyOfItsSourceSize() throws {
        let image = try #require(NSImage(systemSymbolName: "chevron.left.forwardslash.chevron.right", accessibilityDescription: nil))
        let state = makeState([.sectionHeader("Header", image: image), .action("Title") { _ in }])
        let row = try #require(state.rows.first)

        #expect(row.imageLayoutSize == (try headerSymbolSize(image)))
        #expect(state.iconWidths == [row.imageLayoutSize.width, 0])
        if #unavailable(macOS 26) {
            #expect(row.imageLayoutSize != image.size)
        }
    }

    @Test
    func sharesHeaderSymbolWidthsAndAppliesNativeGroupIndentation() throws {
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

        if #available(macOS 27, *) {
            #expect(state.iconWidths == [0, width, 0, width, 0])
        } else if #available(macOS 26, *) {
            #expect(state.iconWidths == [width, width, width, width, 0])
        } else {
            #expect(state.iconWidths == [0, width, 0, width, 0])
        }
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
        let width = try bodySymbolSize(wide).width

        if #available(macOS 27, *) {
            #expect(state.iconWidths == [0, width, 0, width])
        } else if #available(macOS 26, *) {
            #expect(state.iconWidths == [width, width, width, width])
        } else {
            #expect(state.iconWidths == [0, width, 0, width])
        }
    }

    @Test(arguments: [false, true])
    func reservesBodySymbolWidthForALongImagelessHeader(headerFirst: Bool) throws {
        let image = try symbol("doc")
        let header = AlternativeMenuItem.sectionHeader("A deliberately long section header controls the menu width")
        let item = AlternativeMenuItem.action("Item", image: image) { _ in }
        let state = makeState(headerFirst ? [header, item] : [item, header])
        let headerIndex = headerFirst ? 0 : 1
        let width = try bodySymbolSize(image).width

        #expect(state.iconWidths[headerIndex] == 0)
        if #available(macOS 26, *) {
            #expect(state.contentWidth == state.rows[headerIndex].titleWidthWithInsets + width + AlternativeMenuStyle.current.iconSpacing)
        } else {
            #expect(state.contentWidth == state.rows[headerIndex].titleWidthWithInsets)
        }
    }

    @Test(arguments: [CGFloat(0), CGFloat(24)])
    func measuresImagelessRowsBesideHeaderOnlySymbols(_ minimumWidth: CGFloat) throws {
        let image = try symbol("chevron.left.forwardslash.chevron.right")
        let state = makeState(
            [
                .sectionHeader("Header", image: image),
                .action("A deliberately long imageless item controls the menu width") { _ in }
            ],
            minimumSymbolColumnWidth: minimumWidth
        )
        let headerWidth = try headerSymbolSize(image).width
        if minimumWidth > 0 {
            try #require(minimumWidth > headerWidth)
        }
        let reservedWidth: CGFloat
        if #available(macOS 27, *) {
            reservedWidth = minimumWidth
        } else if #available(macOS 26, *) {
            reservedWidth = max(minimumWidth, headerWidth)
        } else {
            reservedWidth = 0
        }

        #expect(state.iconWidths[0] == max(minimumWidth, headerWidth))
        #expect(state.iconWidths[1] == 0)
        #expect(state.contentWidth == state.rows[1].titleWidthWithInsets + (reservedWidth > 0 ? reservedWidth + AlternativeMenuStyle.current.iconSpacing : 0))
    }

    @Test
    func includesWiderHeaderSymbolsInTheSharedWidthWhenABodySymbolExists() throws {
        let headerImage = try symbol("chevron.left.forwardslash.chevron.right")
        let itemImage = try symbol("doc")
        let headerWidth = try headerSymbolSize(headerImage).width
        let itemWidth = try bodySymbolSize(itemImage).width
        try #require(headerWidth > itemWidth)
        let state = makeState([
            .sectionHeader("Header", image: headerImage),
            .action("Item", image: itemImage) { _ in },
            .action("A deliberately long imageless item controls the menu width") { _ in }
        ])

        #expect(state.iconWidths[0] == headerWidth)
        #expect(state.iconWidths[1] == headerWidth)
        if #available(macOS 26, *) {
            #expect(state.contentWidth == state.rows[2].titleWidthWithInsets + headerWidth + AlternativeMenuStyle.current.iconSpacing)
        } else {
            #expect(state.contentWidth == state.rows[2].titleWidthWithInsets)
        }
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
        let width = try bodySymbolSize(wide).width

        if #available(macOS 27, *) {
            #expect(state.iconWidths == [0, 0, 0, width, 0, 0, 0, 56, 0, width, 0])
            #expect(state.contentWidth == state.rows[0].titleWidthWithInsets + width + AlternativeMenuStyle.current.iconSpacing)
        } else if #available(macOS 26, *) {
            #expect(state.iconWidths == [0, 0, width, width, width, 0, 0, 56, 0, width, width])
            #expect(state.contentWidth == state.rows[0].titleWidthWithInsets + width + AlternativeMenuStyle.current.iconSpacing)
        } else {
            #expect(state.iconWidths == [0, 0, 0, width, 0, 0, 0, 56, 0, width, 0])
            #expect(state.contentWidth == state.rows[0].titleWidthWithInsets)
        }
    }

    @Test(arguments: [false, true])
    func appliesNativeSymbolGroupingRulesRegardlessOfEnabledState(isEnabled: Bool) throws {
        let image = try symbol("doc")
        let symbolItem = AlternativeMenuItem.action("Symbol", isEnabled: isEnabled, image: image) { _ in }
        let plain = AlternativeMenuItem.action("Plain", isEnabled: isEnabled) { _ in }
        let bitmap = AlternativeMenuItem.action("Bitmap", isEnabled: isEnabled, image: NSImage(size: NSSize(width: 16, height: 16))) { _ in }
        let header = AlternativeMenuItem.sectionHeader("Header", image: image)
        let width = try bodySymbolSize(image).width
        let cases: [(items: [AlternativeMenuItem], plainIndex: Int, indentsOn27: Bool)] = [
            ([symbolItem, plain], 1, false),
            ([symbolItem, symbolItem, plain], 2, true),
            ([symbolItem, symbolItem, plain, plain], 2, false),
            ([symbolItem, symbolItem, symbolItem, plain, plain], 3, true),
            ([symbolItem, symbolItem, bitmap, plain], 3, false),
            ([symbolItem, symbolItem, symbolItem, bitmap, plain], 4, true),
            ([symbolItem, plain, header], 1, false),
            ([symbolItem, symbolItem, plain, header], 2, true),
            ([symbolItem, symbolItem, plain, .sectionHeader("Header")], 2, true),
            ([symbolItem, symbolItem, plain, .separator], 2, true)
        ]

        for testCase in cases {
            let state = makeState(testCase.items)
            let expectedWidth = max(width, testCase.items.contains { $0.image?.isSymbolImage == false } ? 16 : 0)
            #expect(state.iconWidths[0] == expectedWidth)
            if #available(macOS 27, *) {
                #expect(state.iconWidths[testCase.plainIndex] == (testCase.indentsOn27 ? expectedWidth : 0))
            } else if #available(macOS 26, *) {
                #expect(state.iconWidths[testCase.plainIndex] == expectedWidth)
            } else {
                #expect(state.iconWidths[testCase.plainIndex] == 0)
            }
        }
    }

    @Test
    func recomputesIndentationWhenRowsAreReplaced() throws {
        let image = try symbol("doc")
        let plain = AlternativeMenuItem.action("Plain") { _ in }
        let icon = AlternativeMenuItem.action("Symbol", image: image) { _ in }
        let state = makeState([plain, .separator, icon])
        let width = try bodySymbolSize(image).width

        #expect(state.iconWidths == [0, 0, width])

        state.setRows(makeState([plain, icon]).rows)
        if #available(macOS 27, *) {
            #expect(state.iconWidths == [0, width])
        } else if #available(macOS 26, *) {
            #expect(state.iconWidths == [width, width])
        } else {
            #expect(state.iconWidths == [0, width])
        }

        state.setRows(makeState([plain]).rows)
        #expect(state.iconWidths == [0])
    }

    @Test
    func preservesTheMinimumSymbolColumnWhenReplacingRows() throws {
        let text = try symbol("text.alignleft")
        let html = try symbol("chevron.left.forwardslash.chevron.right")
        let textItem = AlternativeMenuItem.action("Title", image: text) { _ in }
        let htmlItem = AlternativeMenuItem.action("Title", image: html) { _ in }
        let minimumWidth = try bodySymbolSize(html).width
        let state = makeState([textItem, htmlItem], minimumSymbolColumnWidth: minimumWidth)
        let contentWidth = state.rows[0].titleWidthWithInsets + minimumWidth + AlternativeMenuStyle.current.iconSpacing

        #expect(state.iconWidths == [minimumWidth, minimumWidth])
        #expect(state.contentWidth == contentWidth)

        state.setRows([AlternativeMenuRowData(item: textItem, verticalOffset: 0)])

        #expect(state.iconWidths == [minimumWidth])
        #expect(state.contentWidth == contentWidth)
    }

    @Test
    func accommodatesWiderSymbolsAndKeepsOtherGroupsUnindented() throws {
        let image = try symbol("chevron.left.forwardslash.chevron.right")
        let size = try bodySymbolSize(image)
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
        if #available(macOS 27, *) {
            #expect(state.iconWidths == [0, bitmap.size.width, 0, size.width, 0])
        } else if #available(macOS 26, *) {
            #expect(state.iconWidths == [0, bitmap.size.width, 0, size.width, size.width])
        } else {
            #expect(state.iconWidths == [0, bitmap.size.width, 0, size.width, 0])
        }

        state.setRows(Array(state.rows.prefix(2)))

        #expect(state.iconWidths == [0, bitmap.size.width])
    }

    @Test
    func refreshesLayoutWhenRowsAreReplacedAndCleared() throws {
        let minimumWidth: CGFloat = 8
        let state = AlternativeMenuViewState(minimumSymbolColumnWidth: minimumWidth)
        #expect(state.keyColumnWidth == 0)
        #expect(state.modifierColumnWidth == 0)
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
        #expect(state.modifierColumnWidth == decorated.keyEquivalent.modifierColumnWidth)
        #expect(state.trailingColumnWidth > 0)
        #expect(state.iconWidths == [symbolWidth])
        let decoratedWidth = state.contentWidth

        let plain = AlternativeMenuRowData(item: .action("Title") { _ in }, verticalOffset: 0)
        state.setRows([plain])
        #expect(state.keyColumnWidth == 0)
        #expect(state.modifierColumnWidth == 0)
        #expect(state.trailingColumnWidth == 0)
        #expect(state.iconWidths == [0])
        if #available(macOS 26, *) {
            #expect(state.contentWidth == plain.titleWidthWithInsets + minimumWidth + AlternativeMenuStyle.current.iconSpacing)
        } else {
            #expect(state.contentWidth == plain.titleWidthWithInsets)
        }
        #expect(state.contentWidth < decoratedWidth)

        state.setRows([decorated])
        state.setRows([])
        #expect(state.rows.isEmpty)
        #expect(state.keyColumnWidth == 0)
        #expect(state.modifierColumnWidth == 0)
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

        #expect(narrowWidth == baseWidth + AlternativeMenuStyle.current.minimumKeyEquivalentColumnWidth)
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
    func keepsMenuWidthIndependentOfTheSharedDrawingColumn(_ key: String) {
        let modified = AlternativeMenuItem.action(
            "Title",
            keyEquivalent: "a",
            keyEquivalentModifierMask: [.control, .option, .shift, .command]
        ) { _ in }
        let original = makeStateWithKeyEquivalents([modified])
        let expanded = makeStateWithKeyEquivalents([modified, .action("Title", keyEquivalent: key, keyEquivalentModifierMask: []) { _ in }])

        #expect(expanded.keyColumnWidth > original.keyColumnWidth)
        #expect(expanded.trailingColumnWidth == original.trailingColumnWidth)
        #expect(expanded.contentWidth == original.contentWidth)
    }

    @Test
    func appliesAttributedTitleWidthOnlyToItsOwnRow() {
        let plain = AlternativeMenuItem.action("Hello") { _ in }
        let shorter = AlternativeMenuItem.action(attributedTitle: NSAttributedString(string: "A")) { _ in }
        let sameLength = AlternativeMenuItem.action(attributedTitle: NSAttributedString(string: "Hello")) { _ in }
        let plainWidth = makeState([plain]).contentWidth

        #expect(makeState([plain, shorter]).contentWidth == plainWidth)
        #expect(makeState([plain, sameLength]).contentWidth == plainWidth + 2)
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

    private func bodySymbolSize(_ image: NSImage) throws -> NSSize {
        if #available(macOS 26, *) {
            let configuration = NSImage.SymbolConfiguration(
                pointSize: AlternativeMenuStyle.menuFont.pointSize,
                weight: .semibold,
                scale: .small
            )
            return try #require(image.withSymbolConfiguration(configuration)).size
        }
        return image.size
    }

    private func headerSymbolSize(_ image: NSImage) throws -> NSSize {
        let configuration: NSImage.SymbolConfiguration
        if #available(macOS 26, *) {
            configuration = .init(pointSize: AlternativeMenuStyle.headerFont.pointSize, weight: .semibold, scale: .small)
        } else {
            configuration = NSImage.SymbolConfiguration(
                pointSize: AlternativeMenuStyle.headerFont.pointSize,
                weight: .semibold,
                scale: .medium
            ).applying(image.symbolConfiguration)
        }
        return try #require(image.withSymbolConfiguration(configuration)).size
    }

    private func makeState(_ items: [AlternativeMenuItem], minimumSymbolColumnWidth: CGFloat = 0) -> AlternativeMenuViewState {
        let state = AlternativeMenuViewState(minimumSymbolColumnWidth: minimumSymbolColumnWidth)
        state.setRows(items.map { AlternativeMenuRowData(item: $0, verticalOffset: 0) })
        return state
    }
}

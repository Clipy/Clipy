import AppKit
import SwiftUI
import Testing

@testable import AlternativeMenu

@MainActor
struct AlternativeMenuViewTests {
    @Test
    func keepsWidthWithoutKeyEquivalents() {
        let view = AlternativeMenuView(header: nil)
        let item = AlternativeMenuItem.action("Clipboard history item") { _ in }
        let row = AlternativeMenuRowData(item: item, verticalOffset: 0)

        view.setRows([item], selectedIndex: nil)
        #expect(view.preferredWidth == row.titleWidthWithInsets + AlternativeMenuStyle.padding * 2)
    }

    @Test
    func sharesSelectionEmphasisAndResetsItWhenRowsAreReplaced() throws {
        let view = AlternativeMenuView(header: nil)
        let items: [AlternativeMenuItem] = [
            .submenu("Parent", menu: AlternativeMenu(items: [.action("Child") { _ in }])),
            .action("Other") { _ in }
        ]
        let state = try #require(Mirror(reflecting: view).descendant("state") as? AlternativeMenuViewState)
        view.setRows(items, selectedIndex: 0)
        #expect(state.isSelectionEmphasized)

        view.setExpandedRow(0)
        view.isSelectionEmphasized = false
        #expect(state.highlightedRowIndex == 0)
        #expect(!state.isSelectionEmphasized)

        view.setExpandedRow(nil)
        view.isSelectionEmphasized = true
        #expect(state.isSelectionEmphasized)

        view.setExpandedRow(0)
        view.isSelectionEmphasized = false
        view.setRows(items, selectedIndex: 0)
        #expect(view.expandedRow == nil)
        #expect(state.isSelectionEmphasized)
    }

    @Test
    func pagesThroughATallRowInBothDirections() throws {
        let fixture = try ScrollFixture(items: [tallItem, .action("Next") { _ in }])

        #expect(fixture.view.scrollPage(down: true) == 0)
        let firstPage = fixture.offset
        #expect(firstPage > 0 && firstPage < fixture.maximumOffset)

        #expect(fixture.view.scrollPage(down: true) == 1)
        #expect(fixture.offset == fixture.maximumOffset)

        #expect(fixture.view.scrollPage(down: false) == 0)
        #expect(fixture.offset > 0 && fixture.offset < firstPage)
        #expect(fixture.view.scrollPage(down: false) == 0)
        #expect(fixture.offset == 0)
    }

    @Test
    func homeAndEndCanRevealOppositeEdgesOfATallRow() throws {
        let fixture = try ScrollFixture(items: [tallItem])

        fixture.view.scrollRowToVisible(0, alignsTallRowAtBottom: true)
        #expect(fixture.offset > 0)
        #expect(fixture.maximumOffset - fixture.offset <= AlternativeMenuStyle.padding)

        fixture.view.resetScrollPosition()
        fixture.view.scrollRowToVisible(0)
        #expect(fixture.offset == 0)
    }

    @Test
    func retainsPageTargetsForStandardRows() throws {
        let items: [AlternativeMenuItem] = (0..<80).map { index in .action("Row \(index)") { _ in } }
        let fixture = try ScrollFixture(items: items)

        let expectedTargets: [Int]
        if #available(macOS 26, *) {
            expectedTargets = [38, 57, 20, 1]
        } else {
            expectedTargets = [42, 62, 22, 2]
        }
        for (down, expected) in zip([true, true, false, false], expectedTargets) {
            #expect(fixture.view.scrollPage(down: down) == expected)
        }
    }

    @Test(arguments: [false, true])
    func keepsScrollHitTestingAtTheVisibleRowBoundary(hasHeader: Bool) throws {
        let header = hasHeader ? AlternativeMenuHeader(height: 40, minimumWidth: 240) { EmptyView() } : nil
        let items: [AlternativeMenuItem] = (0..<80).map { index in .action("Row \(index)") { _ in } }
        let fixture = try ScrollFixture(items: items, header: header)
        #expect(fixture.view.scrollMenuItems(by: 100))

        let top = header?.height ?? 0
        let boundary = top + AlternativeMenuStyle.minimumRowHeight
        for y in [top + 1, boundary - 0.5] {
            let point = fixture.view.convert(NSPoint(x: 150, y: y), to: nil)
            #expect(fixture.view.edgeScrollDirection(at: point) == -1)
            #expect(fixture.view.rowIndex(at: point) == nil)
        }
        let visiblePoint = fixture.view.convert(NSPoint(x: 150, y: boundary), to: nil)
        #expect(fixture.view.edgeScrollDirection(at: visiblePoint) == 0)
        #expect(fixture.view.rowIndex(at: visiblePoint) != nil)
        if hasHeader {
            let headerPoint = fixture.view.convert(NSPoint(x: 150, y: top - 1), to: nil)
            #expect(fixture.view.edgeScrollDirection(at: headerPoint) == 0)
            #expect(fixture.view.rowIndex(at: headerPoint) == nil)
        }
    }

    private var tallItem: AlternativeMenuItem {
        .action("Tall", image: NSImage(size: NSSize(width: 16, height: 1_200))) { _ in }
    }

    @MainActor
    private struct ScrollFixture {
        let view: AlternativeMenuView
        private let scroll: NSScrollView

        var offset: CGFloat { scroll.documentVisibleRect.minY }
        var maximumOffset: CGFloat { view.preferredHeight - view.bounds.height }

        init(items: [AlternativeMenuItem], header: AlternativeMenuHeader? = nil) throws {
            let viewport = NSRect(x: 0, y: 0, width: 300, height: 500)
            self.view = AlternativeMenuView(header: header)
            view.frame = NSRect(origin: .zero, size: NSSize(width: viewport.width, height: viewport.height + view.viewportTopInset))
            view.setRows(items, selectedIndex: nil)

            self.scroll = NSScrollView(frame: viewport)
            var documentFrame = viewport
            documentFrame.size.height = view.preferredHeight - view.viewportTopInset
            scroll.documentView = FlippedDocument(frame: documentFrame)
            let state = try #require(Mirror(reflecting: view).descendant("state") as? AlternativeMenuViewState)
            state.scrollView = scroll
        }
    }

    private final class FlippedDocument: NSView {
        override var isFlipped: Bool { true }
    }
}

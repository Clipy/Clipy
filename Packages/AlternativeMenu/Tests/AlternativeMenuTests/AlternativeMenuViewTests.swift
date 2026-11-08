import AppKit
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

        #expect(fixture.view.scrollPage(down: true) == 38)
        #expect(fixture.view.scrollPage(down: true) == 56)
        #expect(fixture.view.scrollPage(down: false) == 20)
        #expect(fixture.view.scrollPage(down: false) == 2)
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

        init(items: [AlternativeMenuItem]) throws {
            let viewport = NSRect(x: 0, y: 0, width: 300, height: 500)
            view = AlternativeMenuView(header: nil)
            view.frame = NSRect(origin: .zero, size: NSSize(width: viewport.width, height: viewport.height + view.viewportTopInset))
            view.setRows(items, selectedIndex: nil)

            scroll = NSScrollView(frame: viewport)
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

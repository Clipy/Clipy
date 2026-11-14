import AppKit
import SwiftUI
import Testing

@testable import AlternativeMenu

@MainActor
struct AlternativeMenuPanelTests {
    @Test(arguments: [false, true])
    func keepsMenuPanelsAboveTheMenuBar(acceptsKeyboard: Bool) {
        _ = NSApplication.shared
        let panel = AlternativeMenuPanel(acceptsKeyboard: acceptsKeyboard)

        #expect(panel.level == .popUpMenu)
    }

    @Test
    func capsUpdatedWidthToItsScreen() throws {
        let panel = try makePanel(minimumWidth: 100_000)
        defer { panel.dismiss() }
        let screen = try #require(panel.screen)
        let inset = AlternativeMenuStyle.current.screenEdgeInset
        let limit = min(screen.frame.width * 0.6, screen.frame.width - inset * 2)

        panel.updateItems([.action("Item") { _ in }], highlightsFirstItem: false)

        #expect((floor(limit)...ceil(limit)).contains(panel.frame.width))
        #expect(panel.frame.minX >= screen.frame.minX + inset)
        #expect(panel.frame.maxX <= screen.frame.maxX - inset)
        #expect(panel.frame.minY >= screen.visibleFrame.minY + inset)
        #expect(panel.frame.maxY <= screen.frame.maxY - inset)
    }

    @Test
    func roundsUpdatedWidthUpToFitContent() throws {
        let panel = try makePanel(minimumWidth: 240.5)
        defer { panel.dismiss() }
        _ = try #require(panel.screen)

        panel.updateItems([.action("Item") { _ in }], highlightsFirstItem: false)

        #expect(panel.menuView.preferredWidth == 240.5)
        #expect(panel.frame.width == 241)
    }

    @Test(arguments: [false, true])
    func growsIntoTheMenuBarWithoutEnteringTheDockAndKeepsItsFrameAfterUpdating(hasHeader: Bool) throws {
        _ = NSApplication.shared
        let screen = try #require(NSScreen.main)
        let header = hasHeader ? AlternativeMenuHeader(height: 30, minimumWidth: 240) { EmptyView() } : nil
        let panel = AlternativeMenuPanel(header: header)
        defer { panel.dismiss() }
        let initialFrame = NSRect(x: screen.frame.midX, y: screen.frame.midY - 50, width: 240, height: 100)
        panel.setFrame(initialFrame, display: false)
        let items = (0..<500).map { AlternativeMenuItem.action("Item \($0)") { _ in } }
        panel.setItems(items, highlightsFirstItem: false)
        let grow = try #require(panel.menuView.growMenuHeightBy)

        let growth = grow(screen.frame.height)
        let expandedFrame = panel.frame
        let inset = AlternativeMenuStyle.current.screenEdgeInset
        #expect(expandedFrame.minY == screen.visibleFrame.minY + inset)
        #expect(expandedFrame.maxY == screen.frame.maxY - inset)
        #expect(growth == expandedFrame.height - initialFrame.height)
        #expect(grow(screen.frame.height) == 0)
        #expect(panel.constrainFrameRect(expandedFrame, to: screen) == expandedFrame)

        panel.updateItems(items, highlightsFirstItem: false)
        #expect(panel.frame == expandedFrame)
    }

    @Test(arguments: [CGFloat(-1), 0, 1, 4, 5, 6], [false, true])
    func offsetsOnlyAnchorsAtOrBelowTheVisibleScreenBottom(_ offset: CGFloat, hasHeader: Bool) throws {
        _ = NSApplication.shared
        let screen = try #require(NSScreen.main)
        let header = hasHeader ? AlternativeMenuHeader(height: 30, minimumWidth: 240) { EmptyView() } : nil
        let panel = AlternativeMenuPanel(header: header)
        defer { panel.dismiss() }
        let inset = AlternativeMenuStyle.current.screenEdgeInset
        let padding = offset <= 0 ? AlternativeMenuStyle.current.padding : 0
        let expectedBottom = screen.visibleFrame.minY + max(offset, inset + padding)
        panel.setItems([.action("First") { _ in }], highlightsFirstItem: false)

        panel.open(at: NSPoint(x: screen.frame.midX, y: screen.visibleFrame.minY + offset))

        #expect(panel.frame.minY == expectedBottom)
        panel.updateItems([.action("First") { _ in }, .action("Last") { _ in }], highlightsFirstItem: false)
        #expect(panel.frame.minY == expectedBottom)

        panel.dismiss()
        panel.setItems([.action("First") { _ in }], highlightsFirstItem: false)
        panel.open(at: NSPoint(x: screen.frame.midX, y: screen.visibleFrame.minY + inset))
        #expect(panel.frame.minY == screen.visibleFrame.minY + inset)
    }

    @Test(arguments: [false, true], [false, true])
    func cancellationCannotChangeTheNextPresentation(duringFade: Bool, blinksSelection: Bool) async {
        _ = NSApplication.shared
        let root = AlternativeMenuPanel()
        let child = AlternativeMenuPanel(acceptsKeyboard: false)
        let panels = [root, child]
        for panel in panels {
            panel.setItems([.action("Previous") { _ in }], highlightsFirstItem: true)
        }
        var completed = false
        let fadeStarted = AsyncStream<Void>.makeStream()
        var finishFade: CheckedContinuation<Void, Never>?
        let task = child.animateDismissal(
            fading: panels,
            blinksSelection: blinksSelection,
            fade: { fadingPanels in
                fadingPanels.forEach { $0.alphaValue = 0.5 }
                await withCheckedContinuation { continuation in
                    finishFade = continuation
                    fadeStarted.continuation.yield()
                }
            },
            completion: { completed = true }
        )
        defer {
            task.cancel()
            finishFade?.resume()
            fadeStarted.continuation.finish()
            panels.forEach { $0.dismiss() }
        }

        if duringFade {
            var starts = fadeStarted.stream.makeAsyncIterator()
            _ = await starts.next()
            #expect(child.alphaValue == 0.5)
            #expect(root.alphaValue == 0.5)
            #expect(!child.menuView.hidesSelection)
            #expect(child.selection == 0)
        }

        task.cancel()
        for panel in panels {
            panel.dismiss()
            panel.setItems([.action("Next") { _ in }], highlightsFirstItem: true)
        }
        // Deliver the old animation's completion only after the panels have been reused.
        finishFade?.resume()
        finishFade = nil
        await task.value

        #expect(!completed)
        for panel in panels {
            #expect(panel.alphaValue == 1)
            #expect(!panel.menuView.hidesSelection)
            #expect(panel.items.first?.title == "Next")
            #expect(panel.selection == 0)
        }
    }

    @Test(arguments: [CGFloat(0), 1, 4, 5, 6])
    func keepsClippedBottomPaddingThroughScrollingAndUpdatesThenResetsOnReopening(_ topClearance: CGFloat) throws {
        _ = NSApplication.shared
        let screen = try #require(NSScreen.main)
        let panel = AlternativeMenuPanel()
        defer { panel.dismiss() }
        let padding = AlternativeMenuStyle.current.padding
        let clippedPadding = max(0, padding - topClearance)
        let point = NSPoint(
            x: screen.frame.midX,
            y: screen.frame.maxY - AlternativeMenuStyle.current.screenEdgeInset - topClearance
        )
        var items = [AlternativeMenuItem.action("First") { _ in }, .action("Last") { _ in }]
        panel.setItems(items, highlightsFirstItem: false)
        panel.open(at: point)
        let initialFrame = panel.frame
        let state = try #require(Mirror(reflecting: panel.menuView).descendant("state") as? AlternativeMenuViewState)
        _ = try #require(state.scrollView)
        let grow = try #require(panel.menuView.growMenuHeightBy)

        #expect(initialFrame.height == panel.menuView.preferredHeight - clippedPadding)
        #expect(!panel.menuView.scrollMenuItems(by: padding * 2))
        #expect(state.offset == 0)
        #expect(grow(padding) == 0)
        #expect(panel.frame == initialFrame)

        items.append(.action("Added") { _ in })
        panel.updateItems(items, highlightsFirstItem: false)
        #expect(panel.frame.height == panel.menuView.preferredHeight - clippedPadding)
        #expect(panel.frame.maxY == initialFrame.maxY)
        #expect(state.offset == 0)

        panel.dismiss()
        panel.setItems(items, highlightsFirstItem: false)
        panel.open(at: NSPoint(x: screen.visibleFrame.midX, y: screen.visibleFrame.midY))
        #expect(panel.frame.height == panel.menuView.preferredHeight)
    }

    @Test
    func cancellationFadesTheMenuChainWithoutBlinking() async {
        _ = NSApplication.shared
        let panels = [AlternativeMenuPanel(), AlternativeMenuPanel(acceptsKeyboard: false)]
        for panel in panels {
            panel.setItems([.action("Item") { _ in }], highlightsFirstItem: true)
        }
        var completed = false
        let task = panels[0].animateDismissal(fading: panels, blinksSelection: false) { completed = true }
        defer {
            task.cancel()
            panels.forEach { $0.dismiss() }
        }
        await task.value
        #expect(completed)
        for panel in panels {
            #expect(panel.alphaValue == 0)
            #expect(!panel.menuView.hidesSelection)
            #expect(panel.selection == 0)
        }
    }

    private func makePanel(minimumWidth: CGFloat) throws -> AlternativeMenuPanel {
        _ = NSApplication.shared
        let screen = try #require(NSScreen.main)
        let header = AlternativeMenuHeader(height: 30, minimumWidth: minimumWidth) { EmptyView() }
        let panel = AlternativeMenuPanel(header: header)
        // Keep the panel hidden while associating it with a screen for updateItems.
        panel.setFrame(NSRect(x: screen.visibleFrame.midX, y: screen.visibleFrame.midY, width: 1, height: 1), display: false)
        return panel
    }
}

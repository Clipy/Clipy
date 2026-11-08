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
        let inset = AlternativeMenuStyle.screenEdgeInset
        let availableFrame = screen.frame.insetBy(dx: inset, dy: inset)
        let limit = min(screen.frame.width * 0.6, availableFrame.width)

        panel.updateItems([.action("Item") { _ in }], highlightsFirstItem: false)

        #expect((floor(limit)...ceil(limit)).contains(panel.frame.width))
        #expect(availableFrame.contains(panel.frame))
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
    func growsToPhysicalScreenEdgesAndKeepsItsFrameAfterUpdating(hasHeader: Bool) throws {
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
        let inset = AlternativeMenuStyle.screenEdgeInset
        #expect(expandedFrame.minY == screen.frame.minY + inset)
        #expect(expandedFrame.maxY == screen.frame.maxY - inset)
        #expect(growth == expandedFrame.height - initialFrame.height)
        #expect(grow(screen.frame.height) == 0)
        #expect(panel.constrainFrameRect(expandedFrame, to: screen) == expandedFrame)

        panel.updateItems(items, highlightsFirstItem: false)
        #expect(panel.frame == expandedFrame)
    }

    @Test(arguments: [false, true], [false, true])
    func cancellationCannotChangeTheNextPresentation(duringFade: Bool, blinksSelection: Bool) async throws {
        _ = NSApplication.shared
        let root = AlternativeMenuPanel()
        let child = AlternativeMenuPanel(acceptsKeyboard: false)
        let panels = [root, child]
        for panel in panels {
            panel.setItems([.action("Previous") { _ in }], highlightsFirstItem: true)
        }
        var completed = false
        let task = child.animateDismissal(fading: panels, blinksSelection: blinksSelection) { completed = true }
        defer {
            task.cancel()
            panels.forEach { $0.dismiss() }
        }

        if duringFade {
            let deadline = ContinuousClock.now + .seconds(2)
            while child.alphaValue == 1, ContinuousClock.now < deadline {
                try await Task.sleep(for: .milliseconds(10))
            }
            try #require(child.alphaValue < 1)
            #expect(root.alphaValue < 1)
            #expect(child.selection == 0)
        }

        task.cancel()
        for panel in panels {
            panel.dismiss()
            panel.setItems([.action("Next") { _ in }], highlightsFirstItem: true)
        }
        await task.value
        try await Task.sleep(for: .milliseconds(300))

        #expect(!completed)
        for panel in panels {
            #expect(panel.alphaValue == 1)
            #expect(!panel.menuView.hidesSelection)
            #expect(panel.items.first?.title == "Next")
            #expect(panel.selection == 0)
        }
    }

    @Test
    func cancellationFadesTheMenuChainWithoutBlinking() async throws {
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
        let deadline = ContinuousClock.now + .seconds(2)
        while !completed, ContinuousClock.now < deadline {
            for panel in panels {
                #expect(!panel.menuView.hidesSelection)
                #expect(panel.selection == 0)
            }
            try await Task.sleep(for: .milliseconds(10))
        }
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

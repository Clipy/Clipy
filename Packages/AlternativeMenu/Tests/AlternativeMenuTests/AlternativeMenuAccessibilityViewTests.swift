import AppKit
import Testing
@testable import AlternativeMenu

@MainActor
struct AlternativeMenuAccessibilityViewTests {
    @Test
    func exposesMenuRowsWithTheirLiveSelectionAndExpansion() throws {
        let view = AlternativeMenuView(header: nil)
        let menu = view.menuAccessibilityView
        let items: [AlternativeMenuItem] = [
            .action("Open", keyEquivalent: "o", toolTip: "Opens the item") { _ in },
            .separator,
            .action("Unavailable", isEnabled: false) { _ in },
            .submenu("Submenu", menu: .init(items: [.action("Child") { _ in }]))
        ]
        let matcher = AlternativeMenuKeyEquivalentMatcher()
        matcher.updateItems(items)
        view.setRows(items, selectedIndex: 0) { matcher.display(at: [$0]) }
        let rows = try #require(menu.accessibilityChildren() as? [NSAccessibilityElement])
        try #require(rows.count == 3)

        #expect(menu.accessibilityRole() == .menu)
        #expect(menu.accessibilityTitle() == NSAccessibility.Role.menu.description(with: nil))
        #expect(rows[0].accessibilityRole() == .menuItem)
        #expect(rows[0].accessibilityTitle() == "Open")
        #expect(rows[0].accessibilityHelp() == "Opens the item")
        #expect(rows[0].accessibilityValue() as? String == "⌘O")
        #expect(rows[0].accessibilityParent() as? AlternativeMenuAccessibilityView === menu)
        #expect(rows[0].isAccessibilitySelected())
        #expect(!rows[1].isAccessibilityEnabled())
        #expect(!rows[1].accessibilityPerformPress())
        #expect(!rows[2].isAccessibilityExpanded())

        view.setSelection(3)
        view.setExpandedRow(3)
        #expect(!rows[0].isAccessibilitySelected())
        #expect(rows[2].isAccessibilitySelected())
        #expect(rows[2].isAccessibilityExpanded())
        #expect(menu.accessibilitySelectedChildren()?.first as? NSAccessibilityElement === rows[2])

        view.hidesSelection = true
        #expect(view.selection == 3)
        #expect(rows[2].isAccessibilitySelected())
        #expect(menu.accessibilitySelectedChildren()?.first as? NSAccessibilityElement === rows[2])
    }

    @Test
    func performsActionsOnlyForTheCurrentPresentation() throws {
        let view = AlternativeMenuView(header: nil)
        let window = UnorderedWindow(
            contentRect: NSRect(x: 0, y: 0, width: 240, height: 100),
            styleMask: .borderless, backing: .buffered, defer: false
        )
        window.contentView = view
        let menu = view.menuAccessibilityView
        var pressed: [Int] = []
        var cancellations = 0
        view.onActivateRow = { pressed.append($0) }
        view.onCancelMenu = { cancellations += 1 }
        view.setRows([.action("Previous") { _ in }], selectedIndex: nil)
        let previous = try #require(menu.accessibilityChildren()?.first as? NSAccessibilityElement)
        #expect(!previous.accessibilityPerformPress())
        #expect(!menu.accessibilityPerformCancel())

        window.reportsVisible = true
        view.setRows([.action("Current") { _ in }], selectedIndex: nil)
        let current = try #require(menu.accessibilityChildren()?.first as? NSAccessibilityElement)
        #expect(previous.accessibilityFrame().isEmpty)
        #expect(!previous.accessibilityPerformPress())
        #expect(!previous.accessibilityPerformCancel())
        #expect(current.accessibilityFrame() == view.rowScreenFrame(0))
        #expect(current.accessibilityPerformPress())
        #expect(current.accessibilityPerformCancel())
        #expect(pressed == [0])
        #expect(cancellations == 1)
    }

    @Test
    func exposesThePreparedShortcutInTheInitialAccessibilityValue() throws {
        let view = AlternativeMenuView(header: nil)
        let item = AlternativeMenuItem.action("Shortcut", keyEquivalent: "[") { _ in }
        view.setRows([item], selectedIndex: nil) { _ in AlternativeMenuKeyEquivalent.display(for: "]", modifiers: .command) }

        let row = try #require(view.menuAccessibilityView.accessibilityChildren()?.first as? NSAccessibilityElement)
        #expect(row.accessibilityValue() as? String == "⌘]")
    }

    @Test(arguments: [false, true], [false, true])
    func exposesSubmenuEnabledStateAndOnlyPressesEnabledRows(isEnabled: Bool, hasChildren: Bool) throws {
        let children: [AlternativeMenuItem] = hasChildren ? [.action("Child") { _ in }] : []
        let view = AlternativeMenuView(header: nil)
        let window = UnorderedWindow(
            contentRect: NSRect(x: 0, y: 0, width: 240, height: 100),
            styleMask: .borderless, backing: .buffered, defer: false
        )
        window.contentView = view
        window.reportsVisible = true
        var pressed: [Int] = []
        view.onActivateRow = { pressed.append($0) }
        view.setRows([.submenu("Submenu", menu: .init(items: children), isEnabled: isEnabled)], selectedIndex: nil)
        let row = try #require(view.menuAccessibilityView.accessibilityChildren()?.first as? NSAccessibilityElement)

        #expect(row.isAccessibilityEnabled() == isEnabled)
        #expect(!row.isAccessibilityExpanded())
        #expect(row.isAccessibilitySelectorAllowed(#selector(NSAccessibilityElement.isAccessibilityExpanded)))
        #expect(row.accessibilityPerformPress() == isEnabled)
        #expect(pressed == (isEnabled ? [0] : []))
    }

    private final class UnorderedWindow: NSWindow {
        var reportsVisible = false
        override var isVisible: Bool { reportsVisible }
    }
}

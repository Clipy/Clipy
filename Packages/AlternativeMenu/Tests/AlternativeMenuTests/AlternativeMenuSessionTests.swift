import AppKit
import SwiftUI
import Testing

@testable import AlternativeMenu

@MainActor
@Suite(.serialized)
struct AlternativeMenuSessionTests {
    @Test(arguments: [false, true])
    func positionsLeadingSeparatorsWithoutMovingTheCustomHeader(hasHeader: Bool) throws {
        _ = NSApplication.shared
        let screen = try #require(NSScreen.main)
        let header = hasHeader ? AlternativeMenuHeader(height: 30, minimumWidth: 240) { EmptyView() } : nil
        let root = AlternativeMenuPanel(header: header)
        let session = AlternativeMenuSession(root: root)
        defer { session.dismiss() }
        let point = NSPoint(x: screen.visibleFrame.midX, y: screen.visibleFrame.midY)
        let item = AlternativeMenuItem.action("Item") { _ in }
        session.present(at: point, menu: .init(items: [item]))
        let originalFrame = root.frame

        session.present(at: point, menu: .init(items: [.separator, .separator, item, .separator]))

        #expect(root.frame == originalFrame.offsetBy(dx: 0, dy: hasHeader ? 0 : -AlternativeMenuStyle.current.padding))
    }

    @Test(arguments: [false, true])
    func treatsSeparatorOnlyMenusLikeEmptyMenusWhenPresentingAndUpdating(hasHeader: Bool) throws {
        _ = NSApplication.shared
        let screen = try #require(NSScreen.main)
        let header = hasHeader ? AlternativeMenuHeader(height: 30, minimumWidth: 240) { EmptyView() } : nil
        let root = AlternativeMenuPanel(header: header)
        var dismissals = 0
        let session = AlternativeMenuSession(root: root, onDismiss: { dismissals += 1 })
        defer { session.dismiss() }
        let point = NSPoint(x: screen.visibleFrame.midX, y: screen.visibleFrame.midY)
        let separators: [AlternativeMenuItem] = [.separator, .separator]

        session.present(at: point, menu: .init(items: separators))
        #expect(session.isVisible == hasHeader)
        #expect(dismissals == (hasHeader ? 0 : 1))
        session.dismiss()

        session.present(at: point, menu: .init(items: [.action("Item") { _ in }]))
        try #require(session.isVisible)
        dismissals = 0
        session.updateItems(separators)

        #expect(session.isVisible == hasHeader)
        #expect(dismissals == (hasHeader ? 0 : 1))
        #expect(root.selection == nil)
        #expect(root.menuView.menuAccessibilityView.accessibilityChildren()?.isEmpty == true)
    }

    @Test(arguments: [false, true], [false, true])
    func opensOnlySubmenusWithVisibleContents(hasChildItem: Bool, hasSeparators: Bool) throws {
        _ = NSApplication.shared
        let screen = try #require(NSScreen.main)
        let root = AlternativeMenuPanel()
        let session = AlternativeMenuSession(root: root)
        defer { session.dismiss() }
        var children: [AlternativeMenuItem] = hasChildItem ? [.action("Child") { _ in }] : []
        if hasSeparators {
            children = [.separator] + children + [.separator, .separator]
        }
        let items: [AlternativeMenuItem] = [
            .separator,
            .submenu("Parent", menu: .init(items: children)),
            .separator
        ]
        session.present(at: NSPoint(x: screen.visibleFrame.midX, y: screen.visibleFrame.midY), menu: .init(items: items))
        try #require(session.isVisible)
        let parent = try #require(root.menuView.menuAccessibilityView.accessibilityChildren()?.first as? NSAccessibilityElement)

        #expect(parent.accessibilityPerformPress())

        #expect(session.isVisible)
        #expect(root.selection == 1)
        #expect(root.menuView.expandedRow == (hasChildItem ? 1 : nil))
        #expect(parent.isAccessibilityExpanded() == hasChildItem)
        #expect(!root.menuView.isSelectionEmphasized)
    }

    @Test(arguments: [false, true])
    func restoresParentEmphasisAfterEnteringInvisibleSubmenus(hasSeparators: Bool) throws {
        _ = NSApplication.shared
        let screen = try #require(NSScreen.main)
        let root = AlternativeMenuPanel()
        let session = AlternativeMenuSession(root: root)
        defer { session.dismiss() }
        let children: [AlternativeMenuItem] = hasSeparators ? [.separator, .separator] : []
        session.present(
            at: NSPoint(x: screen.visibleFrame.midX, y: screen.visibleFrame.midY),
            highlightsFirstItem: true,
            menu: .init(items: [.submenu("Parent", menu: .init(items: children)), .action("Next") { _ in }])
        )
        try #require(root.isKeyWindow)

        for (characters, code, selection, isEmphasized) in [
            ("\u{f703}", UInt16(124), 0, false),
            ("\u{f702}", UInt16(123), 0, true),
            ("\u{f703}", UInt16(124), 0, false),
            ("\u{f701}", UInt16(125), 1, true)
        ] {
            let event = try #require(
                NSEvent.keyEvent(
                    with: .keyDown,
                    location: .zero,
                    modifierFlags: [],
                    timestamp: 0,
                    windowNumber: root.windowNumber,
                    context: nil,
                    characters: characters,
                    charactersIgnoringModifiers: characters,
                    isARepeat: false,
                    keyCode: code
                )
            )
            NSApp.sendEvent(event)

            #expect(session.isVisible)
            #expect(root.selection == selection)
            #expect(root.menuView.expandedRow == nil)
            #expect(root.menuView.isSelectionEmphasized == isEmphasized)
        }
    }
}

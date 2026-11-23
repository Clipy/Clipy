import AppKit
import SwiftUI
import Testing

@testable import AlternativeMenu

@MainActor
@Suite(.serialized)
struct AlternativeMenuSessionTests {
    @Test(arguments: [0, 1, 2])
    func retainsEnteredSubmenusWhenThePointerLeaves(enteredDepth: Int) throws {
        let (session, root) = try presentMenu([
            .submenu("Parent", menu: .init(items: [
                .submenu("Child", menu: .init(items: [.action("Grandchild") { _ in }]))
            ])),
            .action("Next") { _ in }
        ], highlightsFirstItem: true)
        defer { session.dismiss() }
        let parentItem = try #require(root.menuView.menuAccessibilityView.accessibilityChildren()?.first as? NSAccessibilityElement)
        #expect(parentItem.accessibilityPerformPress())
        let child = try #require(visibleSubmenu(of: root))
        func movePointer(to point: NSPoint) throws {
            try sendMouse(.mouseMoved, at: point, to: root)
        }
        let childRow = try #require(child.menuView.rowScreenFrame(0))
        if enteredDepth >= 1 {
            try movePointer(to: NSPoint(x: childRow.midX, y: childRow.midY))
        }
        child.selectRow(0)
        let childItem = try #require(child.menuView.menuAccessibilityView.accessibilityChildren()?.first as? NSAccessibilityElement)
        #expect(childItem.accessibilityPerformPress())
        let grandchild = try #require(visibleSubmenu(of: child))
        let grandchildRow = try #require(grandchild.menuView.rowScreenFrame(0))
        if enteredDepth == 2 {
            try movePointer(to: NSPoint(x: grandchildRow.midX, y: grandchildRow.midY))
        }

        let outside = NSPoint(x: root.frame.minX - 100, y: root.frame.minY - 100)
        for step in 0..<2 {
            try movePointer(to: NSPoint(x: outside.x - CGFloat(step), y: outside.y))

            #expect(root.isVisible)
            #expect(child.isVisible == (enteredDepth >= 1))
            #expect(grandchild.isVisible == (enteredDepth == 2))
            #expect(root.selection == (enteredDepth >= 1 ? 0 : nil))
            #expect(root.menuView.expandedRow == (enteredDepth >= 1 ? 0 : nil))
            if enteredDepth >= 1 {
                #expect(child.selection == (enteredDepth == 2 ? 0 : nil))
                #expect(child.menuView.expandedRow == (enteredDepth == 2 ? 0 : nil))
            }
            if enteredDepth == 2 {
                #expect(grandchild.selection == nil)
            }
        }

        let nextRow = try #require(root.menuView.rowScreenFrame(1))
        // Move away from the child within the row to finish diagonal-navigation grace.
        let approachOffset: CGFloat = child.frame.minX < root.frame.minX ? -1 : 1
        try movePointer(to: NSPoint(x: nextRow.midX + approachOffset, y: nextRow.midY))
        try movePointer(to: NSPoint(x: nextRow.midX, y: nextRow.midY))
        #expect(root.selection == 1)
        #expect(!child.isVisible)
        #expect(!grandchild.isVisible)

        #expect(parentItem.accessibilityPerformPress())
        try #require(root.menuView.expandedRow == 0)
        try movePointer(to: outside)
        #expect(root.menuView.expandedRow == nil)
    }

    @Test
    func quickClicksOpenOnlyTheLastHoveredSubmenu() async throws {
        let items: [AlternativeMenuItem] = ["First", "Second", "Third"].map { title in
            .submenu(title, menu: .init(items: [.action("\(title) child") { _ in }]))
        }
        let (session, root) = try presentMenu(items)
        defer { session.dismiss() }

        try hoverRow(0, in: root)
        try await waitForSubmenu(at: 0, in: root)
        let child = try #require(visibleSubmenu(of: root))
        #expect(child.items.first?.title == "First child")
        let originalItem = try #require(child.menuView.menuAccessibilityView.accessibilityChildren()?.first as? NSAccessibilityElement)

        for index in 1...2 {
            let row = try #require(root.menuView.rowScreenFrame(index))
            let point = NSPoint(x: row.midX, y: row.midY)
            for type in [NSEvent.EventType.mouseMoved, .leftMouseDown, .leftMouseUp] {
                try sendMouse(type, at: point, to: root)
            }
            #expect(root.selection == index)
            #expect(root.menuView.expandedRow == nil)
        }

        try await waitForSubmenu(at: 2, in: root)
        #expect(child.isVisible)
        #expect(child.items.first?.title == "Third child")
        #expect(child.selection == nil)
        #expect(!originalItem.accessibilityPerformPress())
    }

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
        var children: [AlternativeMenuItem] = hasChildItem ? [.action("Child") { _ in }] : []
        if hasSeparators {
            children = [.separator] + children + [.separator, .separator]
        }
        let items: [AlternativeMenuItem] = [
            .separator,
            .submenu("Parent", menu: .init(items: children)),
            .separator
        ]
        let (session, root) = try presentMenu(items)
        defer { session.dismiss() }
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
    func arrowNavigationPreviewsSubmenusAtTheCurrentLevel(highlightsFirstItem: Bool) async throws {
        let items: [AlternativeMenuItem] = [
            .submenu("First", menu: .init(items: [.action("First child") { _ in }])),
            .separator,
            .submenu("Disabled", menu: .init(items: []), isEnabled: false),
            .submenu("Second", menu: .init(items: [
                .submenu("Second child", menu: .init(items: [.action("First grandchild") { _ in }])),
                .submenu("Nested", menu: .init(items: [.action("Grandchild") { _ in }]))
            ])),
            .action("Last") { _ in }
        ]
        let (session, root) = try presentMenu(items, highlightsFirstItem: highlightsFirstItem)
        defer { session.dismiss() }
        try #require(root.isKeyWindow)
        if !highlightsFirstItem {
            #expect(root.selection == nil)
            #expect(root.menuView.expandedRow == nil)
            try sendKey(.downArrow, code: 125, to: root)
            let firstRow = try #require(root.menuView.rowScreenFrame(0))
            let point = NSPoint(x: firstRow.midX, y: firstRow.midY)
            try sendMouse(.leftMouseDown, at: point, to: root)
            try sendMouse(.leftMouseUp, at: point, to: root)
            try #require(root.menuView.expandedRow == nil)

            // Keep switching input on this row faster than the submenu delay.
            var offset: CGFloat = 1
            let deadline = ContinuousClock.now + .seconds(2)
            while root.menuView.expandedRow == nil, .now < deadline {
                try sendKey(.upArrow, code: 126, to: root)
                try sendMouse(.mouseMoved, at: NSPoint(x: point.x + offset, y: point.y), to: root)
                offset.negate()
                try await Task.sleep(for: .milliseconds(40))
            }
            try #require(root.menuView.expandedRow == 0)
        } else {
            try await waitForSubmenu(at: 0, in: root)
        }
        #expect(root.selection == 0)
        #expect(root.menuView.isSelectionEmphasized)

        try sendKey(.downArrow, code: 125, to: root)
        #expect(root.selection == 3)
        #expect(root.menuView.expandedRow == nil)
        try await waitForSubmenu(at: 3, in: root)
        let child = try #require(visibleSubmenu(of: root))
        #expect(child.items.first?.title == "Second child")
        #expect(child.selection == nil)
        #expect(child.menuView.expandedRow == nil)
        if highlightsFirstItem {
            try sendKey(.carriageReturn, code: 36, to: root)
        } else {
            try sendKey(root.isRightToLeft ? .leftArrow : .rightArrow, code: root.isRightToLeft ? 123 : 124, to: root)
        }
        #expect(child.selection == 0)
        try await waitForSubmenu(at: 0, in: child)
        #expect(child.menuView.isSelectionEmphasized)
        try sendKey(.downArrow, code: 125, to: root)
        #expect(child.selection == 1)
        try await waitForSubmenu(at: 1, in: child)
        try sendKey(.upArrow, code: 126, to: root)
        #expect(child.selection == 0)
        #expect(child.menuView.expandedRow == nil)

        try sendKey(root.isRightToLeft ? .rightArrow : .leftArrow, code: root.isRightToLeft ? 124 : 123, to: root)
        #expect(root.selection == 3)
        try sendKey(.downArrow, code: 125, to: root)
        #expect(root.selection == 4)
        #expect(root.menuView.expandedRow == nil)
        try sendKey(.upArrow, code: 126, to: root)
        #expect(root.selection == 3)
        try await waitForSubmenu(at: 3, in: root)
        try sendKey(.end, code: 119, to: root)
        #expect(root.selection == 4)
        #expect(root.menuView.expandedRow == nil)

        session.updateItems(items, highlightsFirstItem: false)
        #expect(root.menuView.expandedRow == nil)
        session.updateItems(items, highlightsFirstItem: true)
        try await waitForSubmenu(at: 0, in: root)
    }

    @Test(arguments: [false, true])
    func restoresParentEmphasisAfterEnteringInvisibleSubmenus(hasSeparators: Bool) throws {
        let children: [AlternativeMenuItem] = hasSeparators ? [.separator, .separator] : []
        let (session, root) = try presentMenu([.submenu("Parent", menu: .init(items: children)), .action("Next") { _ in }], highlightsFirstItem: true)
        defer { session.dismiss() }
        try #require(root.isKeyWindow)

        for (key, code, selection, isEmphasized) in [
            (NSEvent.SpecialKey.rightArrow, UInt16(124), 0, false),
            (.leftArrow, UInt16(123), 0, true),
            (.rightArrow, UInt16(124), 0, false),
            (.downArrow, UInt16(125), 1, true)
        ] {
            try sendKey(key, code: code, to: root)

            #expect(session.isVisible)
            #expect(root.selection == selection)
            #expect(root.menuView.expandedRow == nil)
            #expect(root.menuView.isSelectionEmphasized == isEmphasized)
        }
    }

    @Test
    func wheelEventsCancelPendingSubmenusOnlyWhenRowsMove() async throws {
        _ = NSApplication.shared
        let screen = try #require(NSScreen.main)
        let root = AlternativeMenuPanel()
        let session = AlternativeMenuSession(root: root)
        defer { session.dismiss() }
        let lineHeight = AlternativeMenuStyle.current.minimumRowHeight
        let count = Int(screen.frame.height / lineHeight) + 5
        var items = Array(repeating: AlternativeMenuItem.action("Item") { _ in }, count: count)
        items[3] = .submenu("Parent", menu: .init(items: [.action("Child") { _ in }]))
        let anchor = NSPoint(x: screen.frame.midX, y: screen.frame.maxY - AlternativeMenuStyle.current.screenEdgeInset)
        session.present(at: anchor, highlightsFirstItem: true, menu: .init(items: items))
        try #require(root.isKeyWindow)
        let row = try #require(root.menuView.rowScreenFrame(2))
        let point = root.convertPoint(fromScreen: NSPoint(x: row.midX, y: row.midY))
        // Start with a window-targeted event so the session's local monitor receives the wheel.
        let wheel = try #require(NSEvent.mouseEvent(
            with: .mouseMoved, location: point, modifierFlags: [], timestamp: 0,
            windowNumber: root.windowNumber, context: nil, eventNumber: 0, clickCount: 0, pressure: 0
        )?.cgEvent?.copy())
        wheel.type = .scrollWheel
        wheel.setIntegerValueField(.scrollWheelEventIsContinuous, value: 1)
        func scroll(_ delta: Int64, phase: CGScrollPhase) throws {
            wheel.setIntegerValueField(.scrollWheelEventPointDeltaAxis1, value: delta)
            wheel.setIntegerValueField(.scrollWheelEventScrollPhase, value: Int64(phase.rawValue))
            let event = try #require(NSEvent(cgEvent: wheel))
            try #require(event.window === root)
            NSApp.sendEvent(event)
        }

        for _ in 0..<3 {
            try sendKey(.downArrow, code: 125, to: root)
        }
        try #require(root.selection == 3)
        try scroll(1, phase: .changed)
        try scroll(0, phase: .ended)
        try await waitForSubmenu(at: 3, in: root)

        try sendKey(.upArrow, code: 126, to: root)
        try sendKey(.downArrow, code: 125, to: root)
        // A full line brings the same selected parent under the stationary pointer.
        try scroll(-Int64(lineHeight), phase: .began)
        try #require(root.menuView.rowScreenFrame(2) != row)
        try #require(root.selection == 3)
        try await Task.sleep(for: .milliseconds(400))
        #expect(root.menuView.expandedRow == nil)
    }

    @Test(arguments: [false, true])
    func startsANewTypingSequenceWhenEnteringAPreviewedSubmenu(previewsOnPresentation: Bool) async throws {
        let (session, root) = try presentMenu([
            .submenu("Apple", menu: .init(items: [.action("APB") { _ in }, .action("B") { _ in }])),
            .action("P") { _ in }
        ], highlightsFirstItem: previewsOnPresentation)
        defer { session.dismiss() }
        try #require(root.isKeyWindow)
        if previewsOnPresentation {
            try await waitForSubmenu(at: 0, in: root)
        }
        // Keep the typing sequence within the system's timeout regardless of test execution speed.
        try sendKey("a", code: 0, at: 10, to: root)
        if !previewsOnPresentation {
            let row = try #require(root.menuView.rowScreenFrame(0))
            let point = NSPoint(x: row.midX, y: row.midY)
            try sendMouse(.leftMouseDown, at: point, to: root)
            try sendMouse(.leftMouseUp, at: point, to: root)
        }
        try #require(root.menuView.expandedRow == 0)
        // With typing still in the parent, "ap" matches Apple.
        try sendKey("p", code: 35, at: 10.1, to: root)
        try #require(root.selection == 0)
        let child = try #require(visibleSubmenu(of: root))
        #expect(child.items.first?.title == "APB")
        try sendKey(.carriageReturn, code: 36, to: root)
        try #require(child.selection == 0)
        try sendKey("b", code: 11, at: 10.2, to: root)
        #expect(child.selection == 1)
    }

    @Test
    func closingArrowEditsTheHeaderWhenOnlyAPreviewIsOpen() async throws {
        let editor = NSTextView()
        let (session, root) = try presentWithFocusedHeader(editor, highlightsFirstItem: true, shouldHandleMenuKey: { _ in true })
        defer { session.dismiss() }
        try await waitForSubmenu(at: 0, in: root)
        editor.string = "Clip"
        editor.baseWritingDirection = .leftToRight
        editor.setSelectedRange(NSRange(location: 2, length: 0))

        try sendKey(root.isRightToLeft ? .rightArrow : .leftArrow, code: root.isRightToLeft ? 124 : 123, to: root)

        #expect(editor.selectedRange().location == (root.isRightToLeft ? 3 : 1))
        #expect(root.menuView.expandedRow == 0)
    }

    @Test(arguments: [false, true])
    func enteringAHoveredPreviewRespectsTheHeaderKeyPolicy(usesDefaultPolicy: Bool) async throws {
        let policy: ((NSEvent) -> Bool)? = usesDefaultPolicy ? nil : { _ in false }
        let editor = NSTextView()
        let (session, root) = try presentWithFocusedHeader(editor, highlightsFirstItem: false, shouldHandleMenuKey: policy)
        defer { session.dismiss() }
        try hoverRow(0, in: root)
        try await waitForSubmenu(at: 0, in: root)
        let child = try #require(visibleSubmenu(of: root))
        #expect(child.items.first?.title == "Header test child")

        try sendKey(root.isRightToLeft ? .leftArrow : .rightArrow, code: root.isRightToLeft ? 123 : 124, to: root)

        #expect(child.selection == (usesDefaultPolicy ? 0 : nil))
        #expect(root.firstResponder === editor)
    }

    @Test(arguments: [nil, false, true] as [Bool?])
    func textInputInTheHeaderPreservesThePendingPreview(handlesMenuKey: Bool?) async throws {
        let policy: ((NSEvent) -> Bool)? = handlesMenuKey.map { handlesKey in { _ in handlesKey } }
        let editor = NSTextView()
        let (session, root) = try presentWithFocusedHeader(editor, highlightsFirstItem: true, shouldHandleMenuKey: policy)
        defer { session.dismiss() }
        try #require(root.menuView.expandedRow == nil)

        try sendKey("x", code: 7, to: root)

        #expect(editor.string == "x")
        try await waitForSubmenu(at: 0, in: root)
        #expect(root.firstResponder === editor)
    }

    private func presentWithFocusedHeader(
        _ editor: NSTextView,
        highlightsFirstItem: Bool,
        shouldHandleMenuKey: ((NSEvent) -> Bool)?
    ) throws -> (AlternativeMenuSession, AlternativeMenuPanel) {
        _ = NSApplication.shared
        let screen = try #require(NSScreen.main)
        let header = AlternativeMenuHeader(height: 30, minimumWidth: 240) { HeaderEditor(view: editor) }
        let root = AlternativeMenuPanel(header: header)
        let session = AlternativeMenuSession(root: root, shouldHandleMenuKey: shouldHandleMenuKey)
        session.present(
            at: NSPoint(x: screen.visibleFrame.midX, y: screen.visibleFrame.midY),
            highlightsFirstItem: highlightsFirstItem,
            menu: .init(items: [.submenu("Folder", menu: .init(items: [.action("Header test child") { _ in }]))])
        )
        try #require(root.makeFirstResponder(editor))
        try #require(root.isHeaderControlFocused)
        return (session, root)
    }

    private struct HeaderEditor: NSViewRepresentable {
        let view: NSTextView

        func makeNSView(context: Context) -> NSTextView { view }
        func updateNSView(_ view: NSTextView, context: Context) {}
    }
}

private extension AlternativeMenuSessionTests {
    func presentMenu(
        _ items: [AlternativeMenuItem], highlightsFirstItem: Bool = false
    ) throws -> (AlternativeMenuSession, AlternativeMenuPanel) {
        _ = NSApplication.shared
        let screen = try #require(NSScreen.main)
        let root = AlternativeMenuPanel()
        let session = AlternativeMenuSession(root: root)
        session.present(
            at: NSPoint(x: screen.visibleFrame.midX, y: screen.visibleFrame.midY),
            highlightsFirstItem: highlightsFirstItem,
            menu: .init(items: items)
        )
        return (session, root)
    }

    func visibleSubmenu(of parent: AlternativeMenuPanel) -> AlternativeMenuPanel? {
        parent.childWindows?.compactMap { $0 as? AlternativeMenuPanel }.first(where: \.isVisible)
    }

    func hoverRow(_ index: Int, in panel: AlternativeMenuPanel) throws {
        let row = try #require(panel.menuView.rowScreenFrame(index))
        // The session ignores a mouse move that matches the initial pointer position.
        try sendMouse(.mouseMoved, at: NSPoint(x: row.midX, y: row.midY - 1), to: panel)
        try sendMouse(.mouseMoved, at: NSPoint(x: row.midX, y: row.midY), to: panel)
    }

    func sendMouse(_ type: NSEvent.EventType, at point: NSPoint, to panel: AlternativeMenuPanel) throws {
        let event = try #require(NSEvent.mouseEvent(
            with: type,
            location: panel.convertPoint(fromScreen: point),
            modifierFlags: [],
            timestamp: ProcessInfo.processInfo.systemUptime,
            windowNumber: panel.windowNumber, context: nil,
            eventNumber: 0,
            clickCount: type == .mouseMoved ? 0 : 1,
            pressure: 0
        ))
        NSApp.sendEvent(event)
    }

    func sendKey(_ key: NSEvent.SpecialKey, code: UInt16, to panel: AlternativeMenuPanel) throws {
        try sendKey(String(key.unicodeScalar), code: code, to: panel)
    }

    func sendKey(_ characters: String, code: UInt16, at timestamp: TimeInterval = 0, to panel: AlternativeMenuPanel) throws {
        let event = try #require(NSEvent.keyEvent(
            with: .keyDown,
            location: .zero, modifierFlags: [], timestamp: timestamp,
            windowNumber: panel.windowNumber, context: nil,
            characters: characters,
            charactersIgnoringModifiers: characters,
            isARepeat: false,
            keyCode: code
        ))
        NSApp.sendEvent(event)
    }

    func waitForSubmenu(at row: Int, in panel: AlternativeMenuPanel) async throws {
        let deadline = ContinuousClock.now + .seconds(2)
        while panel.menuView.expandedRow != row, .now < deadline {
            try await Task.sleep(for: .milliseconds(10))
        }
        try #require(panel.menuView.expandedRow == row)
    }
}

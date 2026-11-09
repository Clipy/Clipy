// swiftlint:disable file_length

//
//  AlternativeMenuSession.swift
//  AlternativeMenu
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit

/// Presents a reusable popup menu and coordinates its open submenu chain.
///
/// Create and retain a session on the main actor while the menu is in use.
/// Presentation returns without entering a blocking menu-tracking loop, allowing
/// controls in a custom header to use the application's normal input handling.
/// The session owns its windows, event monitors, and pending interaction tasks;
/// releasing it dismisses the current presentation.
///
/// Each session presents one root menu at a time. Calling
/// ``present(at:highlightsFirstItem:event:menu:)`` again replaces that presentation,
/// including its submenus. The caller supplies menu contents and enabled states;
/// ``updateItems(_:highlightsFirstItem:)`` replaces them while the popup is open.
///
/// Item actions run on the main actor after the menu closes and the `onDismiss`
/// callback returns. Explicit dismissal or reopening cancels an action that is
/// still waiting for its confirmation animation to finish.
@MainActor
public final class AlternativeMenuSession {
    /// One child in the open chain, ordered from the root's child to the deepest menu.
    private struct Submenu {
        let panel: AlternativeMenuPanel
        /// The row in the preceding panel that owns this child menu.
        let parentRow: Int
    }

    /// A delayed submenu opening or a grace period for moving diagonally into a child.
    private enum HoverTransition: Equatable {
        case open(AlternativeMenuPanel, Int)
        case aim
    }

    private let root: AlternativeMenuPanel
    private let keyEquivalentMatcher = AlternativeMenuKeyEquivalentMatcher()
    private let typingSelection = AlternativeMenuTypingSelection()
    private let passesScrollWheelToHeader: Bool
    private let shouldHandleMenuKey: ((NSEvent) -> Bool)?
    private let onDismiss: (() -> Void)?
    private var submenus: [Submenu] = []
    /// Marks an active presentation, including setup before the root is visible.
    private var isPresented = false
    private var eventMonitors: [Any] = []
    private var lastPointer = NSPoint.zero
    private var hoveredPanel: AlternativeMenuPanel?
    /// Tracks a press that began on a menu row or was passed to `present`.
    private var trackingRowPress = false
    /// Keeps a mouse-down-opened menu visible when that press ends without dragging.
    private var isOpeningMousePress = false
    private var pendingHover: (transition: HoverTransition, task: Task<Void, Never>)?
    private var edgeScrollTask: Task<Void, Never>?
    /// Blocks menu interaction during a fade and allows reopening to cancel pending actions.
    private var dismissalTask: Task<Void, Never>?

    /// Whether AppKit considers the root popup window visible.
    ///
    /// This remains `true` during confirmation and dismissal animations, until the
    /// window is hidden. It does not indicate whether menu input or item updates
    /// are currently accepted.
    public var isVisible: Bool { root.isVisible }

    /// Creates a session without a custom header. Creating it does not show a menu.
    ///
    /// - Parameters:
    ///   - minimumSymbolColumnWidth: Minimum width, in points, reserved for SF Symbol
    ///     columns in the root menu and its submenus. Must be finite and nonnegative.
    ///     Zero sizes columns to their contents; a larger value can keep icons and
    ///     titles aligned across item updates.
    ///   - onDismiss: A callback retained by the session and called synchronously on
    ///     the main actor after a presentation's windows are hidden and event monitors removed.
    ///     It runs for activation, cancellation, replacement, and a presentation that
    ///     cannot be shown. For activation, it runs before the item's action.
    ///     Calling `dismiss()` on an inactive session does not call it again.
    public convenience init(minimumSymbolColumnWidth: CGFloat = 0, onDismiss: (() -> Void)? = nil) {
        self.init(root: AlternativeMenuPanel(minimumSymbolColumnWidth: minimumSymbolColumnWidth), onDismiss: onDismiss)
    }

    /// Connects a root panel and its optional header input policy to the shared lifecycle.
    init(
        root: AlternativeMenuPanel,
        passesScrollWheelToHeader: Bool = true,
        shouldHandleMenuKey: ((NSEvent) -> Bool)? = nil,
        onDismiss: (() -> Void)? = nil
    ) {
        self.root = root
        self.passesScrollWheelToHeader = passesScrollWheelToHeader
        self.shouldHandleMenuKey = shouldHandleMenuKey
        self.onDismiss = onDismiss
        root.onResignKey = { [weak self] in self?.dismiss() }
        configureAccessibility(for: root)
    }

    isolated deinit {
        dismiss()
    }

    // MARK: - Presentation

    /// Shows a menu near a screen position and returns without waiting for a selection.
    ///
    /// Any existing presentation is dismissed first, including its pending action,
    /// and its `onDismiss` callback runs. If that callback leaves another menu open
    /// on this session, that presentation takes precedence and this call stops.
    ///
    /// An empty or separator-only menu is shown only with a custom header.
    /// Otherwise, or if no screen is available, the presentation ends and calls
    /// `onDismiss` even though no popup became visible.
    ///
    /// - Parameters:
    ///   - point: An anchor in AppKit's global screen coordinates, in points.
    ///     Defaults to the current pointer location. Placement is adjusted to fit
    ///     the screen and may open above the anchor when space below is limited.
    ///   - highlightsFirstItem: Whether to select and reveal the first enabled
    ///     action or submenu parent. Defaults to no initial selection; headers,
    ///     separators, and disabled items are never selected.
    ///   - event: The initiating left-mouse-down event, when opening from a press.
    ///     Passing it lets the user drag into a row and release to activate it.
    ///     Releasing without dragging keeps the menu open. Other event types do
    ///     not start this press-and-drag interaction.
    ///   - menu: The root contents, including actions and nested submenu contents.
    public func present(
        at point: NSPoint = NSEvent.mouseLocation,
        highlightsFirstItem: Bool = false,
        event: NSEvent? = nil,
        menu: AlternativeMenu
    ) {
        dismiss()
        // The dismissal callback may have opened another menu on this session.
        guard !isPresented else {
            return
        }
        isPresented = true
        lastPointer = NSEvent.mouseLocation
        keyEquivalentMatcher.updateItems(menu.items)
        root.setItems(menu.items, highlightsFirstItem: highlightsFirstItem) { keyEquivalentMatcher.display(at: [$0]) }
        root.open(at: point)
        guard root.isVisible else {
            dismiss()
            return
        }
        trackingRowPress = event?.type == .leftMouseDown
        isOpeningMousePress = trackingRowPress
        startMonitoring()
    }

    /// Replaces the current root contents, selection, and shortcut mappings.
    ///
    /// Closes all submenus and resets scrolling, typing selection, and pending
    /// pointer interactions. Previous item identity and selection are not preserved.
    /// The custom header, if any, is retained. The popup can widen to fit new items
    /// but does not shrink in width during a presentation, subject to screen bounds.
    ///
    /// Passing an empty array or only separators dismisses a menu without a header;
    /// a menu with a header remains open. Calls while inactive or during a
    /// confirmation/dismissal animation are ignored, not queued for later.
    ///
    /// - Parameters:
    ///   - items: The complete replacement contents. The caller supplies each item's
    ///     enabled state; the session does not perform responder-chain validation.
    ///   - highlightsFirstItem: Whether to select and reveal the first enabled action
    ///     or submenu parent. When `false`, selection is cleared and scrolling resets
    ///     to the top. When no item is enabled, nothing is selected.
    public func updateItems(_ items: [AlternativeMenuItem], highlightsFirstItem: Bool = false) {
        guard isPresented, dismissalTask == nil else {
            return
        }
        guard root.menuView.hasHeader || items.contains(where: { !$0.isSeparator }) else {
            dismiss()
            return
        }
        stopPointerTimers()
        typingSelection.reset()
        trackingRowPress = false
        isOpeningMousePress = false
        closeSubmenus(after: root)
        keyEquivalentMatcher.updateItems(items)
        root.updateItems(items, highlightsFirstItem: highlightsFirstItem) { keyEquivalentMatcher.display(at: [$0]) }
    }

    /// Ends the current presentation immediately, without a dismissal animation.
    ///
    /// Cancels pending hover, scrolling, and confirmation tasks, removes event
    /// monitors, clears menu contents, and hides the root and all child windows.
    /// An action still waiting for its confirmation animation is not performed.
    ///
    /// Calls `onDismiss` synchronously after cleanup. That callback may present a
    /// new menu on this session, so the session is not necessarily hidden when this
    /// method returns. Calling this method while inactive has no effect.
    public func dismiss() {
        guard isPresented else {
            return
        }
        isPresented = false
        dismissalTask?.cancel()
        dismissalTask = nil
        eventMonitors.forEach(NSEvent.removeMonitor)
        eventMonitors.removeAll()
        stopPointerTimers()
        typingSelection.reset()
        trackingRowPress = false
        isOpeningMousePress = false
        hoveredPanel = nil
        closeSubmenus(after: root)
        keyEquivalentMatcher.updateItems([])
        root.dismiss()
        onDismiss?()
    }
}

// MARK: - Submenus and Activation

private extension AlternativeMenuSession {
    /// Routes accessibility activation and cancellation through the normal interaction paths.
    func configureAccessibility(for panel: AlternativeMenuPanel) {
        panel.menuView.onCancelMenu = { [weak self] in
            self?.animateDismissal()
        }
        panel.menuView.onActivateRow = { [weak self, weak panel] index in
            guard let self, let panel else {
                return
            }
            stopPointerTimers()
            activateRow(index, in: panel, modifiers: NSEvent.modifierFlags, fromKeyboard: true)
        }
    }

    /// Returns the chain index immediately after a parent; the root's first child is zero.
    func childIndex(after panel: AlternativeMenuPanel) -> Int {
        if panel === root {
            return 0
        }
        return submenus.firstIndex { $0.panel === panel }.map { $0 + 1 } ?? submenus.count
    }

    private func child(of panel: AlternativeMenuPanel) -> Submenu? {
        submenus.dropFirst(childIndex(after: panel)).first
    }

    /// Keeps ancestor selections visible, emphasizing only the menu receiving input.
    func emphasizeSelection(in activePanel: AlternativeMenuPanel) {
        for panel in [root] + submenus.map(\.panel) {
            panel.menuView.isSelectionEmphasized = panel === activePanel
        }
    }

    /// Closes descendants deepest first, keeping the parent visible and clearing its expansion state.
    func closeSubmenus(after panel: AlternativeMenuPanel) {
        let index = childIndex(after: panel)
        if index < submenus.count {
            typingSelection.reset()
        }
        while submenus.count > index {
            let child = submenus.removeLast().panel
            if hoveredPanel === child {
                hoveredPanel = nil
                stopEdgeScrolling()
            }
            child.dismiss()
        }
        panel.menuView.setExpandedRow(nil)
        emphasizeSelection(in: panel)
    }

    /// Opens an enabled parent's nonempty submenu, or enters an already open child.
    ///
    /// Keyboard entry can select the first item in a child previously opened by hover.
    /// Children do not take keyboard focus from the root; this session routes keys to
    /// the deepest open menu while the root retains header editing and IME state.
    func openSubmenu(of panel: AlternativeMenuPanel, row: Int, highlightsFirstItem: Bool) {
        guard panel.items.indices.contains(row), panel.items[row].isEnabled,
            let menu = panel.items[row].submenu, let rowFrame = panel.menuView.rowScreenFrame(row)
        else {
            return
        }
        if let child = child(of: panel), child.parentRow == row {
            if highlightsFirstItem, child.panel.selection == nil {
                child.panel.moveSelection(by: 1)
            }
            emphasizeSelection(in: highlightsFirstItem ? child.panel : panel)
            return
        }
        closeSubmenus(after: panel)
        typingSelection.reset()
        guard menu.items.contains(where: { !$0.isSeparator }) else {
            return
        }
        let childPanel = AlternativeMenuPanel(minimumSymbolColumnWidth: panel.minimumSymbolColumnWidth, acceptsKeyboard: false)
        let path = submenus.map(\.parentRow) + [row]
        childPanel.setItems(menu.items, highlightsFirstItem: highlightsFirstItem) { keyEquivalentMatcher.display(at: path + [$0]) }
        configureAccessibility(for: childPanel)
        childPanel.openSubmenu(beside: panel, rowFrame: rowFrame, prefersLeft: root.isRightToLeft)
        guard childPanel.isVisible else {
            return
        }
        panel.toolTip.hide()
        panel.menuView.setExpandedRow(row)
        submenus.append(Submenu(panel: childPanel, parentRow: row))
        emphasizeSelection(in: highlightsFirstItem ? childPanel : panel)
    }

    /// Confirms an action or enters a submenu for pointer, keyboard, and accessibility input.
    /// Activating an empty parent dismisses by pointer but leaves the menu open from the keyboard.
    func activateRow(_ index: Int, in panel: AlternativeMenuPanel, modifiers: NSEvent.ModifierFlags, fromKeyboard: Bool) {
        guard isPresented, dismissalTask == nil, panel.items.indices.contains(index), panel.items[index].isEnabled else {
            return
        }
        switch panel.items[index].kind {
        case .submenu(let menu, _):
            // Unlike an empty submenu, a separator-only submenu keeps its parent open on click.
            if menu.items.isEmpty, !fromKeyboard {
                dismiss()
            } else {
                panel.selectRow(index, scroll: fromKeyboard)
                openSubmenu(of: panel, row: index, highlightsFirstItem: fromKeyboard)
            }
        case .action(_, let perform):
            confirm(index, in: panel, modifiers: modifiers, perform: perform)
        case .sectionHeader, .separator:
            break
        }
    }

    /// Reveals the chosen row and captures its action and modifiers until confirmation finishes.
    func confirm(
        _ index: Int,
        in panel: AlternativeMenuPanel,
        modifiers: NSEvent.ModifierFlags,
        perform: @escaping @MainActor (NSEvent.ModifierFlags) -> Void
    ) {
        panel.selectRow(index, scroll: true)
        emphasizeSelection(in: panel)
        animateDismissal(confirming: panel) { perform(modifiers) }
    }

    /// Fades the whole chain, blinking the selection first only for action confirmation.
    /// Completion runs after `dismiss()` and its callback. External dismissal before
    /// the animation finishes cancels the pending completion.
    func animateDismissal(confirming panel: AlternativeMenuPanel? = nil, completion: @escaping @MainActor () -> Void = {}) {
        guard isPresented, dismissalTask == nil else {
            return
        }
        stopPointerTimers()
        let panels = [root] + submenus.map(\.panel)
        panels.forEach { $0.toolTip.hide() }
        dismissalTask = (panel ?? root).animateDismissal(fading: panels, blinksSelection: panel != nil) { [weak self] in
            guard let self else {
                return
            }
            // Release keyboard focus before pasting or presenting another menu.
            dismiss()
            completion()
        }
    }

    // MARK: - Keyboard

    /// Routes keys to the deepest open menu while respecting header editing and IME composition.
    ///
    /// Marked text takes precedence over all menu keys, including Escape. Otherwise,
    /// Escape and Command-period dismiss before consulting the header's key policy.
    /// That policy may opt focused header controls into menu navigation and shortcuts;
    /// typing selection remains disabled whenever a header control has focus.
    ///
    /// - Returns: `true` when the local event monitor should consume the event.
    func handleKey(_ event: NSEvent) -> Bool {
        guard dismissalTask == nil else {
            return true
        }
        hoveredPanel?.toolTip.hide()
        if root.isHeaderControlFocused || root.hasMarkedText {
            typingSelection.reset()
        }
        // Leave candidate navigation and cancellation to the input method while composing.
        guard !root.hasMarkedText else {
            return false
        }
        stopPointerTimers()
        let modifiers = event.modifierFlags.intersection([.command, .option, .control, .shift])
        let key = event.charactersIgnoringModifiers
        if key == "\u{1b}" || (key == "." && modifiers == .command) {
            animateDismissal()
            return true
        }
        let handlesMenuKey = shouldHandleMenuKey?(event)
        let panel = submenus.last?.panel ?? root
        let headerIsFocused = panel === root && root.isHeaderControlFocused
        guard handlesMenuKey ?? !headerIsFocused else {
            return false
        }
        let handlesKeyEquivalent = handlesMenuKey ?? !root.isHeaderControlFocused
        if !modifiers.isEmpty, handlesKeyEquivalent, handleKeyEquivalent(event) {
            return true
        }
        let activatesWithSpace = key == " " && modifiers.isEmpty && !headerIsFocused
        if key == "\r" || key == "\u{3}" || activatesWithSpace {
            if let selection = panel.selection {
                activateRow(selection, in: panel, modifiers: modifiers, fromKeyboard: true)
            } else if panel !== root {
                dismiss()
            } else {
                NSSound.beep()
            }
            return true
        }
        guard modifiers.isEmpty else {
            return selectByTyping(event, in: panel)
        }
        let opens = root.isRightToLeft ? NSEvent.SpecialKey.leftArrow : .rightArrow
        let closes = root.isRightToLeft ? NSEvent.SpecialKey.rightArrow : .leftArrow
        switch event.specialKey {
        case opens:
            // Enter a submenu that was opened by hovering over its parent.
            if panel !== root, panel.selection == nil {
                panel.moveSelection(by: 1)
                emphasizeSelection(in: panel)
                return true
            }
            guard let selection = panel.selection, panel.items[selection].submenu != nil else {
                return false
            }
            openSubmenu(of: panel, row: selection, highlightsFirstItem: true)
        case closes:
            guard panel !== root else {
                return false
            }
            closeSubmenus(after: submenus.dropLast().last?.panel ?? root)
        case .downArrow:
            panel.moveSelection(by: 1)
        case .upArrow:
            panel.moveSelection(by: -1)
        case .pageUp:
            panel.selectRow(panel.menuView.scrollPage(down: false))
        case .pageDown:
            panel.selectRow(panel.menuView.scrollPage(down: true))
        case .home:
            panel.menuView.resetScrollPosition()
            panel.selectRow(panel.items.firstIndex(where: \.isEnabled), scroll: true)
        case .end:
            let selection = panel.items.lastIndex(where: \.isEnabled)
            panel.selectRow(selection)
            if let selection {
                panel.menuView.scrollRowToVisible(selection, alignsTallRowAtBottom: true)
            }
        default:
            if handlesKeyEquivalent, handleKeyEquivalent(event) {
                return true
            }
            return selectByTyping(event, in: panel)
        }
        emphasizeSelection(in: submenus.last?.panel ?? root)
        typingSelection.reset()
        return true
    }

    /// Applies native title-prefix selection only when no header control has focus.
    func selectByTyping(_ event: NSEvent, in panel: AlternativeMenuPanel) -> Bool {
        guard !root.isHeaderControlFocused else {
            return false
        }
        let result = typingSelection.select(event, in: panel.items)
        if let index = result.index {
            panel.selectRow(index, scroll: true)
            emphasizeSelection(in: panel)
        }
        return result.handled
    }

    /// Matches shortcuts across the entire menu, including unopened submenus.
    /// Matches in open menus use confirmation effects; other matches dismiss and run directly.
    /// A handled native match consumes the event even if it produces no action.
    func handleKeyEquivalent(_ event: NSEvent) -> Bool {
        let result = keyEquivalentMatcher.match(event)
        guard result.handled else {
            return false
        }
        if let item = result.item, let path = result.path, case .action(_, let perform) = item.kind {
            let depth = path.count - 1
            if depth <= submenus.count, path.dropLast().elementsEqual(submenus.prefix(depth).map(\.parentRow)) {
                confirm(path[depth], in: depth == 0 ? root : submenus[depth - 1].panel, modifiers: event.modifierFlags, perform: perform)
                return true
            }
            // Native shortcuts to unopened submenus dismiss without confirmation effects.
            dismiss()
            perform(event.modifierFlags)
        }
        return true
    }
}

// MARK: - Event Monitoring

private extension AlternativeMenuSession {
    /// Installs one local and one global monitor for the whole open chain.
    ///
    /// Only local keys received while the root is key are routed to the menu.
    /// Global monitoring observes pointer events, allowing outside clicks and
    /// drag releases to be tracked without intercepting other applications' keys.
    /// Wheel events over the header may continue to its controls according to policy.
    func startMonitoring() {
        let mouseDown: NSEvent.EventTypeMask = [.leftMouseDown, .rightMouseDown, .otherMouseDown]
        let pointerEvents: NSEvent.EventTypeMask = [mouseDown, .mouseMoved, .leftMouseDragged, .leftMouseUp]
        let local = NSEvent.addLocalMonitorForEvents(matching: [pointerEvents, .keyDown, .scrollWheel]) { [weak self] event in
            guard let self, isPresented else {
                return event
            }
            if dismissalTask != nil, let panel = event.window as? AlternativeMenuPanel, contains(panel) {
                return nil
            }
            switch event.type {
            case .keyDown:
                return root.isKeyWindow && handleKey(event) ? nil : event
            case .scrollWheel:
                guard let panel = event.window as? AlternativeMenuPanel, contains(panel) else {
                    return event
                }
                if passesScrollWheelToHeader, panel.menuView.isInHeader(at: event.locationInWindow) {
                    return event
                }
                stopPointerTimers()
                if panel.scrollWheelByEvent(event) {
                    closeSubmenus(after: panel)
                }
                return nil
            default:
                return handleMouse(event, isLocal: true) ? nil : event
            }
        }
        let global = NSEvent.addGlobalMonitorForEvents(matching: pointerEvents) { [weak self] event in
            guard let self, isPresented else {
                return
            }
            _ = handleMouse(event, isLocal: false)
        }
        eventMonitors.append(contentsOf: [local, global].compactMap { $0 })
    }

    func contains(_ panel: AlternativeMenuPanel) -> Bool {
        panel === root || submenus.contains { $0.panel === panel }
    }

    // MARK: - Pointer Tracking

    /// Hit-tests in screen coordinates, preferring the deepest child where windows overlap.
    func panel(at point: NSPoint) -> AlternativeMenuPanel? {
        submenus.last { $0.panel.frame.contains(point) }?.panel ?? (root.frame.contains(point) ? root : nil)
    }

    /// Tracks hovering and press-drag-release activation across all menu windows.
    ///
    /// Outside clicks dismiss and continue to their original destination. A global
    /// click during IME composition is left alone because it may target a candidate
    /// window owned by another process. The opening mouse-up alone does not activate a row.
    ///
    /// - Returns: Whether to consume the event when called by the local monitor.
    func handleMouse(_ event: NSEvent, isLocal: Bool) -> Bool {
        guard dismissalTask == nil else {
            return false
        }
        let point = event.window?.convertPoint(toScreen: event.locationInWindow) ?? event.locationInWindow
        let panel = panel(at: point)
        switch event.type {
        case .mouseMoved, .leftMouseDragged:
            if point != lastPointer {
                typingSelection.reset()
                if event.type == .leftMouseDragged {
                    isOpeningMousePress = false
                }
                let previous = lastPointer
                lastPointer = point
                updateHover(in: panel, at: point, previous: previous, showsToolTip: event.type == .mouseMoved)
            }
            return trackingRowPress && event.type == .leftMouseDragged
        case .leftMouseDown, .rightMouseDown, .otherMouseDown:
            isOpeningMousePress = false
            stopPointerTimers()
            hoveredPanel?.toolTip.hide()
            guard isLocal, let window = event.window as? AlternativeMenuPanel, contains(window), let panel else {
                // The input method's candidate window can be owned by another process.
                if isLocal || !root.hasMarkedText {
                    dismiss()
                }
                return false
            }
            guard event.type == .leftMouseDown else {
                return false
            }
            if panel.edgeScrollDirection(at: point) != 0 {
                lastPointer = point
                updateHover(in: panel, at: point, previous: nil, showsToolTip: false)
                return true
            }
            guard let index = panel.rowIndex(at: point) else {
                return false
            }
            selectHoveredRow(index, in: panel)
            trackingRowPress = true
            return true
        case .leftMouseUp:
            guard trackingRowPress else {
                return false
            }
            trackingRowPress = false
            if isOpeningMousePress {
                isOpeningMousePress = false
                return true
            }
            if let panel, let index = panel.rowIndex(at: point) {
                activateRow(index, in: panel, modifiers: event.modifierFlags, fromKeyboard: false)
            } else if panel == nil {
                dismiss()
            }
            return true
        default:
            return false
        }
    }

    func selectHoveredRow(_ index: Int?, in panel: AlternativeMenuPanel) {
        if let child = child(of: panel) {
            if child.parentRow != index {
                closeSubmenus(after: panel)
            } else {
                // Returning to a parent keeps its child open without a selected child row.
                if self.child(of: child.panel) != nil {
                    closeSubmenus(after: child.panel)
                }
                child.panel.selectRow(nil)
            }
        }
        panel.selectRow(index)
        emphasizeSelection(in: panel)
    }

    /// Updates selection, submenu delays, tooltips, and edge scrolling from actual pointer movement.
    /// Moving toward an open child temporarily preserves its parent selection; a nil
    /// previous point applies the current hit immediately after that grace period ends.
    func updateHover(in panel: AlternativeMenuPanel?, at point: NSPoint, previous: NSPoint?, showsToolTip: Bool) {
        let index = panel?.rowIndex(at: point)
        if let previous, let parent = panel ?? hoveredPanel, let child = child(of: parent), child.parentRow != index,
            AlternativeMenuSubmenuGeometry.isMovingTowardSubmenu(
                from: previous,
                to: point,
                submenuFrame: child.panel.frame,
                opensLeft: child.panel.frame.minX < parent.frame.minX
            ) {
            scheduleHover(.aim, after: .milliseconds(500))
            return
        }

        if hoveredPanel !== panel {
            hoveredPanel?.toolTip.hide()
            stopEdgeScrolling()
            hoveredPanel = panel
        }
        guard let panel else {
            cancelHover()
            closeSubmenus(after: root)
            root.selectRow(nil)
            return
        }

        selectHoveredRow(index, in: panel)
        if let index, panel.selection == index, panel.items[index].submenu != nil, child(of: panel) == nil {
            scheduleHover(.open(panel, index), after: .milliseconds(200))
        } else {
            cancelHover()
        }
        let overScrollEdge = panel.edgeScrollDirection(at: point) != 0
        if showsToolTip && !overScrollEdge {
            panel.updateToolTip(for: index)
        } else {
            panel.toolTip.hide()
        }
        if overScrollEdge {
            startEdgeScrolling(in: panel)
        } else {
            stopEdgeScrolling()
        }
    }

    /// Keeps one pending hover transition without restarting its delay for the same target.
    /// Revalidates the pointer location before opening a child or applying deferred selection.
    private func scheduleHover(_ transition: HoverTransition, after delay: Duration) {
        guard pendingHover?.transition != transition else {
            return
        }
        cancelHover()
        let task = Task { [weak self] in
            try? await Task.sleep(for: delay)
            guard !Task.isCancelled, let self, isPresented else {
                return
            }
            pendingHover = nil
            switch transition {
            case let .open(panel, row):
                guard self.panel(at: lastPointer) === panel, panel.rowIndex(at: lastPointer) == row else {
                    return
                }
                openSubmenu(of: panel, row: row, highlightsFirstItem: false)
            case .aim:
                updateHover(in: panel(at: lastPointer), at: lastPointer, previous: nil, showsToolTip: !trackingRowPress)
            }
        }
        pendingHover = (transition, task)
    }

    func cancelHover() {
        pendingHover?.task.cancel()
        pendingHover = nil
    }

    func stopPointerTimers() {
        cancelHover()
        stopEdgeScrolling()
    }

    /// Scrolls while the last tracked pointer position remains over a scroll arrow.
    /// Each tick recomputes availability because scrolling can also grow the popup.
    func startEdgeScrolling(in panel: AlternativeMenuPanel) {
        guard edgeScrollTask == nil else {
            return
        }
        edgeScrollTask = Task { [weak self, weak panel] in
            while !Task.isCancelled, let self, let panel {
                let direction = panel.edgeScrollDirection(at: lastPointer)
                guard direction != 0 else {
                    edgeScrollTask = nil
                    selectHoveredRow(panel.rowIndex(at: lastPointer), in: panel)
                    return
                }
                // NSMenu advances one standard menu line every 60 ms while hovering an arrow.
                panel.menuView.scrollMenuItems(by: direction * AlternativeMenuStyle.current.minimumRowHeight)
                try? await Task.sleep(for: .milliseconds(60))
            }
        }
    }

    func stopEdgeScrolling() {
        edgeScrollTask?.cancel()
        edgeScrollTask = nil
    }
}

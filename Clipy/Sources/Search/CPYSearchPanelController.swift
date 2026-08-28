//
//  CPYSearchPanelController.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/08/25.
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import Dependencies

final class CPYSearchPanelController: NSWindowController {

    // MARK: - Singleton
    static let sharedController = CPYSearchPanelController()

    // MARK: - Constants
    private let panelWidth: CGFloat = 680
    private let searchHeaderHeight: CGFloat = 50
    private let rowHeight: CGFloat = 46
    private let emptyContentHeight: CGFloat = 80
    private let padding: CGFloat = 8

    // MARK: - Properties
    private let searchPanel: CPYSearchPanel
    private let searchViewController: CPYSearchPanelViewController
    private var previousApplication: NSRunningApplication?

    @Dependency(\.mainQueue)
    private var mainQueue
    @Dependency(\.pasteService)
    private var pasteService
    @Dependency(\.pasteboardHistoryRepository)
    private var pasteboardHistoryRepository

    // MARK: - Initialize
    init() {
        let initialRect = NSRect(x: 0, y: 0, width: panelWidth, height: searchHeaderHeight + padding)
        let panel = CPYSearchPanel(contentRect: initialRect)
        let viewController = CPYSearchPanelViewController()
        panel.contentViewController = viewController

        self.searchPanel = panel
        self.searchViewController = viewController

        super.init(window: panel)

        setupEventHandlers()
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    // MARK: - Setup
    private func setupEventHandlers() {
        searchViewController.onSelect = { [weak self] item in
            self?.handleSelection(item)
        }

        searchViewController.onCancel = { [weak self] in
            self?.dismiss(restoreFocus: true)
        }

        searchViewController.onItemsCountChanged = { [weak self] count in
            self?.updatePanelFrame(itemCount: count, animated: true)
        }

        NotificationCenter.default.addObserver(
            self,
            selector: #selector(windowDidResignKey),
            name: NSWindow.didResignKeyNotification,
            object: searchPanel
        )
    }

    // MARK: - Presentation
    /// Shows the floating search panel.
    ///
    /// ## Activation & Pasting note:
    /// Because Clipy is an `LSUIElement` (agent/menu bar app), giving the search panel reliable
    /// keyboard focus requires calling `NSApp.activate(ignoringOtherApps: true)`.
    ///
    /// However, `PasteService.paste()` emits a `⌘V` CGEvent to the currently frontmost application.
    /// If Clipy remains active when the event is posted, the paste keystroke is consumed by Clipy itself!
    /// Therefore, we save `previousApplication = NSWorkspace.shared.frontmostApplication` upon presentation,
    /// and restore focus to `previousApplication` upon closing and confirm before triggering `PasteService.paste()`.
    func show() {
        previousApplication = NSWorkspace.shared.frontmostApplication

        searchViewController.resetSearch()
        updatePanelFrame(itemCount: searchViewController.items.count, animated: false)

        NSApp.activate(ignoringOtherApps: true)
        searchPanel.makeKeyAndOrderFront(nil)
        searchPanel.makeFirstResponder(searchViewController.searchField)
    }

    func dismiss(restoreFocus: Bool) {
        searchPanel.orderOut(nil)
        if restoreFocus, let prevApp = previousApplication, prevApp.bundleIdentifier != Bundle.main.bundleIdentifier {
            prevApp.activate(options: .activateIgnoringOtherApps)
        }
    }

    // MARK: - Frame Calculation
    private func updatePanelFrame(itemCount: Int, animated: Bool) {
        guard let screen = targetScreen() else { return }

        let visibleRows = min(itemCount, Constants.Search.maxVisibleRows)
        let contentHeight: CGFloat
        if visibleRows > 0 {
            contentHeight = searchHeaderHeight + (CGFloat(visibleRows) * rowHeight) + padding
        } else if !searchViewController.searchField.stringValue.isEmpty {
            contentHeight = searchHeaderHeight + emptyContentHeight
        } else {
            contentHeight = searchHeaderHeight + padding
        }

        let screenFrame = screen.visibleFrame
        let newX = screenFrame.origin.x + (screenFrame.width - panelWidth) / 2
        // Position slightly above vertical center for Spotlight-like ergonomics
        let newY = screenFrame.origin.y + (screenFrame.height - contentHeight) * 0.65

        let newFrame = NSRect(x: newX, y: newY, width: panelWidth, height: contentHeight)
        searchPanel.setFrame(newFrame, display: true, animate: animated)
    }

    private func targetScreen() -> NSScreen? {
        let mouseLocation = NSEvent.mouseLocation
        return NSScreen.screens.first { $0.frame.contains(mouseLocation) } ?? NSScreen.main
    }

    // MARK: - Selection Handling
    private func handleSelection(_ item: SearchResultItem) {
        let prevApp = previousApplication
        dismiss(restoreFocus: false)

        // Reactivate target application before sending paste keystroke
        if let prevApp = prevApp, prevApp.bundleIdentifier != Bundle.main.bundleIdentifier {
            prevApp.activate(options: .activateIgnoringOtherApps)
        }

        // Wait 120ms for target app activation to complete before emitting ⌘V
        mainQueue.schedule(after: mainQueue.now.advanced(by: .milliseconds(120))) { [weak self] in
            self?.paste(item: item)
        }
    }

    private func paste(item: SearchResultItem) {
        switch item {
        case .history(let detail):
            guard let content = pasteboardHistoryRepository.fetchContent(id: detail.history.id) else {
                NSSound.beep()
                return
            }
            pasteService.paste(id: detail.history.id, content: content)
        case .snippet(let match):
            pasteService.copyToPasteboard(with: match.snippet.content)
            pasteService.paste()
        }
    }

    // MARK: - Notifications
    @objc private func windowDidResignKey(_ notification: Notification) {
        dismiss(restoreFocus: false)
    }
}

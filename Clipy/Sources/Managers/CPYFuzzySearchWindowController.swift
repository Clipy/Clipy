//
//  CPYFuzzySearchWindowController.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import Dependencies

// MARK: - Item
/// A single searchable entry shown in the fuzzy panel.
private struct FuzzySearchItem {
    enum Kind {
        case history(PasteboardHistory.ID)
        case snippet(content: String)
    }

    let kind: Kind
    /// Source badge, e.g. "History" or the owning snippet folder title.
    let source: String
    /// Characters shown to the user, with line breaks and tabs replaced by
    /// visible glyphs so multi-line clips read on a single row.
    let displayCharacters: [Character]
    /// Lowercased characters matched against. Line breaks/tabs are kept as plain
    /// whitespace here (not the glyphs), so the glyphs are never searched. Same
    /// length as `displayCharacters`, so match positions map 1:1 for highlighting.
    let searchCharacters: [Character]

    /// - Parameter rawText: the (already end-trimmed) text, possibly multi-line.
    init(kind: Kind, rawText: String, source: String, maxLength: Int = 500) {
        self.kind = kind
        self.source = source

        var display = [Character]()
        var search = [Character]()
        display.reserveCapacity(Swift.min(rawText.count, maxLength))
        search.reserveCapacity(Swift.min(rawText.count, maxLength))
        for character in rawText.prefix(maxLength) {
            switch character {
            case "\n", "\r", "\r\n":
                display.append(DisplayGlyph.newline)
                search.append(" ")
            case "\t":
                display.append(DisplayGlyph.tab)
                search.append(" ")
            default:
                display.append(character)
                // Lowercase per-character, keeping length 1:1 with the display.
                search.append(character.lowercased().first ?? character)
            }
        }
        self.displayCharacters = display
        self.searchCharacters = search
    }
}

// MARK: - Display Glyphs
/// Single-character substitutes shown in place of whitespace that would
/// otherwise collapse a multi-line clip to its first line.
private enum DisplayGlyph {
    static let newline: Character = "↵"
    static let tab: Character = "⇥"

    static let all: Set<Character> = [newline, tab]
}

// MARK: - Panel
/// Borderless panel that is allowed to become key so its search field can
/// receive keystrokes even though Clipy runs as a menu-bar agent.
private final class CPYFuzzySearchPanel: NSPanel {
    override var canBecomeKey: Bool { true }
    override var canBecomeMain: Bool { true }
}

// MARK: - Window Controller
final class CPYFuzzySearchWindowController: NSWindowController {

    // MARK: - Properties
    private let searchField = NSTextField()
    private let tableView = NSTableView()
    private let scrollView = NSScrollView()
    private let placeholderLabel = NSTextField(labelWithString: "")

    private var allItems = [FuzzySearchItem]()
    private var filteredItems = [FuzzySearchItem]()
    /// Matched character positions for the currently filtered rows.
    private var highlightPositions = [[Int]]()

    private let rowHeight: CGFloat = 34
    private let maxVisibleRows = 10

    @Dependency(\.pasteboardHistoryRepository)
    private var pasteboardHistoryRepository
    @Dependency(\.snippetRepository)
    private var snippetRepository

    // MARK: - Initialize
    init() {
        let panel = CPYFuzzySearchPanel(
            contentRect: NSRect(x: 0, y: 0, width: 680, height: 400),
            styleMask: [.borderless, .nonactivatingPanel],
            backing: .buffered,
            defer: false
        )
        panel.isFloatingPanel = true
        panel.level = .modalPanel
        panel.hidesOnDeactivate = false
        panel.isMovableByWindowBackground = false
        panel.backgroundColor = .clear
        panel.isOpaque = false
        panel.hasShadow = true
        super.init(window: panel)
        panel.delegate = self
        setupViews()
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    // MARK: - Show
    func showPanel() {
        guard let window = window else { return }

        reloadItems()
        searchField.stringValue = ""
        applyFilter(query: "")

        positionWindow()

        // Deliberately do NOT activate the app. As a non-activating panel it can
        // still become key and receive typing, while the app the user was in
        // stays frontmost — so pressing Enter pastes into it immediately, exactly
        // like selecting from the history menu.
        window.makeKeyAndOrderFront(nil)
        window.orderFrontRegardless()
        window.makeFirstResponder(searchField)
    }

    private func closePanel() {
        window?.orderOut(nil)
    }
}

// MARK: - Setup
private extension CPYFuzzySearchWindowController {
    func setupViews() {
        guard let window = window else { return }

        let contentView = NSVisualEffectView()
        contentView.material = .popover
        contentView.state = .active
        contentView.blendingMode = .behindWindow
        contentView.wantsLayer = true
        contentView.layer?.cornerRadius = 10
        contentView.layer?.masksToBounds = true
        window.contentView = contentView

        // Search field
        searchField.translatesAutoresizingMaskIntoConstraints = false
        searchField.font = .systemFont(ofSize: 22, weight: .light)
        searchField.placeholderString = String(localized: "Search history and snippets…")
        searchField.isBordered = false
        searchField.drawsBackground = false
        searchField.focusRingType = .none
        searchField.delegate = self
        searchField.maximumNumberOfLines = 1
        searchField.cell?.usesSingleLineMode = true
        searchField.cell?.lineBreakMode = .byTruncatingTail
        contentView.addSubview(searchField)

        let separator = NSBox()
        separator.boxType = .separator
        separator.translatesAutoresizingMaskIntoConstraints = false
        contentView.addSubview(separator)

        // Table
        let column = NSTableColumn(identifier: NSUserInterfaceItemIdentifier("main"))
        column.resizingMask = .autoresizingMask
        tableView.addTableColumn(column)
        // Make the single column stretch to fill the full table width so long
        // entries use all the available horizontal space instead of truncating.
        tableView.columnAutoresizingStyle = .lastColumnOnlyAutoresizingStyle
        tableView.headerView = nil
        tableView.backgroundColor = .clear
        tableView.rowHeight = rowHeight
        tableView.intercellSpacing = NSSize(width: 0, height: 0)
        tableView.selectionHighlightStyle = .regular
        tableView.style = .fullWidth
        tableView.dataSource = self
        tableView.delegate = self
        tableView.action = #selector(tableViewClicked)
        tableView.target = self
        tableView.doubleAction = #selector(tableViewClicked)

        scrollView.translatesAutoresizingMaskIntoConstraints = false
        scrollView.documentView = tableView
        scrollView.hasVerticalScroller = true
        scrollView.drawsBackground = false
        scrollView.automaticallyAdjustsContentInsets = false
        contentView.addSubview(scrollView)

        // Placeholder shown when there are no results.
        placeholderLabel.translatesAutoresizingMaskIntoConstraints = false
        placeholderLabel.font = .systemFont(ofSize: 13)
        placeholderLabel.textColor = .secondaryLabelColor
        placeholderLabel.alignment = .center
        placeholderLabel.stringValue = String(localized: "No results")
        placeholderLabel.isHidden = true
        contentView.addSubview(placeholderLabel)

        NSLayoutConstraint.activate([
            searchField.topAnchor.constraint(equalTo: contentView.topAnchor, constant: 16),
            searchField.leadingAnchor.constraint(equalTo: contentView.leadingAnchor, constant: 18),
            searchField.trailingAnchor.constraint(equalTo: contentView.trailingAnchor, constant: -18),

            separator.topAnchor.constraint(equalTo: searchField.bottomAnchor, constant: 12),
            separator.leadingAnchor.constraint(equalTo: contentView.leadingAnchor),
            separator.trailingAnchor.constraint(equalTo: contentView.trailingAnchor),

            scrollView.topAnchor.constraint(equalTo: separator.bottomAnchor),
            scrollView.leadingAnchor.constraint(equalTo: contentView.leadingAnchor),
            scrollView.trailingAnchor.constraint(equalTo: contentView.trailingAnchor),
            scrollView.bottomAnchor.constraint(equalTo: contentView.bottomAnchor),

            placeholderLabel.centerXAnchor.constraint(equalTo: scrollView.centerXAnchor),
            placeholderLabel.centerYAnchor.constraint(equalTo: scrollView.centerYAnchor)
        ])
    }

    func positionWindow() {
        guard let window = window else { return }
        let mouseLocation = NSEvent.mouseLocation
        let screen = NSScreen.screens.first { NSMouseInRect(mouseLocation, $0.frame, false) } ?? NSScreen.main
        guard let visibleFrame = screen?.visibleFrame else { return }

        let width: CGFloat = min(900, visibleFrame.width - 80)
        let height = windowHeight()
        let originX = visibleFrame.midX - width / 2
        // Anchor the top edge in the upper third (Spotlight-style) so the list
        // grows downward instead of the whole window jumping as results change.
        let topAnchorY = visibleFrame.origin.y + visibleFrame.height * 0.82
        let originY = topAnchorY - height
        window.setFrame(NSRect(x: originX, y: originY, width: width, height: height), display: true)
        // Stretch the column to the new table width so entries fill the row.
        tableView.sizeLastColumnToFit()
    }

    func windowHeight() -> CGFloat {
        let headerHeight: CGFloat = 16 + searchField.intrinsicContentSize.height + 12 + 1
        let visibleRows = max(1, min(maxVisibleRows, filteredItems.count))
        let listHeight = CGFloat(visibleRows) * rowHeight + 8
        return headerHeight + listHeight
    }
}

// MARK: - Data
private extension CPYFuzzySearchWindowController {
    func reloadItems() {
        var items = [FuzzySearchItem]()

        // Search up to the configured history maximum, same as the rest of the app.
        let reorderClipsAfterPasting = AppEnvironment.current.defaults.bool(forKey: Constants.UserDefaults.reorderClipsAfterPasting)
        let maxHistory = AppEnvironment.current.defaults.integer(forKey: Constants.UserDefaults.maxHistorySize)
        let historyLabel = String(localized: "History")
        let historyDetails = pasteboardHistoryRepository.fetchHistoryDetails(
            sortsByCreatedAt: !reorderClipsAfterPasting,
            includesThumbnailAsset: false,
            limit: maxHistory
        )
        for detail in historyDetails {
            let rawText = detail.history.fullDisplaySource
            guard !rawText.isEmpty else { continue }
            items.append(FuzzySearchItem(kind: .history(detail.history.id), rawText: rawText, source: historyLabel))
        }

        // Enabled snippets from enabled folders.
        for folderDetail in snippetRepository.fetchFolderDetails() where folderDetail.folder.isEnabled {
            for snippet in folderDetail.snippets where snippet.isEnabled {
                let rawText = snippet.title.trimmingCharacters(in: .whitespacesAndNewlines)
                guard !rawText.isEmpty else { continue }
                items.append(FuzzySearchItem(kind: .snippet(content: snippet.content), rawText: rawText, source: folderDetail.folder.title))
            }
        }

        allItems = items
    }

    func applyFilter(query: String) {
        let trimmed = query.trimmingCharacters(in: .whitespaces)
        if trimmed.isEmpty {
            filteredItems = allItems
            highlightPositions = Array(repeating: [], count: allItems.count)
        } else {
            let queryCharacters = Array(trimmed.lowercased())
            var scored = [(item: FuzzySearchItem, match: FuzzyMatcher.Match, order: Int)]()
            for (index, item) in allItems.enumerated() {
                if let match = FuzzyMatcher.match(query: queryCharacters, in: item.searchCharacters) {
                    scored.append((item, match, index))
                }
            }
            scored.sort { lhs, rhs in
                if lhs.match.score != rhs.match.score {
                    return lhs.match.score > rhs.match.score
                }
                return lhs.order < rhs.order
            }
            filteredItems = scored.map { $0.item }
            highlightPositions = scored.map { $0.match.positions }
        }

        placeholderLabel.isHidden = !filteredItems.isEmpty
        tableView.reloadData()
        if !filteredItems.isEmpty {
            tableView.selectRowIndexes(IndexSet(integer: 0), byExtendingSelection: false)
            tableView.scrollRowToVisible(0)
        }
        positionWindow()
    }
}

// MARK: - Selection
private extension CPYFuzzySearchWindowController {
    @objc func tableViewClicked() {
        guard tableView.clickedRow >= 0 else { return }
        performSelection(at: tableView.clickedRow)
    }

    func performSelection(at row: Int) {
        guard filteredItems.indices.contains(row) else {
            NSSound.beep()
            return
        }
        let item = filteredItems[row]
        let pasteService = AppEnvironment.current.pasteService

        // Resolve everything up front so that, once the panel is dismissed,
        // nothing but the paste itself happens.
        let pasteAction: () -> Void
        switch item.kind {
        case .history(let id):
            guard let content = pasteboardHistoryRepository.fetchContent(id: id) else {
                NSSound.beep()
                return
            }
            pasteAction = { pasteService.paste(id: id, content: content) }
        case .snippet(let content):
            pasteAction = {
                pasteService.copyToPasteboard(with: content)
                pasteService.paste()
            }
        }

        // Dismiss and paste straight away. The panel never activated Clipy, so
        // the previously focused app is still frontmost and receives the ⌘V
        // immediately — matching the history menu's behaviour.
        closePanel()
        pasteAction()
    }

    func moveSelection(by delta: Int) {
        guard !filteredItems.isEmpty else { return }
        let current = tableView.selectedRow
        var next = current + delta
        next = max(0, min(filteredItems.count - 1, next))
        tableView.selectRowIndexes(IndexSet(integer: next), byExtendingSelection: false)
        tableView.scrollRowToVisible(next)
    }
}

// MARK: - NSTableViewDataSource
extension CPYFuzzySearchWindowController: NSTableViewDataSource {
    func numberOfRows(in tableView: NSTableView) -> Int {
        return filteredItems.count
    }
}

// MARK: - NSTableViewDelegate
extension CPYFuzzySearchWindowController: NSTableViewDelegate {
    func tableView(_ tableView: NSTableView, viewFor tableColumn: NSTableColumn?, row: Int) -> NSView? {
        let identifier = NSUserInterfaceItemIdentifier("FuzzyCell")
        let cell = (tableView.makeView(withIdentifier: identifier, owner: self) as? FuzzySearchCellView) ?? {
            let view = FuzzySearchCellView()
            view.identifier = identifier
            return view
        }()
        let item = filteredItems[row]
        let positions = highlightPositions.indices.contains(row) ? highlightPositions[row] : []
        cell.configure(displayCharacters: item.displayCharacters, source: item.source, highlighted: positions)
        return cell
    }

    func tableView(_ tableView: NSTableView, rowViewForRow row: Int) -> NSTableRowView? {
        return FuzzySearchRowView()
    }
}

// MARK: - NSTextFieldDelegate
extension CPYFuzzySearchWindowController: NSTextFieldDelegate {
    func controlTextDidChange(_ obj: Notification) {
        applyFilter(query: searchField.stringValue)
    }

    func control(_ control: NSControl, textView: NSTextView, doCommandBy commandSelector: Selector) -> Bool {
        switch commandSelector {
        case #selector(NSResponder.moveUp(_:)):
            moveSelection(by: -1)
            return true
        case #selector(NSResponder.moveDown(_:)):
            moveSelection(by: 1)
            return true
        case #selector(NSResponder.insertNewline(_:)):
            performSelection(at: tableView.selectedRow)
            return true
        case #selector(NSResponder.cancelOperation(_:)):
            closePanel()
            return true
        default:
            return false
        }
    }
}

// MARK: - NSWindowDelegate
extension CPYFuzzySearchWindowController: NSWindowDelegate {
    func windowDidResignKey(_ notification: Notification) {
        // Dismiss when the user clicks away or switches apps.
        closePanel()
    }
}

// MARK: - Row View
/// Rounds the selection highlight to match the panel style.
private final class FuzzySearchRowView: NSTableRowView {
    override func drawSelection(in dirtyRect: NSRect) {
        guard selectionHighlightStyle != .none else { return }
        let rect = bounds.insetBy(dx: 6, dy: 2)
        let path = NSBezierPath(roundedRect: rect, xRadius: 6, yRadius: 6)
        NSColor.selectedContentBackgroundColor.setFill()
        path.fill()
    }
}

// MARK: - Cell View
private final class FuzzySearchCellView: NSTableCellView {
    private let titleLabel = NSTextField(labelWithString: "")
    private let sourceLabel = NSTextField(labelWithString: "")

    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        setup()
    }

    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }

    private func setup() {
        titleLabel.translatesAutoresizingMaskIntoConstraints = false
        titleLabel.maximumNumberOfLines = 1
        titleLabel.cell?.usesSingleLineMode = true
        titleLabel.cell?.lineBreakMode = .byTruncatingTail
        titleLabel.font = .systemFont(ofSize: 13)
        addSubview(titleLabel)

        sourceLabel.translatesAutoresizingMaskIntoConstraints = false
        sourceLabel.font = .systemFont(ofSize: 10, weight: .medium)
        sourceLabel.textColor = .secondaryLabelColor
        sourceLabel.alignment = .right
        sourceLabel.setContentHuggingPriority(.required, for: .horizontal)
        sourceLabel.setContentCompressionResistancePriority(.required, for: .horizontal)
        addSubview(sourceLabel)

        NSLayoutConstraint.activate([
            titleLabel.leadingAnchor.constraint(equalTo: leadingAnchor, constant: 18),
            titleLabel.centerYAnchor.constraint(equalTo: centerYAnchor),
            sourceLabel.leadingAnchor.constraint(greaterThanOrEqualTo: titleLabel.trailingAnchor, constant: 8),
            sourceLabel.trailingAnchor.constraint(equalTo: trailingAnchor, constant: -18),
            sourceLabel.centerYAnchor.constraint(equalTo: centerYAnchor)
        ])
    }

    func configure(displayCharacters: [Character], source: String, highlighted: [Int]) {
        let attributed = NSMutableAttributedString(
            string: String(displayCharacters),
            attributes: [
                .font: NSFont.systemFont(ofSize: 13),
                .foregroundColor: NSColor.labelColor
            ]
        )
        // Dim the whitespace glyphs so they read as subtle markers.
        for (position, character) in displayCharacters.enumerated() where DisplayGlyph.all.contains(character) {
            attributed.addAttribute(
                .foregroundColor,
                value: NSColor.tertiaryLabelColor,
                range: NSRange(location: position, length: 1)
            )
        }
        // Matched characters win over the dimming above.
        let highlightSet = Set(highlighted)
        for position in highlightSet where position < displayCharacters.count {
            attributed.addAttributes(
                [
                    .font: NSFont.systemFont(ofSize: 13, weight: .bold),
                    .foregroundColor: NSColor.controlAccentColor
                ],
                range: NSRange(location: position, length: 1)
            )
        }
        titleLabel.attributedStringValue = attributed
        sourceLabel.stringValue = source.uppercased()
    }
}

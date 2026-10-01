import AppKit
import Sharing

/// Search is a header in the original NSMenu; every result is a native menu item.
final class HistoryMenuSearchView: NSView, NSSearchFieldDelegate {
    let searchField: NSSearchField = {
        let field = NSSearchField()
        field.cell = HistoryMenuSearchCell(textCell: "")
        field.isEditable = true
        field.isSelectable = true
        field.isBezeled = true
        field.focusRingType = .exterior
        field.cell?.isScrollable = true
        return field
    }()
    weak var sortItem: NSMenuItem?
    @Shared(.historySearchSort) private var searchSort
    weak var owningMenu: NSMenu?
    var historyItems = [NSMenuItem]()
    @Shared(.showsToolTipsOnMenuItems) private var showsToolTipsOnMenuItems
    @Shared(.maximumToolTipLength) private var maximumToolTipLength
    @Shared(.maximumMenuItemTitleLength) private var maximumMenuItemTitleLength
    @Shared(.reordersClipsAfterPasting) private var reordersClipsAfterPasting
    private let store = HistoryMenuSearchStore()
    private let queue = DispatchQueue(label: "Clipy.menuSearch", qos: .userInitiated)
    private var results = [NSMenuItem]()
    private var originalItems = [NSMenuItem]()
    private var pending: DispatchWorkItem?
    private var disabledItems = [(NSMenuItem, Bool)]()
    private var generation = 0
    private var searching = false

    init() {
        super.init(frame: NSRect(x: 0, y: 0, width: 320, height: 38))
        searchField.frame = NSRect(x: 10, y: 6, width: 300, height: 26)
        searchField.autoresizingMask = [.width]
        searchField.placeholderString = String(localized: "Search history…")
        searchField.sendsSearchStringImmediately = true
        searchField.delegate = self
        addSubview(searchField)
    }

    required init?(coder: NSCoder) { fatalError("init(coder:) has not been implemented") }

    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        if let window { window.makeFirstResponder(searchField) }
    }

    func stop() {
        pending?.cancel()
        pending = nil
        for (item, enabled) in disabledItems { item.isEnabled = enabled }
        disabledItems.removeAll()
        generation += 1
        searching = false
    }

    func endMenuTracking() {
        (searchField.cell as? HistoryMenuSearchCell)?.endMenuTracking()
        if let window = searchField.window, let editor = searchField.currentEditor(), window.firstResponder === editor {
            window.makeFirstResponder(nil)
        }
        stop()
    }

    func reset() {
        stop()
        searchField.stringValue = ""
        restoreHistory()
        updateSortVisibility()
    }

    private func restoreHistory() {
        if !originalItems.isEmpty {
            replaceItems(originalItems)
            originalItems.removeAll()
        }
        results.removeAll()
    }

    // Publish one complete menu snapshot. Hiding hundreds of original rows and
    // inserting results one by one leaves stale tracking geometry in AppKit.
    private func replaceItems(_ items: [NSMenuItem]) {
        guard let menu = owningMenu else { return }
        NSAnimationContext.runAnimationGroup { context in
            context.duration = 0
            // Keep the search header and tools attached to their window. Replacing
            // those views while their field editor is active can dismiss tracking.
            let current = menu.items
            var prefix = 0
            while prefix < min(current.count, items.count), current[prefix] === items[prefix] { prefix += 1 }
            var suffix = 0
            while suffix < min(current.count, items.count) - prefix,
                  current[current.count - 1 - suffix] === items[items.count - 1 - suffix] { suffix += 1 }
            for index in (prefix..<(current.count - suffix)).reversed() { menu.removeItem(at: index) }
            for index in prefix..<(items.count - suffix) { menu.insertItem(items[index], at: index) }
            menu.update()
        }
    }

    private func publish(_ items: [NSMenuItem]) {
        guard let menu = owningMenu else { return }
        if originalItems.isEmpty { originalItems = menu.items }
        let historyIDs = Set(historyItems.map(ObjectIdentifier.init))
        var snapshot = [NSMenuItem]()
        for item in originalItems {
            if historyIDs.contains(ObjectIdentifier(item)) {
                if item === historyItems.first { snapshot.append(contentsOf: items) }
            } else {
                snapshot.append(item)
            }
        }
        results = items
        replaceItems(snapshot)
    }

    private func message(_ text: String) {
        let item = NSMenuItem(title: text, action: nil, keyEquivalent: "")
        item.isEnabled = false
        publish([item])
    }

    func installSortControl(in menu: NSMenu) {
        let item = NSMenuItem(title: String(localized: "Sort Search Results"), action: nil, keyEquivalent: "")
        let submenu = NSMenu()
        for sort in HistoryMenuSearchStore.Sort.allCases {
            let option = NSMenuItem(title: sort.title, action: #selector(changeSearchSort(_:)), keyEquivalent: "")
            option.target = self
            option.representedObject = sort.rawValue
            submenu.addItem(option)
        }
        item.submenu = submenu
        menu.addItem(item)
        sortItem = item
        updateSortVisibility()
    }

    @objc private func changeSearchSort(_ sender: NSMenuItem) {
        guard let value = sender.representedObject as? String,
              let sort = HistoryMenuSearchStore.Sort(rawValue: value) else { return }
        $searchSort.withLock { $0 = sort }
        controlTextDidChange(Notification(name: NSControl.textDidChangeNotification))
    }

    private func updateSortVisibility() {
        sortItem?.isHidden = searchField.stringValue.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
        for item in sortItem?.submenu?.items ?? [] {
            item.state = (item.representedObject as? String) == searchSort.rawValue ? .on : .off
        }
    }

    func controlTextDidChange(_ obj: Notification) {
        updateSortVisibility()
        stop()
        let query = searchField.stringValue
        guard !query.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
            restoreHistory()
            return
        }
        searching = true
        // Keep the existing layout visible while the next snapshot is computed.
        // Disable stale actions until it arrives; never paste an outdated match.
        for item in results.isEmpty ? historyItems : results {
            disabledItems.append((item, item.isEnabled))
            item.isEnabled = false
        }
        let request = generation
        let byCreation = !reordersClipsAfterPasting
        let sort = searchSort
        let titleLimit = max(10, maximumMenuItemTitleLength)
        let store = self.store
        let work = DispatchWorkItem { [weak self] in
            let matches = Result { try store.search(query: query, sortsByCreatedAt: byCreation, limit: 31, sort: sort) }
                RunLoop.main.perform(inModes: [.eventTracking, .default]) { [weak self] in
                    guard let self, self.generation == request else { return }
                    self.searching = false
                    for (item, enabled) in self.disabledItems { item.isEnabled = enabled }
                    self.disabledItems.removeAll()
                    switch matches {
                    case .success(let entries):
                        var items = [NSMenuItem]()
                        for entry in entries.prefix(30) {
                            let limit = titleLimit
                            let preview = entry.label.prefix(limit).components(separatedBy: .whitespacesAndNewlines)
                                .filter { !$0.isEmpty }.joined(separator: " ")
                            let title = preview + (entry.label.count > limit ? "…" : "")
                            let item = NSMenuItem(title: title,
                                                  action: #selector(AppDelegate.selectClipMenuItem(_:)), keyEquivalent: "")
                            item.target = NSApp.delegate
                            item.representedObject = entry.id
                            if self.showsToolTipsOnMenuItems {
                                item.toolTip = String(entry.label.prefix(max(1, self.maximumToolTipLength)))
                            }
                            items.append(item)
                        }
                        if entries.isEmpty {
                            self.message(String(localized: "No matching clips"))
                            return
                        }
                        if entries.count > 30 {
                            let more = NSMenuItem(title: String(localized: "First 30 matches — narrow your search"), action: nil, keyEquivalent: "")
                            more.isEnabled = false
                            items.append(more)
                        }
                        self.publish(items)
                    case .failure:
                        self.message(String(localized: "Could not search history"))
                    }
                }
        }
        pending = work
        queue.async(execute: work)
    }

    func control(_ control: NSControl, textView: NSTextView, doCommandBy commandSelector: Selector) -> Bool {
        if commandSelector == #selector(NSResponder.cancelOperation(_:)) {
            owningMenu?.cancelTracking()
            return true
        }
        if commandSelector == #selector(NSResponder.insertNewline(_:)), !searchField.stringValue.isEmpty {
            guard !searching else { return true }
            let highlighted = owningMenu?.highlightedItem
            let selected = highlighted?.representedObject is PasteboardHistory.ID ? highlighted : results.first
            guard let item = selected, let action = item.action, item.isEnabled else { return true }
            owningMenu?.cancelTracking()
            DispatchQueue.main.async { NSApp.sendAction(action, to: item.target, from: item) }
            return true
        }
        return false
    }
}

final class HistoryMenuFieldEditor: NSTextView {
    private let insertionPoint = HistoryMenuInsertionPoint()
    private var blinkTimer: Timer?
    private var blinkOn = true
    private var caretEnabled = false
    private var refreshQueued = false

    var isMenuInsertionPointActive: Bool {
        caretEnabled && isEditable && selectedRange().length == 0 && window?.firstResponder === self
    }

    // AppKit suppresses its shared insertion-point timer in non-key menu
    // windows. Draw just the caret locally; all editing stays in NSTextView.
    override var shouldDrawInsertionPoint: Bool { false }

    override func becomeFirstResponder() -> Bool {
        let accepted = super.becomeFirstResponder()
        if accepted {
            caretEnabled = true
            if insertionPoint.superview == nil { addSubview(insertionPoint) }
            blinkTimer?.invalidate()
            let timer = Timer(timeInterval: 0.55, repeats: true) { [weak self] _ in
                guard let self else { return }
                self.blinkOn.toggle()
                self.refreshInsertionPoint()
            }
            blinkTimer = timer
            RunLoop.main.add(timer, forMode: .default)
            RunLoop.main.add(timer, forMode: .eventTracking)
            queueInsertionPointRefresh()
        }
        return accepted
    }

    override func resignFirstResponder() -> Bool {
        let accepted = super.resignFirstResponder()
        if accepted { stopInsertionPoint() }
        return accepted
    }

    override func viewDidMoveToWindow() {
        super.viewDidMoveToWindow()
        if window == nil { stopInsertionPoint() }
    }

    override func updateInsertionPointStateAndRestartTimer(_ restartFlag: Bool) {
        super.updateInsertionPointStateAndRestartTimer(restartFlag)
        queueInsertionPointRefresh()
    }

    private func queueInsertionPointRefresh() {
        blinkOn = true
        guard !refreshQueued else { return }
        refreshQueued = true
        RunLoop.main.perform(inModes: [.default, .eventTracking]) { [weak self] in
            guard let self else { return }
            self.refreshQueued = false
            self.refreshInsertionPoint()
        }
    }

    private func refreshInsertionPoint() {
        guard isMenuInsertionPointActive, let window else {
            insertionPoint.isHidden = true
            return
        }
        let range = NSRange(location: min(selectedRange().location, string.utf16.count), length: 0)
        let screenRect = firstRect(forCharacterRange: range, actualRange: nil)
        var rect = convert(window.convertFromScreen(screenRect), from: nil)
        rect.size.width = 1.5
        insertionPoint.frame = rect
        insertionPoint.isHidden = !blinkOn || rect.height <= 0
        insertionPoint.needsDisplay = true
    }

    fileprivate func stopInsertionPoint() {
        caretEnabled = false
        blinkTimer?.invalidate()
        blinkTimer = nil
        insertionPoint.isHidden = true
    }

    deinit { blinkTimer?.invalidate() }
}

final class HistoryMenuInsertionPoint: NSView {
    override func hitTest(_ point: NSPoint) -> NSView? { nil }
    override func draw(_ dirtyRect: NSRect) {
        NSColor.labelColor.setFill()
        bounds.fill()
    }
}

final class HistoryMenuSearchCell: NSSearchFieldCell {
    private lazy var editor: NSTextView = {
        let editor = HistoryMenuFieldEditor()
        editor.isFieldEditor = true
        editor.isRichText = false
        editor.insertionPointColor = .labelColor
        return editor
    }()

    override func fieldEditor(for controlView: NSView) -> NSTextView? { editor }

    func endMenuTracking() { (editor as? HistoryMenuFieldEditor)?.stopInsertionPoint() }
}

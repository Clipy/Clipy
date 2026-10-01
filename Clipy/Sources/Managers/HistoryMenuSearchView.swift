import AppKit
import Sharing

/// Search is a header in the original NSMenu; every result is a native menu item.
final class HistoryMenuSearchView: NSView, NSSearchFieldDelegate {
    let searchField = NSSearchField()
    weak var owningMenu: NSMenu?
    var historyItems = [NSMenuItem]()
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

    func reset() {
        stop()
        searchField.stringValue = ""
        restoreHistory()
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

    func controlTextDidChange(_ obj: Notification) {
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
        let titleLimit = max(10, maximumMenuItemTitleLength)
        let store = self.store
        let work = DispatchWorkItem { [weak self] in
            let matches = Result { try store.search(query: query, sortsByCreatedAt: byCreation, limit: 31) }
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

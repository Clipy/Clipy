//
//  CPYSearchPanelViewController.swift
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
import Combine
import Dependencies

final class CPYSearchPanelViewController: NSViewController {

    // MARK: - Properties
    let searchField = NSTextField()
    private let searchIconImageView = NSImageView()
    private let separatorView = NSBox()
    private let tableView = NSTableView()
    private let scrollView = NSScrollView()
    private let emptyLabel = NSTextField(labelWithString: "")

    @Dependency(\.searchService)
    private var searchService
    @Dependency(\.mainQueue)
    private var mainQueue

    private let querySubject = PassthroughSubject<String, Never>()
    private var cancellables: Set<AnyCancellable> = []

    private(set) var items: [SearchResultItem] = []

    var onSelect: ((SearchResultItem) -> Void)?
    var onCancel: (() -> Void)?
    var onItemsCountChanged: ((Int) -> Void)?

    // MARK: - Lifecycle
    override func loadView() {
        let visualEffectView = NSVisualEffectView()
        visualEffectView.material = .hudWindow
        visualEffectView.blendingMode = .behindWindow
        visualEffectView.state = .active
        visualEffectView.wantsLayer = true
        visualEffectView.layer?.cornerRadius = 12
        visualEffectView.layer?.masksToBounds = true
        self.view = visualEffectView
    }

    override func viewDidLoad() {
        super.viewDidLoad()
        setupUI()
        setupBindings()
    }

    // MARK: - UI Setup
    private func setupUI() {
        // Search Icon
        searchIconImageView.translatesAutoresizingMaskIntoConstraints = false
        searchIconImageView.image = NSImage(systemSymbolName: "magnifyingglass", accessibilityDescription: nil)
        searchIconImageView.contentTintColor = .secondaryLabelColor
        view.addSubview(searchIconImageView)

        // Search Field
        searchField.translatesAutoresizingMaskIntoConstraints = false
        searchField.placeholderString = String(localized: "Search clips and snippets...")
        searchField.font = .systemFont(ofSize: 16, weight: .regular)
        searchField.focusRingType = .none
        searchField.isBordered = false
        searchField.drawsBackground = false
        searchField.delegate = self
        view.addSubview(searchField)

        // Separator
        separatorView.translatesAutoresizingMaskIntoConstraints = false
        separatorView.boxType = .separator
        view.addSubview(separatorView)

        // Table View
        tableView.translatesAutoresizingMaskIntoConstraints = false
        tableView.headerView = nil
        tableView.rowHeight = 44
        tableView.intercellSpacing = NSSize(width: 0, height: 2)
        tableView.backgroundColor = .clear
        tableView.selectionHighlightStyle = .regular
        tableView.delegate = self
        tableView.dataSource = self
        tableView.target = self
        tableView.doubleAction = #selector(tableViewDoubleClicked)
        tableView.action = #selector(tableViewClicked)

        let column = NSTableColumn(identifier: NSUserInterfaceItemIdentifier("SearchColumn"))
        column.resizingMask = .autoresizingMask
        tableView.addTableColumn(column)

        // Scroll View
        scrollView.translatesAutoresizingMaskIntoConstraints = false
        scrollView.drawsBackground = false
        scrollView.hasVerticalScroller = true
        scrollView.autohidesScrollers = true
        scrollView.documentView = tableView
        view.addSubview(scrollView)

        // Empty Label
        emptyLabel.translatesAutoresizingMaskIntoConstraints = false
        emptyLabel.stringValue = String(localized: "No results found")
        emptyLabel.font = .systemFont(ofSize: 14, weight: .medium)
        emptyLabel.textColor = .secondaryLabelColor
        emptyLabel.alignment = .center
        emptyLabel.isHidden = true
        view.addSubview(emptyLabel)

        NSLayoutConstraint.activate([
            searchIconImageView.leadingAnchor.constraint(equalTo: view.leadingAnchor, constant: 14),
            searchIconImageView.topAnchor.constraint(equalTo: view.topAnchor, constant: 14),
            searchIconImageView.widthAnchor.constraint(equalToConstant: 20),
            searchIconImageView.heightAnchor.constraint(equalToConstant: 20),

            searchField.leadingAnchor.constraint(equalTo: searchIconImageView.trailingAnchor, constant: 10),
            searchField.trailingAnchor.constraint(equalTo: view.trailingAnchor, constant: -14),
            searchField.centerYAnchor.constraint(equalTo: searchIconImageView.centerYAnchor),
            searchField.heightAnchor.constraint(equalToConstant: 28),

            separatorView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            separatorView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            separatorView.topAnchor.constraint(equalTo: searchField.bottomAnchor, constant: 10),
            separatorView.heightAnchor.constraint(equalToConstant: 1),

            scrollView.leadingAnchor.constraint(equalTo: view.leadingAnchor),
            scrollView.trailingAnchor.constraint(equalTo: view.trailingAnchor),
            scrollView.topAnchor.constraint(equalTo: separatorView.bottomAnchor, constant: 4),
            scrollView.bottomAnchor.constraint(equalTo: view.bottomAnchor, constant: -4),

            emptyLabel.centerXAnchor.constraint(equalTo: view.centerXAnchor),
            emptyLabel.centerYAnchor.constraint(equalTo: scrollView.centerYAnchor)
        ])
    }

    // MARK: - Bindings
    private func setupBindings() {
        querySubject
            .debounce(for: .milliseconds(120), scheduler: mainQueue)
            .removeDuplicates()
            .sink { [weak self] query in
                self?.performSearch(query: query)
            }
            .store(in: &cancellables)
    }

    // MARK: - Search
    func resetSearch() {
        searchField.stringValue = ""
        performSearch(query: "")
    }

    private func performSearch(query: String) {
        items = searchService.search(query: query, limit: Constants.Search.resultLimit)
        tableView.reloadData()

        let hasQuery = !query.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
        emptyLabel.isHidden = !(items.isEmpty && hasQuery)

        if !items.isEmpty {
            tableView.selectRowIndexes(IndexSet(integer: 0), byExtendingSelection: false)
            tableView.scrollRowToVisible(0)
        }

        onItemsCountChanged?(items.count)
    }

    // MARK: - Actions
    @objc private func tableViewClicked() {
        let clickedRow = tableView.clickedRow
        guard clickedRow >= 0, clickedRow < items.count else { return }
        onSelect?(items[clickedRow])
    }

    @objc private func tableViewDoubleClicked() {
        let selectedRow = tableView.selectedRow
        guard selectedRow >= 0, selectedRow < items.count else { return }
        onSelect?(items[selectedRow])
    }

    private func selectPreviousRow() {
        let currentRow = tableView.selectedRow
        let targetRow = max(0, currentRow - 1)
        guard targetRow != currentRow, targetRow < items.count else { return }
        tableView.selectRowIndexes(IndexSet(integer: targetRow), byExtendingSelection: false)
        tableView.scrollRowToVisible(targetRow)
    }

    private func selectNextRow() {
        let currentRow = tableView.selectedRow
        let targetRow = min(items.count - 1, max(0, currentRow + 1))
        guard targetRow != currentRow, targetRow < items.count else { return }
        tableView.selectRowIndexes(IndexSet(integer: targetRow), byExtendingSelection: false)
        tableView.scrollRowToVisible(targetRow)
    }

    private func confirmSelection() {
        let selectedRow = tableView.selectedRow
        guard selectedRow >= 0, selectedRow < items.count else { return }
        onSelect?(items[selectedRow])
    }
}

// MARK: - NSTextFieldDelegate
extension CPYSearchPanelViewController: NSTextFieldDelegate {
    func controlTextDidChange(_ obj: Notification) {
        querySubject.send(searchField.stringValue)
    }

    func control(_ control: NSControl, textView: NSTextView, doCommandBy commandSelector: Selector) -> Bool {
        if commandSelector == #selector(NSResponder.moveUp(_:)) {
            selectPreviousRow()
            return true
        } else if commandSelector == #selector(NSResponder.moveDown(_:)) {
            selectNextRow()
            return true
        } else if commandSelector == #selector(NSResponder.insertNewline(_:)) {
            confirmSelection()
            return true
        } else if commandSelector == #selector(NSResponder.cancelOperation(_:)) {
            onCancel?()
            return true
        }
        return false
    }
}

// MARK: - NSTableViewDataSource & Delegate
extension CPYSearchPanelViewController: NSTableViewDataSource, NSTableViewDelegate {
    func numberOfRows(in tableView: NSTableView) -> Int {
        return items.count
    }

    func tableView(_ tableView: NSTableView, rowViewForRow row: Int) -> NSTableRowView? {
        return CPYSearchResultRowView()
    }

    func tableView(_ tableView: NSTableView, viewFor tableColumn: NSTableColumn?, row: Int) -> NSView? {
        guard row < items.count else { return nil }
        let cellView: CPYSearchResultCellView
        if let reusedView = tableView.makeView(withIdentifier: CPYSearchResultCellView.reuseIdentifier, owner: self) as? CPYSearchResultCellView {
            cellView = reusedView
        } else {
            cellView = CPYSearchResultCellView()
            cellView.identifier = CPYSearchResultCellView.reuseIdentifier
        }
        cellView.configure(with: items[row])
        return cellView
    }
}

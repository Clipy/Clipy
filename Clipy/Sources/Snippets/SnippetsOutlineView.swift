//
//  SnippetsOutlineView.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/07/05.
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import SwiftUI

/// Wraps `NSOutlineView` because SwiftUI's `List` cannot reproduce the required
/// drag and drop behaviors on macOS 13: reordering root-level folders, reordering
/// snippets inside a folder and moving snippets across folders with insertion
/// indicators, plus inline renaming of items.
struct SnippetsOutlineView: NSViewRepresentable {

    @ObservedObject var viewModel: SnippetsEditorViewModel

    func makeCoordinator() -> Coordinator {
        Coordinator(viewModel: viewModel)
    }

    func makeNSView(context: Context) -> NSScrollView {
        let outlineView = NSOutlineView()
        let column = NSTableColumn(identifier: NSUserInterfaceItemIdentifier("SnippetsEditorColumn"))
        column.resizingMask = .autoresizingMask
        outlineView.addTableColumn(column)
        outlineView.outlineTableColumn = column
        outlineView.headerView = nil
        outlineView.rowHeight = 25
        outlineView.intercellSpacing = NSSize(width: 3, height: 2)
        outlineView.indentationPerLevel = 16
        outlineView.focusRingType = .none
        outlineView.allowsMultipleSelection = false
        outlineView.autoresizesOutlineColumn = false
        outlineView.columnAutoresizingStyle = .firstColumnOnlyAutoresizingStyle
        // Enable Drag and Drop
        outlineView.registerForDraggedTypes([DraggedData.pasteboardType])
        outlineView.dataSource = context.coordinator
        outlineView.delegate = context.coordinator
        context.coordinator.outlineView = outlineView

        let scrollView = NSScrollView()
        scrollView.documentView = outlineView
        scrollView.hasVerticalScroller = true
        scrollView.autohidesScrollers = true

        context.coordinator.reloadData()
        context.coordinator.lastStructureVersion = viewModel.structureVersion
        return scrollView
    }

    func updateNSView(_ scrollView: NSScrollView, context: Context) {
        let coordinator = context.coordinator
        coordinator.viewModel = viewModel
        if coordinator.lastStructureVersion != viewModel.structureVersion {
            coordinator.lastStructureVersion = viewModel.structureVersion
            coordinator.reloadData()
        }
        coordinator.syncSelection()
    }
}

// MARK: - Coordinator
extension SnippetsOutlineView {
    final class Coordinator: NSObject {
        var viewModel: SnippetsEditorViewModel
        weak var outlineView: NSOutlineView?
        var lastStructureVersion = 0
        private var isSyncingSelection = false

        init(viewModel: SnippetsEditorViewModel) {
            self.viewModel = viewModel
            super.init()
        }

        /// Reloads without propagating selection change notifications back to the
        /// view model. Reloads run during the SwiftUI update pass, and the view model
        /// has already been updated at that point.
        func reloadData() {
            isSyncingSelection = true
            outlineView?.reloadData()
            isSyncingSelection = false
        }

        /// Reflects `viewModel.selection` into the outline view, expanding the parent
        /// folder when a snippet is selected.
        func syncSelection() {
            guard let outlineView else { return }
            guard let item = item(for: viewModel.selection) else {
                if viewModel.selection == nil && outlineView.selectedRow >= 0 {
                    isSyncingSelection = true
                    outlineView.deselectAll(nil)
                    isSyncingSelection = false
                }
                return
            }
            if let snippet = item as? EditorSnippet,
               let folder = viewModel.folders.first(where: { $0.id == snippet.folderID }) {
                outlineView.expandItem(folder)
            }
            let row = outlineView.row(forItem: item)
            guard row >= 0, row != outlineView.selectedRow else { return }
            isSyncingSelection = true
            outlineView.selectRowIndexes(IndexSet(integer: row), byExtendingSelection: false)
            outlineView.scrollRowToVisible(row)
            isSyncingSelection = false
        }

        private func item(for selection: SnippetsEditorViewModel.Selection?) -> Any? {
            switch selection {
            case .folder(let id):
                return viewModel.folders.first { $0.id == id }
            case .snippet(let id):
                for folder in viewModel.folders {
                    if let snippet = folder.snippets.first(where: { $0.id == id }) {
                        return snippet
                    }
                }
                return nil
            case nil:
                return nil
            }
        }
    }
}

// MARK: - NSOutlineView DataSource
extension SnippetsOutlineView.Coordinator: NSOutlineViewDataSource {
    func outlineView(_ outlineView: NSOutlineView, numberOfChildrenOfItem item: Any?) -> Int {
        if item == nil {
            return viewModel.folders.count
        } else if let folder = item as? EditorSnippetFolder {
            return folder.snippets.count
        }
        return 0
    }

    func outlineView(_ outlineView: NSOutlineView, isItemExpandable item: Any) -> Bool {
        (item as? EditorSnippetFolder).map { !$0.snippets.isEmpty } ?? false
    }

    func outlineView(_ outlineView: NSOutlineView, child index: Int, ofItem item: Any?) -> Any {
        (item as? EditorSnippetFolder).map { $0.snippets[index] as Any } ?? viewModel.folders[index] as Any
    }

    // MARK: - Drag and Drop
    func outlineView(_ outlineView: NSOutlineView, pasteboardWriterForItem item: Any) -> NSPasteboardWriting? {
        let pasteboardItem = NSPasteboardItem()
        if let folder = item as? EditorSnippetFolder, let index = viewModel.folders.firstIndex(where: { $0.id == folder.id }) {
            let draggedData = DraggedData(type: .folder, folderID: folder.id, snippetID: nil, index: index)
            guard let data = try? NSKeyedArchiver.archivedData(withRootObject: draggedData, requiringSecureCoding: true) else { return nil }
            pasteboardItem.setData(data, forType: DraggedData.pasteboardType)
        } else if let snippet = item as? EditorSnippet, let folder = outlineView.parent(forItem: snippet) as? EditorSnippetFolder {
            guard let index = folder.snippets.firstIndex(where: { $0.id == snippet.id }) else { return nil }
            let draggedData = DraggedData(type: .snippet, folderID: folder.id, snippetID: snippet.id, index: index)
            guard let data = try? NSKeyedArchiver.archivedData(withRootObject: draggedData, requiringSecureCoding: true) else { return nil }
            pasteboardItem.setData(data, forType: DraggedData.pasteboardType)
        } else {
            return nil
        }
        return pasteboardItem
    }

    func outlineView(_ outlineView: NSOutlineView, validateDrop info: NSDraggingInfo, proposedItem item: Any?, proposedChildIndex index: Int) -> NSDragOperation {
        guard let draggedData = draggedData(from: info) else { return NSDragOperation() }

        switch draggedData.type {
        case .folder where item == nil:
            return .move
        case .snippet where item is EditorSnippetFolder:
            return .move
        default:
            return NSDragOperation()
        }
    }

    func outlineView(_ outlineView: NSOutlineView, acceptDrop info: NSDraggingInfo, item: Any?, childIndex index: Int) -> Bool {
        guard let draggedData = draggedData(from: info) else { return false }

        switch draggedData.type {
        case .folder:
            return viewModel.moveFolder(draggedData, to: index)
        case .snippet:
            return viewModel.moveSnippet(draggedData, to: item as? EditorSnippetFolder, at: index)
        }
    }

    private func draggedData(from info: NSDraggingInfo) -> DraggedData? {
        let pasteboard = info.draggingPasteboard
        guard let data = pasteboard.data(forType: DraggedData.pasteboardType) else { return nil }
        return try? NSKeyedUnarchiver.unarchivedObject(ofClasses: [DraggedData.self, NSUUID.self], from: data) as? DraggedData
    }
}

// MARK: - NSOutlineView Delegate
extension SnippetsOutlineView.Coordinator: NSOutlineViewDelegate {
    func outlineView(_ outlineView: NSOutlineView, viewFor tableColumn: NSTableColumn?, item: Any) -> NSView? {
        if let folder = item as? EditorSnippetFolder {
            return SnippetHostingCell(title: folder.title, isFolder: true, isEnabled: folder.isEnabled) { [weak self] title in
                self?.viewModel.rename(folder: folder, title: title)
            }
        } else if let snippet = item as? EditorSnippet {
            return SnippetHostingCell(title: snippet.title, isFolder: false, isEnabled: snippet.isEnabled) { [weak self] title in
                self?.viewModel.rename(snippet: snippet, title: title)
            }
        }
        return nil
    }

    func outlineViewSelectionDidChange(_ notification: Notification) {
        guard !isSyncingSelection, let outlineView else { return }
        guard let item = outlineView.item(atRow: outlineView.selectedRow) else {
            viewModel.selection = nil
            return
        }
        if let folder = item as? EditorSnippetFolder {
            viewModel.selection = .folder(folder.id)
        } else if let snippet = item as? EditorSnippet {
            viewModel.selection = .snippet(snippet.id)
        }
    }
}

// MARK: - SwiftUI Cell
private struct SnippetRow: View {
    let title: String
    let isFolder: Bool
    let isEnabled: Bool
    var isSelected = false
    let onRename: (String) -> Void

    @State private var draft: String
    @State private var savedTitle: String
    @FocusState private var isEditing: Bool

    init(title: String, isFolder: Bool, isEnabled: Bool, onRename: @escaping (String) -> Void) {
        self.title = title
        self.isFolder = isFolder
        self.isEnabled = isEnabled
        self.onRename = onRename
        _draft = State(initialValue: title)
        _savedTitle = State(initialValue: title)
    }

    var body: some View {
        HStack(alignment: .center, spacing: 4) {
            Image(systemName: isFolder ? "folder" : "text.quote")
                .resizable()
                .scaledToFit()
                .frame(width: 16, height: 13)
                .foregroundStyle(!isEnabled ? Color(nsColor: .disabledControlTextColor)
                    : isSelected ? Color(nsColor: .alternateSelectedControlTextColor)
                    : isFolder ? .accentColor : .primary)
                .accessibilityHidden(true)
            TextField("", text: $draft)
                .textFieldStyle(.plain)
                .focused($isEditing)
                .foregroundStyle(!isEnabled ? Color(nsColor: .disabledControlTextColor)
                    : isSelected && !isEditing ? Color(nsColor: .alternateSelectedControlTextColor) : .primary)
                .onSubmit(commitRename)
                .onChange(of: isEditing) {
                    if !isEditing { commitRename() }
                }
                .onExitCommand {
                    draft = savedTitle
                    isEditing = false
                }
        }
        .font(.system(size: 14))
        .padding(.leading, isFolder ? 5 : 0)
        .padding(.trailing, 2)
        .frame(maxWidth: .infinity, maxHeight: .infinity, alignment: .leading)
    }

    private func commitRename() {
        guard !draft.isEmpty else {
            draft = savedTitle
            return
        }
        if draft != savedTitle {
            savedTitle = draft
            onRename(draft)
        }
    }
}

/// The outline view owns selection and dragging; the hosted view owns row content.
private final class SnippetHostingCell: NSTableCellView {
    private let hostingView: NSHostingView<SnippetRow>

    override var backgroundStyle: NSView.BackgroundStyle {
        didSet { hostingView.rootView.isSelected = backgroundStyle == .emphasized }
    }

    init(title: String, isFolder: Bool, isEnabled: Bool, onRename: @escaping (String) -> Void) {
        hostingView = NSHostingView(rootView: SnippetRow(title: title, isFolder: isFolder, isEnabled: isEnabled, onRename: onRename))
        super.init(frame: .zero)
        hostingView.translatesAutoresizingMaskIntoConstraints = false
        addSubview(hostingView)
        NSLayoutConstraint.activate([
            hostingView.leadingAnchor.constraint(equalTo: leadingAnchor),
            hostingView.trailingAnchor.constraint(equalTo: trailingAnchor),
            hostingView.topAnchor.constraint(equalTo: topAnchor),
            hostingView.bottomAnchor.constraint(equalTo: bottomAnchor)
        ])
    }

    @available(*, unavailable)
    required init?(coder: NSCoder) {
        fatalError("init(coder:) has not been implemented")
    }
}

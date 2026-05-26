//
//  CPYSnippetsEditorWindowController.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2016/05/18.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Cocoa
import Dependencies
import KeyHolder
import Magnet
import AEXML

final class CPYSnippetsEditorWindowController: NSWindowController {

    // MARK: - Properties
    static let sharedController = CPYSnippetsEditorWindowController(windowNibName: "CPYSnippetsEditorWindowController")
    @IBOutlet private weak var splitView: CPYSplitView!
    @IBOutlet private weak var folderSettingView: NSView!
    @IBOutlet private weak var folderTitleTextField: NSTextField!
    @IBOutlet private weak var folderShortcutRecordView: RecordView! {
        didSet {
            folderShortcutRecordView.delegate = self
        }
    }
    @IBOutlet private var textView: CPYPlaceHolderTextView! {
        didSet {
            textView.font = NSFont.systemFont(ofSize: 14)
            textView.isAutomaticQuoteSubstitutionEnabled = false
            textView.enabledTextCheckingTypes = 0
            textView.isRichText = false
            textView.placeHolderText = String(localized: "Please fill in the contents of the snippet")
        }
    }
    @IBOutlet private weak var outlineView: NSOutlineView! {
        didSet {
            // Enable Drag and Drop
            outlineView.registerForDraggedTypes([NSPasteboard.PasteboardType(rawValue: Constants.Common.draggedDataType)])
        }
    }

    @Dependency(\.snippetRepository)
    private var snippetRepository
    private var folders = [EditorSnippetFolder]()
    private var selectedFolder: EditorSnippetFolder? {
        guard let item = outlineView.item(atRow: outlineView.selectedRow) else { return nil }
        return item as? EditorSnippetFolder ?? outlineView.parent(forItem: item) as? EditorSnippetFolder
    }

    // MARK: - Window Life Cycle
    override func windowDidLoad() {
        super.windowDidLoad()
        self.window?.collectionBehavior = NSWindow.CollectionBehavior.canJoinAllSpaces
        self.window?.backgroundColor = NSColor(white: 0.99, alpha: 1)
        if #available(OSX 10.10, *) {
            self.window?.titlebarAppearsTransparent = true
        }
        folders = snippetRepository.fetchFolderDetails().map(EditorSnippetFolder.init)
        outlineView.reloadData()
        // Select first folder
        if let folder = folders.first {
            outlineView.selectRowIndexes(IndexSet(integer: outlineView.row(forItem: folder)), byExtendingSelection: false)
            changeItemFocus()
        }
    }

    override func showWindow(_ sender: Any?) {
        super.showWindow(sender)
        window?.makeKeyAndOrderFront(self)
    }
}

// MARK: - IBActions
extension CPYSnippetsEditorWindowController {
    @IBAction private func addSnippetButtonTapped(_ sender: AnyObject) {
        guard let folder = selectedFolder, let snippet = snippetRepository.insertSnippet(to: folder.id) else {
            NSSound.beep()
            return
        }
        let editorSnippet = EditorSnippet(snippet: snippet)
        folder.snippets.append(editorSnippet)
        outlineView.reloadData()
        outlineView.expandItem(folder)
        outlineView.selectRowIndexes(IndexSet(integer: outlineView.row(forItem: editorSnippet)), byExtendingSelection: false)
        changeItemFocus()
    }

    @IBAction private func addFolderButtonTapped(_ sender: AnyObject) {
        guard let folder = snippetRepository.insertFolder() else {
            NSSound.beep()
            return
        }
        let editorFolder = EditorSnippetFolder(folder: folder)
        folders.append(editorFolder)
        outlineView.reloadData()
        outlineView.selectRowIndexes(IndexSet(integer: outlineView.row(forItem: editorFolder)), byExtendingSelection: false)
        changeItemFocus()
    }

    @IBAction private func deleteButtonTapped(_ sender: AnyObject) {
        guard let item = outlineView.item(atRow: outlineView.selectedRow) else {
            NSSound.beep()
            return
        }

        let alert = NSAlert()
        alert.messageText = String(localized: "Delete Item")
        alert.informativeText = String(localized: "Are you sure want to delete this item?")
        alert.addButton(withTitle: String(localized: "Delete Item"))
        alert.addButton(withTitle: String(localized: "Cancel"))
        NSApp.activate(ignoringOtherApps: true)
        let result = alert.runModal()
        if result != NSApplication.ModalResponse.alertFirstButtonReturn { return }

        if let folder = item as? EditorSnippetFolder {
            folders.removeAll(where: { $0.id == folder.id })
            snippetRepository.deleteFolder(folder.id)
            AppEnvironment.current.hotKeyService.unregisterSnippetHotKey(with: folder.id.uuidString)
        } else if let snippet = item as? EditorSnippet, let folder = outlineView.parent(forItem: item) as? EditorSnippetFolder {
            folder.snippets.removeAll(where: { $0.id == snippet.id })
            snippetRepository.deleteSnippet(snippet.id)
        }
        outlineView.reloadData()
        changeItemFocus()
    }

    @IBAction private func changeStatusButtonTapped(_ sender: AnyObject) {
        guard let item = outlineView.item(atRow: outlineView.selectedRow) else {
            NSSound.beep()
            return
        }
        if let folder = item as? EditorSnippetFolder {
            folder.isEnabled.toggle()
            snippetRepository.updateFolderIsEnabled(folder.id, isEnabled: folder.isEnabled)
        } else if let snippet = item as? EditorSnippet {
            snippet.isEnabled.toggle()
            snippetRepository.updateSnippetIsEnabled(snippet.id, isEnabled: snippet.isEnabled)
        }
        outlineView.reloadData()
        changeItemFocus()
    }

    @IBAction private func importSnippetButtonTapped(_ sender: AnyObject) {
        let panel = NSOpenPanel()
        panel.allowsMultipleSelection = false
        panel.directoryURL = URL(fileURLWithPath: NSHomeDirectory())
        panel.allowedFileTypes = [Constants.Xml.fileType]
        let returnCode = panel.runModal()

        if returnCode != NSApplication.ModalResponse.OK { return }

        let fileURLs = panel.urls
        guard let url = fileURLs.first else { return }
        guard let data = try? Data(contentsOf: url) else { return }

        do {
            var options = AEXMLOptions()
            options.parserSettings.shouldTrimWhitespace = false
            let xmlDocument = try AEXMLDocument(xml: data, options: options)
            let folders = xmlDocument[Constants.Xml.rootElement]
                .children
                .map { folderElement in
                    let title = folderElement[Constants.Xml.titleElement].value ?? "untitled folder"
                    let snippets = folderElement[Constants.Xml.snippetsElement][Constants.Xml.snippetElement]
                        .all?
                        .map { (title: $0[Constants.Xml.titleElement].value ?? "untitled snippet", content: $0[Constants.Xml.contentElement].value ?? "") } ?? []
                    return (title: title, snippets: snippets)
                }
            guard let folderDetails = snippetRepository.insertFolders(folders) else {
                NSSound.beep()
                return
            }
            self.folders.append(contentsOf: folderDetails.map(EditorSnippetFolder.init))
            outlineView.reloadData()
        } catch {
            NSSound.beep()
        }
    }

    @IBAction private func exportSnippetButtonTapped(_ sender: AnyObject) {
        let xmlDocument = AEXMLDocument()
        let rootElement = xmlDocument.addChild(name: Constants.Xml.rootElement)

        folders.forEach { folder in
            let folderElement = rootElement.addChild(name: Constants.Xml.folderElement)

            folderElement.addChild(name: Constants.Xml.titleElement, value: folder.title)

            let snippetsElement = folderElement.addChild(name: Constants.Xml.snippetsElement)
            folder.snippets
                .forEach { snippet in
                    let snippetElement = snippetsElement.addChild(name: Constants.Xml.snippetElement)
                    snippetElement.addChild(name: Constants.Xml.titleElement, value: snippet.title)
                    snippetElement.addChild(name: Constants.Xml.contentElement, value: snippet.content)
                }
        }

        let panel = NSSavePanel()
        panel.accessoryView = nil
        panel.canSelectHiddenExtension = true
        panel.allowedFileTypes = [Constants.Xml.fileType]
        panel.allowsOtherFileTypes = false
        panel.directoryURL = URL(fileURLWithPath: NSHomeDirectory())
        panel.nameFieldStringValue = "snippets"
        let returnCode = panel.runModal()

        if returnCode != NSApplication.ModalResponse.OK { return }

        guard let data = xmlDocument.xml.data(using: String.Encoding.utf8) else { return }
        guard let url = panel.url else { return }

        do {
            try data.write(to: url, options: .atomic)
        } catch {
            NSSound.beep()
        }
    }
}

// MARK: - Item Selected
private extension CPYSnippetsEditorWindowController {
    func changeItemFocus() {
        // Reset TextView Undo/Redo history
        textView.undoManager?.removeAllActions()
        guard let item = outlineView.item(atRow: outlineView.selectedRow) else {
            folderSettingView.isHidden = true
            textView.isHidden = true
            folderShortcutRecordView.keyCombo = nil
            folderTitleTextField.stringValue = ""
            return
        }
        if let folder = item as? EditorSnippetFolder {
            textView.string = ""
            folderTitleTextField.stringValue = folder.title
            folderShortcutRecordView.keyCombo = AppEnvironment.current.hotKeyService.snippetKeyCombo(forIdentifier: folder.id.uuidString)
            folderSettingView.isHidden = false
            textView.isHidden = true
        } else if let snippet = item as? EditorSnippet {
            textView.string = snippet.content
            folderTitleTextField.stringValue = ""
            folderShortcutRecordView.keyCombo = nil
            folderSettingView.isHidden = true
            textView.isHidden = false
        }
    }
}

// MARK: - NSSplitView Delegate
extension CPYSnippetsEditorWindowController: NSSplitViewDelegate {
    func splitView(_ splitView: NSSplitView, constrainMinCoordinate proposedMinimumPosition: CGFloat, ofSubviewAt dividerIndex: Int) -> CGFloat {
        proposedMinimumPosition + 150
    }

    func splitView(_ splitView: NSSplitView, constrainMaxCoordinate proposedMaximumPosition: CGFloat, ofSubviewAt dividerIndex: Int) -> CGFloat {
        proposedMaximumPosition / 2
    }
}

// MARK: - NSOutlineView DataSource
extension CPYSnippetsEditorWindowController: NSOutlineViewDataSource {
    func outlineView(_ outlineView: NSOutlineView, numberOfChildrenOfItem item: Any?) -> Int {
        if item == nil {
            return folders.count
        } else if let folder = item as? EditorSnippetFolder {
            return folder.snippets.count
        }
        return 0
    }

    func outlineView(_ outlineView: NSOutlineView, isItemExpandable item: Any) -> Bool {
        (item as? EditorSnippetFolder).map { !$0.snippets.isEmpty } ?? false
    }

    func outlineView(_ outlineView: NSOutlineView, child index: Int, ofItem item: Any?) -> Any {
        (item as? EditorSnippetFolder).map { $0.snippets[index] as Any } ?? folders[index] as Any
    }

    func outlineView(_ outlineView: NSOutlineView, objectValueFor tableColumn: NSTableColumn?, byItem item: Any?) -> Any? {
        (item as? EditorSnippetFolder).map { $0.title } ?? (item as? EditorSnippet).map { $0.title } ?? ""
    }

    // MARK: - Drag and Drop
    func outlineView(_ outlineView: NSOutlineView, pasteboardWriterForItem item: Any) -> NSPasteboardWriting? {
        let pasteboardItem = NSPasteboardItem()
        if let folder = item as? EditorSnippetFolder, let index = folders.firstIndex(where: { $0.id == folder.id }) {
            let draggedData = DraggedData(type: .folder, folderID: folder.id, snippetID: nil, index: index)
            guard let data = try? NSKeyedArchiver.archivedData(withRootObject: draggedData, requiringSecureCoding: true) else { return nil }
            pasteboardItem.setData(data, forType: NSPasteboard.PasteboardType(rawValue: Constants.Common.draggedDataType))
        } else if let snippet = item as? EditorSnippet, let folder = outlineView.parent(forItem: snippet) as? EditorSnippetFolder {
            guard let index = folder.snippets.firstIndex(where: { $0.id == snippet.id }) else { return nil }
            let draggedData = DraggedData(type: .snippet, folderID: folder.id, snippetID: snippet.id, index: index)
            guard let data = try? NSKeyedArchiver.archivedData(withRootObject: draggedData, requiringSecureCoding: true) else { return nil }
            pasteboardItem.setData(data, forType: NSPasteboard.PasteboardType(rawValue: Constants.Common.draggedDataType))
        } else {
            return nil
        }
        return pasteboardItem
    }

    func outlineView(_ outlineView: NSOutlineView, validateDrop info: NSDraggingInfo, proposedItem item: Any?, proposedChildIndex index: Int) -> NSDragOperation {
        let pasteboard = info.draggingPasteboard
        guard let data = pasteboard.data(forType: NSPasteboard.PasteboardType(rawValue: Constants.Common.draggedDataType)) else { return NSDragOperation() }
        guard let draggedData = try? NSKeyedUnarchiver.unarchivedObject(ofClasses: [DraggedData.self, NSUUID.self], from: data) as? DraggedData else { return NSDragOperation() }

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
        let pasteboard = info.draggingPasteboard
        guard let data = pasteboard.data(forType: NSPasteboard.PasteboardType(rawValue: Constants.Common.draggedDataType)) else { return false }
        guard let draggedData = try? NSKeyedUnarchiver.unarchivedObject(ofClasses: [DraggedData.self, NSUUID.self], from: data) as? DraggedData else { return false }

        switch draggedData.type {
        case .folder where index != draggedData.index && index >= 0:
            guard let folder = folders.first(where: { $0.id == draggedData.folderID }) else { return false }
            folders.insert(folder, at: index)
            let removedIndex = (index < draggedData.index) ? draggedData.index + 1 : draggedData.index
            folders.remove(at: removedIndex)
            snippetRepository.updateFolderIndexes(folders.map(\.id))
            outlineView.reloadData()
            outlineView.selectRowIndexes(IndexSet(integer: outlineView.row(forItem: folder)), byExtendingSelection: false)
            changeItemFocus()
            return true
        case .snippet:
            guard let fromFolder = folders.first(where: { $0.id == draggedData.folderID }) else { return false }
            guard let toFolder = item as? EditorSnippetFolder else { return false }
            guard let snippet = fromFolder.snippets.first(where: { $0.id == draggedData.snippetID }) else { return false }

            if draggedData.folderID == toFolder.id {
                guard index >= 0, index != draggedData.index else { return false }
                // Move to same folder
                fromFolder.snippets.insert(snippet, at: index)
                let removedIndex = (index < draggedData.index) ? draggedData.index + 1 : draggedData.index
                fromFolder.snippets.remove(at: removedIndex)
                snippetRepository.updateSnippetIndexes(fromFolder.snippets.map(\.id))
                outlineView.reloadData()
                outlineView.selectRowIndexes(NSIndexSet(index: outlineView.row(forItem: snippet)) as IndexSet, byExtendingSelection: false)
                changeItemFocus()
                return true
            } else {
                // Move to other folder
                let index = max(0, index)
                toFolder.snippets.insert(snippet, at: index)
                fromFolder.snippets.removeAll(where: { $0.id == snippet.id })
                snippetRepository.moveSnippet(snippet.id, to: toFolder.id, snippetIDs: toFolder.snippets.map(\.id))
                outlineView.reloadData()
                outlineView.expandItem(toFolder)
                outlineView.selectRowIndexes(NSIndexSet(index: outlineView.row(forItem: snippet)) as IndexSet, byExtendingSelection: false)
                changeItemFocus()
                return true
            }
        default:
            return false
        }
    }
}

// MARK: - NSOutlineView Delegate
extension CPYSnippetsEditorWindowController: NSOutlineViewDelegate {
    func outlineView(_ outlineView: NSOutlineView, willDisplayCell cell: Any, for tableColumn: NSTableColumn?, item: Any) {
        guard let cell = cell as? CPYSnippetsEditorCell else { return }
        if let folder = item as? EditorSnippetFolder {
            cell.iconType = .folder
            cell.isItemEnabled = folder.isEnabled
        } else if let snippet = item as? EditorSnippet {
            cell.iconType = .none
            cell.isItemEnabled = snippet.isEnabled
        }
    }

    func outlineViewSelectionDidChange(_ notification: Notification) {
        changeItemFocus()
    }

    func control(_ control: NSControl, textShouldEndEditing fieldEditor: NSText) -> Bool {
        let text = fieldEditor.string
        guard !text.isEmpty else { return false }
        guard let outlineView = control as? NSOutlineView else { return false }
        guard let item = outlineView.item(atRow: outlineView.selectedRow) else { return false }
        if let folder = item as? EditorSnippetFolder {
            folder.title = text
            snippetRepository.updateFolderTitle(folder.id, title: text)
        } else if let snippet = item as? EditorSnippet {
            snippet.title = text
            snippetRepository.updateSnippetTitle(snippet.id, title: text)
        }
        changeItemFocus()
        return true
    }
}

// MARK: - NSTextView Delegate
extension CPYSnippetsEditorWindowController: NSTextViewDelegate {
    func textView(_ textView: NSTextView, shouldChangeTextIn affectedCharRange: NSRange, replacementString: String?) -> Bool {
        guard let replacementString = replacementString else { return false }
        guard let snippet = outlineView.item(atRow: outlineView.selectedRow) as? EditorSnippet else { return false }

        let string = (textView.string as NSString).replacingCharacters(in: affectedCharRange, with: replacementString)
        snippet.content = string
        snippetRepository.updateSnippetContent(snippet.id, content: string)

        return true
    }
}

// MARK: - RecordView Delegate
extension CPYSnippetsEditorWindowController: RecordViewDelegate {
    func recordViewShouldBeginRecording(_ recordView: RecordView) -> Bool {
        guard selectedFolder != nil else { return false }
        return true
    }

    func recordView(_ recordView: RecordView, canRecordKeyCombo keyCombo: KeyCombo) -> Bool {
        guard selectedFolder != nil else { return false }
        return true
    }

    func recordView(_ recordView: RecordView, didChangeKeyCombo keyCombo: KeyCombo?) {
        guard let selectedFolder = selectedFolder else { return }
        guard let keyCombo = keyCombo else {
            AppEnvironment.current.hotKeyService.unregisterSnippetHotKey(with: selectedFolder.id.uuidString)
            return
        }
        AppEnvironment.current.hotKeyService.registerSnippetHotKey(with: selectedFolder.id.uuidString, keyCombo: keyCombo)
    }

    func recordViewDidEndRecording(_ recordView: RecordView) {}
}

// MARK: - Objects
/// Snippet editor objects used only by the snippets editor outline view.
///
/// `NSOutlineView` infers visual state such as expansion and selection from item object
/// identity, so using SQLiteData table values directly can cause visual updates to be
/// treated as different items after reloads. Keep dedicated `NSObject` wrappers for this
/// screen so the outline view can maintain its UI state while the database remains table-based.
private final class EditorSnippetFolder: NSObject {
    let id: SnippetFolder.ID
    var title: String
    var index: Int
    var isEnabled: Bool
    var snippets: [EditorSnippet]

    init(folderDetail: SnippetFolderDetail) {
        self.id = folderDetail.folder.id
        self.title = folderDetail.folder.title
        self.index = folderDetail.folder.index
        self.isEnabled = folderDetail.folder.isEnabled
        self.snippets = folderDetail.snippets.map(EditorSnippet.init)
        super.init()
    }

    init(folder: SnippetFolder) {
        self.id = folder.id
        self.title = folder.title
        self.index = folder.index
        self.isEnabled = folder.isEnabled
        self.snippets = []
        super.init()
    }
}

private final class EditorSnippet: NSObject {
    let id: Snippet.ID
    var folderID: SnippetFolder.ID
    var title: String
    var content: String
    var index: Int
    var isEnabled: Bool

    init(snippet: Snippet) {
        self.id = snippet.id
        self.folderID = snippet.folderID
        self.title = snippet.title
        self.content = snippet.content
        self.index = snippet.index
        self.isEnabled = snippet.isEnabled
        super.init()
    }
}

//
//  CPYSnippetsEditorWindowController.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/05/18.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import RealmSwift
import KeyHolder
import Magnet
import AEXML

final class CPYSnippetsEditorWindowController: NSWindowController {

    // MARK: - Properties
    static let sharedController = CPYSnippetsEditorWindowController(windowNibName: "CPYSnippetsEditorWindowController")
    @IBOutlet weak var splitView: CPYSplitView!
    @IBOutlet weak var folderSettingView: NSView!
    @IBOutlet weak var folderTitleTextField: NSTextField!
    @IBOutlet weak var folderShortcutRecordView: RecordView! {
        didSet {
            folderShortcutRecordView.delegate = self
        }
    }
    @IBOutlet var textView: CPYPlaceHolderTextView! {
        didSet {
            textView.font = NSFont.systemFont(ofSize: 14)
            textView.isAutomaticQuoteSubstitutionEnabled = false
            textView.enabledTextCheckingTypes = 0
            textView.isRichText = false
        }
    }
    @IBOutlet weak var outlineView: NSOutlineView! {
        didSet {
            // Enable Drag and Drop
            outlineView.register(forDraggedTypes: [Constants.Common.draggedDataType])
        }
    }

    fileprivate let defaults = UserDefaults.standard
    fileprivate var folders = [CPYFolder]()
    fileprivate var selectedSnippet: CPYSnippet? {
        guard let snippet = outlineView.item(atRow: outlineView.selectedRow) as? CPYSnippet else { return nil }
        return snippet
    }
    fileprivate var selectedFolder: CPYFolder? {
        guard let item = outlineView.item(atRow: outlineView.selectedRow) else { return nil }
        if let folder = outlineView.parent(forItem: item) as? CPYFolder {
            return folder
        } else if let folder = item as? CPYFolder {
            return folder
        }
        return nil
    }

    // MARK: - Window Life Cycle
    override func windowDidLoad() {
        super.windowDidLoad()
        self.window?.collectionBehavior = .canJoinAllSpaces
        self.window?.backgroundColor = NSColor(white: 0.99, alpha: 1)
        if #available(OSX 10.10, *) {
            self.window?.titlebarAppearsTransparent = true
        }
        // HACK: Copy as an object that does not put under Realm management.
        // https://github.com/realm/realm-cocoa/issues/1734
        let realm = try! Realm()
        folders = realm.objects(CPYFolder.self)
                    .sorted(byProperty: "index", ascending: true)
                    .map { $0.deepCopy() }
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
    @IBAction func addSnippetButtonTapped(_ sender: AnyObject) {
        guard let folder = selectedFolder else {
            NSBeep()
            return
        }
        let snippet = folder.createSnippet()
        folder.snippets.append(snippet)
        folder.mergeSnippet(snippet)
        outlineView.reloadData()
        outlineView.expandItem(folder)
        outlineView.selectRowIndexes(IndexSet(integer: outlineView.row(forItem: snippet)), byExtendingSelection: false)
        changeItemFocus()
    }

    @IBAction func addFolderButtonTapped(_ sender: AnyObject) {
        let folder = CPYFolder.create()
        folders.append(folder)
        folder.merge()
        outlineView.reloadData()
        outlineView.selectRowIndexes(IndexSet(integer: outlineView.row(forItem: folder)), byExtendingSelection: false)
        changeItemFocus()
    }

    @IBAction func deleteButtonTapped(_ sender: AnyObject) {
        guard let item = outlineView.item(atRow: outlineView.selectedRow) else {
            NSBeep()
            return
        }

        let alert = NSAlert()
        alert.messageText = LocalizedString.DeleteItem.value
        alert.informativeText = LocalizedString.ConfirmDeleteItem.value
        alert.addButton(withTitle: LocalizedString.DeleteItem.value)
        alert.addButton(withTitle: LocalizedString.Cancel.value)
        NSApp.activate(ignoringOtherApps: true)
        let result = alert.runModal()
        if result != NSAlertFirstButtonReturn { return }

        if let folder = item as? CPYFolder {
            folders.removeObject(folder)
            folder.remove()
            HotKeyManager.sharedManager.removeFolderHotKey(folder.identifier)
        } else if let snippet = item as? CPYSnippet, let folder = outlineView.parent(forItem: item) as? CPYFolder, let index = folder.snippets.index(of: snippet) {
            folder.snippets.remove(objectAtIndex: index)
            snippet.remove()
        }
        outlineView.reloadData()
        changeItemFocus()
    }

    @IBAction func changeStatusButtonTapped(_ sender: AnyObject) {
        guard let item = outlineView.item(atRow: outlineView.selectedRow) else {
            NSBeep()
            return
        }
        if let folder = item as? CPYFolder {
            folder.enable = !folder.enable
            folder.merge()
        } else if let snippet = item as? CPYSnippet {
            snippet.enable = !snippet.enable
            snippet.merge()
        }
        outlineView.reloadData()
        changeItemFocus()
    }

    @IBAction func importSnippetButtonTapped(_ sender: AnyObject) {
        let panel = NSOpenPanel()
        panel.allowsMultipleSelection = false
        panel.directoryURL = URL(fileURLWithPath: NSHomeDirectory())
        panel.allowedFileTypes = [Constants.Xml.fileType]
        let returnCode = panel.runModal()

        if returnCode != NSModalResponseOK { return }

        let fileURLs = panel.urls
        guard let url = fileURLs.first else { return }
        guard let data = try? Data(contentsOf: url) else { return }

        do {
            let realm = try! Realm()
            let lastFolder = realm.objects(CPYFolder.self).sorted(byProperty: "index", ascending: true).last
            var folderIndex = (lastFolder?.index ?? -1) + 1
            // Create Document
            let xmlDocument = try AEXMLDocument(xml: data)
            xmlDocument[Constants.Xml.rootElement]
                .children
                .forEach { folderElement in
                    let folder = CPYFolder()
                    // Title
                    folder.title = folderElement[Constants.Xml.titleElement].value ?? "untitled folder"
                    // Index
                    folder.index = folderIndex
                    // Sync DB
                    realm.transaction { realm.add(folder) }
                    // Snippet
                    var snippetIndex = 0
                    folderElement[Constants.Xml.snippetsElement][Constants.Xml.snippetElement]
                        .all?
                        .forEach { snippetElement in
                            let snippet = CPYSnippet()
                            snippet.title = snippetElement[Constants.Xml.titleElement].value ?? "untitled snippet"
                            snippet.content = snippetElement[Constants.Xml.contentElement].value ?? ""
                            snippet.index = snippetIndex
                            realm.transaction { folder.snippets.append(snippet) }
                            // Increment snippet index
                            snippetIndex += 1
                        }
                    // Increment folder index
                    folderIndex += 1
                    // Add folder
                    let copyFolder = folder.deepCopy()
                    folders.append(copyFolder)
                }
            outlineView.reloadData()
        } catch {
            NSBeep()
        }
    }

    @IBAction func exportSnippetButtonTapped(_ sender: AnyObject) {
        let xmlDocument = AEXMLDocument()
        let rootElement = xmlDocument.addChild(name: Constants.Xml.rootElement)

        let realm = try! Realm()
        let folders = realm.objects(CPYFolder.self).sorted(byProperty: "index", ascending: true)
        folders.forEach { folder in
            let folderElement = rootElement.addChild(name: Constants.Xml.folderElement)

            folderElement.addChild(name: Constants.Xml.titleElement, value: folder.title)

            let snippetsElement = folderElement.addChild(name: Constants.Xml.snippetsElement)
            folder.snippets
                .sorted(byProperty: "index", ascending: true)
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

        if returnCode != NSModalResponseOK { return }

        guard let data = xmlDocument.xml.data(using: String.Encoding.utf8) else { return }
        guard let url = panel.url else { return }

        do {
            try data.write(to: url, options: .atomic)
        } catch {
            NSBeep()
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
        if let folder = item as? CPYFolder {
            textView.string = ""
            folderTitleTextField.stringValue = folder.title
            folderShortcutRecordView.keyCombo = HotKeyManager.sharedManager.folderKeyCombo(folder.identifier)
            folderSettingView.isHidden = false
            textView.isHidden = true
        } else if let snippet = item as? CPYSnippet {
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
        return proposedMinimumPosition + 150
    }

    func splitView(_ splitView: NSSplitView, constrainMaxCoordinate proposedMaximumPosition: CGFloat, ofSubviewAt dividerIndex: Int) -> CGFloat {
        return proposedMaximumPosition / 2
    }
}

// MARK: - NSOutlineView DataSource
extension CPYSnippetsEditorWindowController: NSOutlineViewDataSource {
    func outlineView(_ outlineView: NSOutlineView, numberOfChildrenOfItem item: Any?) -> Int {
        if item == nil {
            return Int(folders.count)
        } else if let folder = item as? CPYFolder {
            return Int(folder.snippets.count)
        }
        return 0
    }

    func outlineView(_ outlineView: NSOutlineView, isItemExpandable item: Any) -> Bool {
        if let folder = item as? CPYFolder {
            return (folder.snippets.count != 0)
        }
        return false
    }

    func outlineView(_ outlineView: NSOutlineView, child index: Int, ofItem item: Any?) -> Any {
        if item == nil {
            return folders[index]
        } else if let folder = item as? CPYFolder {
            return folder.snippets[index]
        }
        return ""
    }

    func outlineView(_ outlineView: NSOutlineView, objectValueFor tableColumn: NSTableColumn?, byItem item: Any?) -> Any? {
        if let folder = item as? CPYFolder {
            return folder.title
        } else if let snippet = item as? CPYSnippet {
            return snippet.title
        }
        return ""
    }

    // MARK: - Drag and Drop
    func outlineView(_ outlineView: NSOutlineView, pasteboardWriterForItem item: Any) -> NSPasteboardWriting? {
        let pasteboardItem = NSPasteboardItem()
        if let folder = item as? CPYFolder, let index = folders.index(of: folder) {
            let draggedData = CPYDraggedData(type: .folder, folderIdentifier: folder.identifier, snippetIdentifier: nil, index: index)
            let data = NSKeyedArchiver.archivedData(withRootObject: draggedData)
            pasteboardItem.setData(data, forType: Constants.Common.draggedDataType)
        } else if let snippet = item as? CPYSnippet, let folder = outlineView.parent(forItem: snippet) as? CPYFolder {
            guard let index = folder.snippets.index(of: snippet) else { return nil }
            let draggedData = CPYDraggedData(type: .snippet, folderIdentifier: folder.identifier, snippetIdentifier: snippet.identifier, index: Int(index))
            let data = NSKeyedArchiver.archivedData(withRootObject: draggedData)
            pasteboardItem.setData(data, forType: Constants.Common.draggedDataType)
        } else {
            return nil
        }
        return pasteboardItem
    }

    func outlineView(_ outlineView: NSOutlineView, validateDrop info: NSDraggingInfo, proposedItem item: Any?, proposedChildIndex index: Int) -> NSDragOperation {
        if index < 0 { return NSDragOperation() }
        let pasteboard = info.draggingPasteboard()
        guard let data = pasteboard.data(forType: Constants.Common.draggedDataType) else { return NSDragOperation() }
        guard let draggedData = NSKeyedUnarchiver.unarchiveObject(with: data) as? CPYDraggedData else { return NSDragOperation() }

        switch draggedData.type {
        case .folder where item == nil:
            return .move
        case .snippet where item is CPYFolder:
            return .move
        default:
            return NSDragOperation()
        }
    }

    func outlineView(_ outlineView: NSOutlineView, acceptDrop info: NSDraggingInfo, item: Any?, childIndex index: Int) -> Bool {
        if index < 0 { return false  }
        let pasteboard = info.draggingPasteboard()
        guard let data = pasteboard.data(forType: Constants.Common.draggedDataType) else { return false }
        guard let draggedData = NSKeyedUnarchiver.unarchiveObject(with: data) as? CPYDraggedData else { return false }

        switch draggedData.type {
        case .folder where index != draggedData.index:
            guard let folder = folders.filter({ $0.identifier == draggedData.folderIdentifier }).first else { return false }
            folders.insert(folder, at: index)
            let removedIndex = (index < draggedData.index) ? draggedData.index + 1 : draggedData.index
            folders.remove(at: removedIndex)
            outlineView.reloadData()
            outlineView.selectRowIndexes(IndexSet(integer: outlineView.row(forItem: folder)), byExtendingSelection: false)
            CPYFolder.rearrangesIndex(folders)
            changeItemFocus()
            return true
        case .snippet:
            guard let fromFolder = folders.filter({ $0.identifier == draggedData.folderIdentifier }).first else { return false }
            guard let toFolder = item as? CPYFolder else { return false }
            guard let snippet = fromFolder.snippets.filter({ $0.identifier == draggedData.snippetIdentifier }).first else { return false }

            if fromFolder.identifier == toFolder.identifier {
                if index == draggedData.index { return false }
                // Move to same folder
                fromFolder.snippets.insert(snippet, at: index)
                let removedIndex = (index < draggedData.index) ? draggedData.index + 1 : draggedData.index
                fromFolder.snippets.remove(objectAtIndex: removedIndex)
                outlineView.reloadData()
                outlineView.selectRowIndexes(NSIndexSet(index: outlineView.row(forItem: snippet)) as IndexSet, byExtendingSelection: false)
                fromFolder.rearrangesSnippetIndex()
                changeItemFocus()
                return true
            } else {
                // Move to other folder
                toFolder.snippets.insert(snippet, at: index)
                fromFolder.snippets.remove(objectAtIndex: draggedData.index)
                outlineView.reloadData()
                outlineView.expandItem(toFolder)
                outlineView.selectRowIndexes(NSIndexSet(index: outlineView.row(forItem: snippet)) as IndexSet, byExtendingSelection: false)
                toFolder.insertSnippet(snippet, index: index)
                fromFolder.removeSnippet(snippet)
                changeItemFocus()
                return true
            }
        default: return false
        }
    }
}

// MARK: - NSOutlineView Delegate
extension CPYSnippetsEditorWindowController: NSOutlineViewDelegate {
    func outlineView(_ outlineView: NSOutlineView, willDisplayCell cell: Any, for tableColumn: NSTableColumn?, item: Any) {
        guard let cell = cell as? CPYSnippetsEditorCell else { return }
        if let folder = item as? CPYFolder {
            cell.iconType = .folder
            cell.isItemEnabled = folder.enable
        } else if let snippet = item as? CPYSnippet {
            cell.iconType = .none
            cell.isItemEnabled = snippet.enable
        }
    }

    func outlineViewSelectionDidChange(_ notification: Notification) {
        changeItemFocus()
    }

    func control(_ control: NSControl, textShouldEndEditing fieldEditor: NSText) -> Bool {
        guard let text = fieldEditor.string, text.characters.count != 0 else { return false }
        guard let outlineView = control as? NSOutlineView else { return false }
        guard let item = outlineView.item(atRow: outlineView.selectedRow) else { return false }
        if let folder = item as? CPYFolder {
            folder.title = text
            folder.merge()
        } else if let snippet = item as? CPYSnippet {
            snippet.title = text
            snippet.merge()
        }
        changeItemFocus()
        return true
    }
}

// MARK: - NSTextView Delegate
extension CPYSnippetsEditorWindowController: NSTextViewDelegate {
    func textView(_ textView: NSTextView, shouldChangeTextIn affectedCharRange: NSRange, replacementString: String?) -> Bool {
        guard let replacementString = replacementString else { return false }
        guard let text = textView.string else { return false }
        guard let snippet = selectedSnippet else { return false }
        let string = (text as NSString).replacingCharacters(in: affectedCharRange, with: replacementString)
        snippet.content = string
        snippet.merge()
        return true
    }
}

// MARK: - RecordView Delegate
extension CPYSnippetsEditorWindowController: RecordViewDelegate {
    func recordViewShouldBeginRecording(_ recordView: RecordView) -> Bool {
        guard let _ = selectedFolder else { return false }
        return true
    }

    func recordView(_ recordView: RecordView, canRecordKeyCombo keyCombo: KeyCombo) -> Bool {
        guard let _ = selectedFolder else { return false }
        return true
    }

    func recordViewDidClearShortcut(_ recordView: RecordView) {
        guard let selectedFolder = selectedFolder else { return }
        HotKeyManager.sharedManager.removeFolderHotKey(selectedFolder.identifier)
    }

    func recordView(_ recordView: RecordView, didChangeKeyCombo keyCombo: KeyCombo) {
        guard let selectedFolder = selectedFolder else { return }
        HotKeyManager.sharedManager.addFolderHotKey(selectedFolder.identifier, keyCombo: keyCombo)
    }

    func recordViewDidEndRecording(_ recordView: RecordView) {}
}

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
            outlineView.allowsMultipleSelection = true
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
    fileprivate var selectedItems: [Any] {
        return outlineView.selectedRowIndexes.flatMap({ outlineView.item(atRow: $0.hashValue ) })
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
                    .sorted(byKeyPath: #keyPath(CPYFolder.index), ascending: true)
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

    fileprivate func reloadData() {
        outlineView.reloadData()
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
        reloadData()
        outlineView.expandItem(folder)
        outlineView.selectRowIndexes(IndexSet(integer: outlineView.row(forItem: snippet)), byExtendingSelection: false)
        changeItemFocus()
    }

    @IBAction func addFolderButtonTapped(_ sender: AnyObject) {
        let folder = CPYFolder.create()
        folders.append(folder)
        folder.merge()
        reloadData()
        outlineView.selectRowIndexes(IndexSet(integer: outlineView.row(forItem: folder)), byExtendingSelection: false)
        changeItemFocus()
    }

    @IBAction func deleteButtonTapped(_ sender: AnyObject) {
        if !checkSelectedItem() {
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

        deleteItems()
    }

    @IBAction func changeStatusButtonTapped(_ sender: AnyObject) {
        if !checkSelectedItem() {
            return
        }
        changeStatusItems()
    }

    @IBAction func importSnippetButtonTapped(_ sender: AnyObject) {
        importSnippet()
    }

    @IBAction func exportSnippetButtonTapped(_ sender: AnyObject) {
        askForExportSnippet()
    }

    fileprivate func checkSelectedItem() -> Bool {
        if outlineView.selectedRowIndexes.isEmpty {
            NSBeep()
            return false
        }
        return true
    }

    fileprivate func changeStatusItems() {
        for item in selectedItems {
            if let folder = item as? CPYFolder {
                folder.enable = !folder.enable
                folder.merge()
            } else if let snippet = item as? CPYSnippet {
                snippet.enable = !snippet.enable
                snippet.merge()
            }
        }
        reloadData()
        changeItemFocus()
    }

    fileprivate func deleteItems() {
        for item in selectedItems {
            if let folder = item as? CPYFolder {
                folders.removeObject(folder)
                folder.remove()
                HotKeyService.shared.unregisterSnippetHotKey(with: folder.identifier)
            } else if let snippet = item as? CPYSnippet, let folder = outlineView.parent(forItem: item) as? CPYFolder, let indexSnippetInFolder = folder.snippets.index(of: snippet) {
                folder.snippets.remove(objectAtIndex: indexSnippetInFolder)
                snippet.remove()
            }
        }
        reloadData()
        changeItemFocus()
    }

    fileprivate func importSnippet() {
        let panel = NSOpenPanel()
        panel.allowsMultipleSelection = false
        panel.directoryURL = URL(fileURLWithPath: NSHomeDirectory())
        panel.allowedFileTypes = [Constants.ExtensionType.xml.rawValue, Constants.ExtensionType.json.rawValue]
        let returnCode = panel.runModal()

        if returnCode != NSModalResponseOK { return }

        let fileURLs = panel.urls
        guard let url = fileURLs.first else { return }
        guard let data = try? Data(contentsOf: url) else { return }

        DispatchQueue.global().async {
            if url.pathExtension == Constants.ExtensionType.xml.rawValue {
                self.importXMLSnippet(data: data)
            } else if url.pathExtension == Constants.ExtensionType.json.rawValue {
                self.importJSONSnippet(data: data)
            }
        }
    }

    fileprivate func askForExportSnippet() {
        let alert = NSAlert()
        alert.messageText = LocalizedString.Snippet.value
        alert.informativeText = LocalizedString.Preference.value
        alert.addButton(withTitle: Constants.ExtensionType.xml.rawValue)
        alert.addButton(withTitle: Constants.ExtensionType.json.rawValue)
        alert.addButton(withTitle: LocalizedString.Cancel.value)
        NSApp.activate(ignoringOtherApps: true)
        let result = alert.runModal()

        switch result {
        case NSAlertFirstButtonReturn:
            exportSnippet(extensionType: Constants.ExtensionType.xml)
        case NSAlertSecondButtonReturn:
            exportSnippet(extensionType: Constants.ExtensionType.json)
        default:
            return
        }
    }

    fileprivate func exportSnippet(extensionType: Constants.ExtensionType) {
        let panel = NSSavePanel()
        panel.accessoryView = nil
        panel.canSelectHiddenExtension = true
        panel.allowedFileTypes = [extensionType.rawValue]
        panel.allowsOtherFileTypes = false
        panel.directoryURL = URL(fileURLWithPath: NSHomeDirectory())
        panel.nameFieldStringValue = "snippets"
        let returnCode = panel.runModal()

        if returnCode != NSModalResponseOK { return }

        guard let url = panel.url else { return }

        var data: Data?
        switch extensionType {
        case Constants.ExtensionType.xml:
            data = exportXMLSnippet()
        case Constants.ExtensionType.json:
            data = exportJSONSnippet()
        }

        do {
            try data?.write(to: url, options: .atomic)
        } catch {
            NSBeep()
        }
    }

    fileprivate func importXMLSnippet(data: Data) {
        do {
            let realm = try! Realm()
            let lastFolder = realm.objects(CPYFolder.self).sorted(byKeyPath: #keyPath(CPYFolder.index), ascending: true).last
            var folderIndex = (lastFolder?.index ?? -1) + 1

            // Create Document
            let xmlDocument = try AEXMLDocument(xml: data)
            xmlDocument[Constants.ExportFile.rootElement]
                .children
                .forEach { folderElement in
                    let folder = CPYFolder()
                    // Title
                    folder.title = folderElement[Constants.ExportFile.titleElement].value ?? "untitled folder"
                    // Index
                    folder.index = folderIndex
                    // Sync DB
                    realm.transaction { realm.add(folder) }
                    // Snippet
                    var snippetIndex = 0
                    folderElement[Constants.ExportFile.snippetsElement][Constants.ExportFile.snippetElement]
                        .all?
                        .forEach { snippetElement in
                            let snippet = CPYSnippet()
                            snippet.title = snippetElement[Constants.ExportFile.titleElement].value ?? "untitled snippet"
                            snippet.content = snippetElement[Constants.ExportFile.contentElement].value ?? ""
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
            reloadData()
        } catch {
            NSBeep()
        }
    }

    fileprivate func importJSONSnippet(data: Data) {
        let realm = try! Realm()
        let lastFolder = realm.objects(CPYFolder.self).sorted(byKeyPath: #keyPath(CPYFolder.index), ascending: true).last
        var folderIndex = (lastFolder?.index ?? -1) + 1

        //From JSON
        if let dictFromJSON = try? JSONSerialization.jsonObject(with: data, options: []) as? [String:Any] {
            let foldersFromDict = dictFromJSON?[Constants.ExportFile.rootElement] as? [NSDictionary]
            foldersFromDict?.forEach({ folderElement in
                let folder = CPYFolder()
                // Title
                folder.title = folderElement[Constants.ExportFile.titleElement] as? String ?? "untitled folder"
                // Index
                folder.index = folderIndex
                // Sync DB
                realm.transaction { realm.add(folder) }
                // Snippet
                var snippetIndex = 0
                let snippedtFormDict = folderElement[Constants.ExportFile.snippetsElement] as? [NSDictionary]
                snippedtFormDict?.forEach { snippetElement in
                    let snippet = CPYSnippet()
                    snippet.title = snippetElement[Constants.ExportFile.titleElement] as? String ?? "untitled snippet"
                    snippet.content = snippetElement[Constants.ExportFile.contentElement] as? String ?? ""
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
            })
        }
        reloadData()
    }

    fileprivate func exportXMLSnippet() -> Data? {
        let xmlDocument = AEXMLDocument()
        let rootElement = xmlDocument.addChild(name: Constants.ExportFile.rootElement)

        let realm = try! Realm()
        let folders = realm.objects(CPYFolder.self).sorted(byKeyPath: #keyPath(CPYFolder.index), ascending: true)
        folders.forEach { folder in
            let folderElement = rootElement.addChild(name: Constants.ExportFile.folderElement)

            folderElement.addChild(name: Constants.ExportFile.titleElement, value: folder.title)

            let snippetsElement = folderElement.addChild(name: Constants.ExportFile.snippetsElement)
            folder.snippets
                .sorted(byKeyPath: #keyPath(CPYSnippet.index), ascending: true)
                .forEach { snippet in
                    let snippetElement = snippetsElement.addChild(name: Constants.ExportFile.snippetElement)
                    snippetElement.addChild(name: Constants.ExportFile.titleElement, value: snippet.title)
                    snippetElement.addChild(name: Constants.ExportFile.contentElement, value: snippet.content)
            }
        }
        return xmlDocument.xml.data(using: String.Encoding.utf8)
    }

    fileprivate func exportJSONSnippet() -> Data? {
        let realm = try! Realm()
        let folders = realm.objects(CPYFolder.self).sorted(byKeyPath: #keyPath(CPYFolder.index), ascending: true)
        let foldersDict: [NSDictionary] = folders.map({ $0.toDictionary() })
        let rootDict: [String:Any] = [Constants.ExportFile.rootElement: foldersDict]

        return try? JSONSerialization.data(withJSONObject: rootDict, options: .prettyPrinted)
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
            folderShortcutRecordView.keyCombo = HotKeyService.shared.snippetKeyCombo(forIdentifier: folder.identifier)
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
            reloadData()
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
                reloadData()
                outlineView.selectRowIndexes(NSIndexSet(index: outlineView.row(forItem: snippet)) as IndexSet, byExtendingSelection: false)
                fromFolder.rearrangesSnippetIndex()
                changeItemFocus()
                return true
            } else {
                // Move to other folder
                toFolder.snippets.insert(snippet, at: index)
                fromFolder.snippets.remove(objectAtIndex: draggedData.index)
                reloadData()
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
        HotKeyService.shared.unregisterSnippetHotKey(with: selectedFolder.identifier)
    }

    func recordView(_ recordView: RecordView, didChangeKeyCombo keyCombo: KeyCombo) {
        guard let selectedFolder = selectedFolder else { return }
        HotKeyService.shared.registerSnippetHotKey(with: selectedFolder.identifier, keyCombo: keyCombo)
    }

    func recordViewDidEndRecording(_ recordView: RecordView) {}
}

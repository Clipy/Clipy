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
            textView.font = NSFont.systemFontOfSize(14)
            textView.automaticQuoteSubstitutionEnabled = false
            textView.enabledTextCheckingTypes = 0
            textView.richText = false
        }
    }
    @IBOutlet weak var outlineView: NSOutlineView! {
        didSet {
            // Enable Drag and Drop
            outlineView.registerForDraggedTypes([Constants.Common.draggedDataType])
        }
    }

    private let defaults = NSUserDefaults.standardUserDefaults()
    private var folders = [CPYFolder]()
    private var selectedSnippet: CPYSnippet? {
        guard let snippet = outlineView.itemAtRow(outlineView.selectedRow) as? CPYSnippet else { return nil }
        return snippet
    }
    private var selectedFolder: CPYFolder? {
        guard let item = outlineView.itemAtRow(outlineView.selectedRow) else { return nil }
        if let folder = outlineView.parentForItem(item) as? CPYFolder {
            return folder
        } else if let folder = item as? CPYFolder {
            return folder
        }
        return nil
    }

    // MARK: - Window Life Cycle
    override func windowDidLoad() {
        super.windowDidLoad()
        self.window?.collectionBehavior = .CanJoinAllSpaces
        self.window?.backgroundColor = NSColor(white: 0.99, alpha: 1)
        if #available(OSX 10.10, *) {
            self.window?.titlebarAppearsTransparent = true
        }
        // HACK: Copy as an object that does not put under Realm management.
        // https://github.com/realm/realm-cocoa/issues/1734
        let realm = try! Realm()
        folders = realm.objects(CPYFolder.self)
                    .sorted("index", ascending: true)
                    .map { $0.deepCopy() }
        // Select first folder
        if let folder = folders.first {
            outlineView.selectRowIndexes(NSIndexSet(index: outlineView.rowForItem(folder)), byExtendingSelection: false)
            changeItemFocus()
        }
    }

    override func showWindow(sender: AnyObject?) {
        super.showWindow(sender)
        window?.makeKeyAndOrderFront(self)
    }
}

// MARK: - IBActions
extension CPYSnippetsEditorWindowController {
    @IBAction func addSnippetButtonTapped(sender: AnyObject) {
        guard let folder = selectedFolder else {
            NSBeep()
            return
        }
        let snippet = folder.createSnippet()
        folder.snippets.append(snippet)
        folder.mergeSnippet(snippet)
        outlineView.reloadData()
        outlineView.expandItem(folder)
        outlineView.selectRowIndexes(NSIndexSet(index: outlineView.rowForItem(snippet)), byExtendingSelection: false)
        changeItemFocus()
    }

    @IBAction func addFolderButtonTapped(sender: AnyObject) {
        let folder = CPYFolder.create()
        folders.append(folder)
        folder.merge()
        outlineView.reloadData()
        outlineView.selectRowIndexes(NSIndexSet(index: outlineView.rowForItem(folder)), byExtendingSelection: false)
        changeItemFocus()
    }

    @IBAction func deleteButtonTapped(sender: AnyObject) {
        guard let item = outlineView.itemAtRow(outlineView.selectedRow) else {
            NSBeep()
            return
        }

        let alert = NSAlert()
        alert.messageText = LocalizedString.DeleteItem.value
        alert.informativeText = LocalizedString.ConfirmDeleteItem.value
        alert.addButtonWithTitle(LocalizedString.DeleteItem.value)
        alert.addButtonWithTitle(LocalizedString.Cancel.value)
        NSApp.activateIgnoringOtherApps(true)
        let result = alert.runModal()
        if result != NSAlertFirstButtonReturn { return }

        if let folder = item as? CPYFolder {
            folders.removeObject(folder)
            folder.remove()
            HotKeyManager.sharedManager.removeFolderHotKey(folder.identifier)
        } else if let snippet = item as? CPYSnippet, folder = outlineView.parentForItem(item) as? CPYFolder, index = folder.snippets.indexOf(snippet) {
            folder.snippets.removeAtIndex(index)
            snippet.remove()
        }
        outlineView.reloadData()
        changeItemFocus()
    }

    @IBAction func changeStatusButtonTapped(sender: AnyObject) {
        guard let item = outlineView.itemAtRow(outlineView.selectedRow) else {
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

    @IBAction func importSnippetButtonTapped(sender: AnyObject) {
        let panel = NSOpenPanel()
        panel.allowsMultipleSelection = false
        panel.directoryURL = NSURL(fileURLWithPath: NSHomeDirectory())
        panel.allowedFileTypes = [Constants.Xml.fileType]
        let returnCode = panel.runModal()

        if returnCode != NSOKButton { return }

        let fileURLs = panel.URLs
        guard let url = fileURLs.first else { return }
        guard let data = NSData(contentsOfURL: url) else { return }

        do {
            let realm = try! Realm()
            let lastFolder = realm.objects(CPYFolder.self).sorted("index", ascending: true).last
            var folderIndex = (lastFolder?.index ?? -1) + 1
            // Create Document
            let xmlDocument = try AEXMLDocument(xmlData: data)
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

    @IBAction func exportSnippetButtonTapped(sender: AnyObject) {
        let xmlDocument = AEXMLDocument()
        let rootElement = xmlDocument.addChild(name: Constants.Xml.rootElement)

        let realm = try! Realm()
        let folders = realm.objects(CPYFolder.self).sorted("index", ascending: true)
        folders.forEach { folder in
            let folderElement = rootElement.addChild(name: Constants.Xml.folderElement)
            folderElement.addChild(name: Constants.Xml.titleElement, value: folder.title, attributes: nil)

            let snippetsElement = folderElement.addChild(name: Constants.Xml.snippetsElement)
            folder.snippets
                .sorted("index", ascending: true)
                .forEach { snippet in
                    let snippetElement = snippetsElement.addChild(name: Constants.Xml.snippetElement)
                    snippetElement.addChild(name: Constants.Xml.titleElement, value: snippet.title, attributes: nil)
                    snippetElement.addChild(name: Constants.Xml.contentElement, value: snippet.content, attributes: nil)
                }
        }

        let panel = NSSavePanel()
        panel.accessoryView = nil
        panel.canSelectHiddenExtension = true
        panel.allowedFileTypes = [Constants.Xml.fileType]
        panel.allowsOtherFileTypes = false
        panel.directoryURL = NSURL(fileURLWithPath: NSHomeDirectory())
        panel.nameFieldStringValue = "snippets"
        let returnCode = panel.runModal()

        if returnCode != NSOKButton { return }

        guard let data = xmlDocument.xmlString.dataUsingEncoding(NSUTF8StringEncoding) else { return }
        guard let path = panel.URL?.path else { return }

        if !data.writeToFile(path, atomically: true) { NSBeep() }
    }
}

// MARK: - Item Selected
private extension CPYSnippetsEditorWindowController {
    private func changeItemFocus() {
        // Reset TextView Undo/Redo history
        textView.undoManager?.removeAllActions()
        guard let item = outlineView.itemAtRow(outlineView.selectedRow) else {
            folderSettingView.hidden = true
            textView.hidden = true
            folderShortcutRecordView.keyCombo = nil
            folderTitleTextField.stringValue = ""
            return
        }
        if let folder = item as? CPYFolder {
            textView.string = ""
            folderTitleTextField.stringValue = folder.title
            folderShortcutRecordView.keyCombo = HotKeyManager.sharedManager.folderKeyCombo(folder.identifier)
            folderSettingView.hidden = false
            textView.hidden = true
        } else if let snippet = item as? CPYSnippet {
            textView.string = snippet.content
            folderTitleTextField.stringValue = ""
            folderShortcutRecordView.keyCombo = nil
            folderSettingView.hidden = true
            textView.hidden = false
        }
    }
}

// MARK: - NSSplitView Delegate
extension CPYSnippetsEditorWindowController: NSSplitViewDelegate {
    func splitView(splitView: NSSplitView, constrainMinCoordinate proposedMinimumPosition: CGFloat, ofSubviewAt dividerIndex: Int) -> CGFloat {
        return proposedMinimumPosition + 150
    }

    func splitView(splitView: NSSplitView, constrainMaxCoordinate proposedMaximumPosition: CGFloat, ofSubviewAt dividerIndex: Int) -> CGFloat {
        return proposedMaximumPosition / 2
    }
}

// MARK: - NSOutlineView DataSource
extension CPYSnippetsEditorWindowController: NSOutlineViewDataSource {
    func outlineView(outlineView: NSOutlineView, numberOfChildrenOfItem item: AnyObject?) -> Int {
        if item == nil {
            return Int(folders.count)
        } else if let folder = item as? CPYFolder {
            return Int(folder.snippets.count)
        }
        return 0
    }

    func outlineView(outlineView: NSOutlineView, isItemExpandable item: AnyObject) -> Bool {
        if let folder = item as? CPYFolder {
            return (folder.snippets.count != 0)
        }
        return false
    }

    func outlineView(outlineView: NSOutlineView, child index: Int, ofItem item: AnyObject?) -> AnyObject {
        if item == nil {
            return folders[index]
        } else if let folder = item as? CPYFolder {
            return folder.snippets[index]
        }
        return ""
    }

    func outlineView(outlineView: NSOutlineView, objectValueForTableColumn tableColumn: NSTableColumn?, byItem item: AnyObject?) -> AnyObject? {
        if let folder = item as? CPYFolder {
            return folder.title
        } else if let snippet = item as? CPYSnippet {
            return snippet.title
        }
        return ""
    }

    // MARK: - Drag and Drop
    func outlineView(outlineView: NSOutlineView, pasteboardWriterForItem item: AnyObject) -> NSPasteboardWriting? {
        let pasteboardItem = NSPasteboardItem()
        if let folder = item as? CPYFolder, index = folders.indexOf(folder) {
            let draggedData = CPYDraggedData(type: .Folder, folderIdentifier: folder.identifier, snippetIdentifier: nil, index: index)
            let data = NSKeyedArchiver.archivedDataWithRootObject(draggedData)
            pasteboardItem.setData(data, forType: Constants.Common.draggedDataType)
        } else if let snippet = item as? CPYSnippet, folder = outlineView.parentForItem(snippet) as? CPYFolder {
            guard let index = folder.snippets.indexOf(snippet) else { return nil }
            let draggedData = CPYDraggedData(type: .Snippet, folderIdentifier: folder.identifier, snippetIdentifier: snippet.identifier, index: Int(index))
            let data = NSKeyedArchiver.archivedDataWithRootObject(draggedData)
            pasteboardItem.setData(data, forType: Constants.Common.draggedDataType)
        } else {
            return nil
        }
        return pasteboardItem
    }

    func outlineView(outlineView: NSOutlineView, validateDrop info: NSDraggingInfo, proposedItem item: AnyObject?, proposedChildIndex index: Int) -> NSDragOperation {
        if index < 0 { return .None }
        let pasteboard = info.draggingPasteboard()
        guard let data = pasteboard.dataForType(Constants.Common.draggedDataType) else { return .None }
        guard let draggedData = NSKeyedUnarchiver.unarchiveObjectWithData(data) as? CPYDraggedData else { return .None }

        switch draggedData.type {
        case .Folder where item == nil:
            return .Move
        case .Snippet where item is CPYFolder:
            return .Move
        default:
            return .None
        }
    }

    func outlineView(outlineView: NSOutlineView, acceptDrop info: NSDraggingInfo, item: AnyObject?, childIndex index: Int) -> Bool {
        if index < 0 { return false  }
        let pasteboard = info.draggingPasteboard()
        guard let data = pasteboard.dataForType(Constants.Common.draggedDataType) else { return false }
        guard let draggedData = NSKeyedUnarchiver.unarchiveObjectWithData(data) as? CPYDraggedData else { return false }

        switch draggedData.type {
        case .Folder where index != draggedData.index:
            guard let folder = folders.filter({ $0.identifier == draggedData.folderIdentifier }).first else { return false }
            folders.insert(folder, atIndex: index)
            let removedIndex = (index < draggedData.index) ? draggedData.index + 1 : draggedData.index
            folders.removeAtIndex(removedIndex)
            outlineView.reloadData()
            outlineView.selectRowIndexes(NSIndexSet(index: outlineView.rowForItem(folder)), byExtendingSelection: false)
            CPYFolder.rearrangesIndex(folders)
            changeItemFocus()
            return true
        case .Snippet:
            guard let fromFolder = folders.filter({ $0.identifier == draggedData.folderIdentifier }).first else { return false }
            guard let toFolder = item as? CPYFolder else { return false }
            guard let snippet = fromFolder.snippets.filter({ $0.identifier == draggedData.snippetIdentifier }).first else { return false }

            if fromFolder.identifier == toFolder.identifier {
                if index == draggedData.index { return false }
                // Move to same folder
                fromFolder.snippets.insert(snippet, atIndex: index)
                let removedIndex = (index < draggedData.index) ? draggedData.index + 1 : draggedData.index
                fromFolder.snippets.removeAtIndex(removedIndex)
                outlineView.reloadData()
                outlineView.selectRowIndexes(NSIndexSet(index: outlineView.rowForItem(snippet)), byExtendingSelection: false)
                fromFolder.rearrangesSnippetIndex()
                changeItemFocus()
                return true
            } else {
                // Move to other folder
                toFolder.snippets.insert(snippet, atIndex: index)
                fromFolder.snippets.removeAtIndex(draggedData.index)
                outlineView.reloadData()
                outlineView.expandItem(toFolder)
                outlineView.selectRowIndexes(NSIndexSet(index: outlineView.rowForItem(snippet)), byExtendingSelection: false)
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
    func outlineView(outlineView: NSOutlineView, willDisplayCell cell: AnyObject, forTableColumn tableColumn: NSTableColumn?, item: AnyObject) {
        guard let cell = cell as? CPYSnippetsEditorCell else { return }
        if let folder = item as? CPYFolder {
            cell.iconType = .Folder
            cell.isItemEnabled = folder.enable
        } else if let snippet = item as? CPYSnippet {
            cell.iconType = .None
            cell.isItemEnabled = snippet.enable
        }
    }

    func outlineViewSelectionDidChange(notification: NSNotification) {
        changeItemFocus()
    }

    func control(control: NSControl, textShouldEndEditing fieldEditor: NSText) -> Bool {
        guard let text = fieldEditor.string where text.characters.count != 0 else { return false }
        guard let outlineView = control as? NSOutlineView else { return false }
        guard let item = outlineView.itemAtRow(outlineView.selectedRow) else { return false }
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
    func textView(textView: NSTextView, shouldChangeTextInRange affectedCharRange: NSRange, replacementString: String?) -> Bool {
        guard let replacementString = replacementString else { return false }
        guard let text = textView.string else { return false }
        guard let snippet = selectedSnippet else { return false }
        let string = (text as NSString).stringByReplacingCharactersInRange(affectedCharRange, withString: replacementString)
        snippet.content = string
        snippet.merge()
        return true
    }
}

// MARK: - RecordView Delegate
extension CPYSnippetsEditorWindowController: RecordViewDelegate {
    func recordViewShouldBeginRecording(recordView: RecordView) -> Bool {
        guard let _ = selectedFolder else { return false }
        return true
    }

    func recordView(recordView: RecordView, canRecordKeyCombo keyCombo: KeyCombo) -> Bool {
        guard let _ = selectedFolder else { return false }
        return true
    }

    func recordViewDidClearShortcut(recordView: RecordView) {
        guard let selectedFolder = selectedFolder else { return }
        HotKeyManager.sharedManager.removeFolderHotKey(selectedFolder.identifier)
    }

    func recordView(recordView: RecordView, didChangeKeyCombo keyCombo: KeyCombo) {
        guard let selectedFolder = selectedFolder else { return }
        HotKeyManager.sharedManager.addFolderHotKey(selectedFolder.identifier, keyCombo: keyCombo)
    }

    func recordViewDidEndRecording(recordView: RecordView) {}
}

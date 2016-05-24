//
//  CPYSnippetsEditorWindowController.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/05/18.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import Realm
import KeyHolder
import Magnet

final class CPYSnippetsEditorWindowController: NSWindowController {

    // MARK: - Properties
    static let sharedController = CPYSnippetsEditorWindowController(windowNibName: "CPYSnippetsEditorWindowController")
    @IBOutlet weak var splitView: CPYSplitView!
    @IBOutlet weak var folderSettingView: NSView!
    @IBOutlet weak var outlineView: NSOutlineView!
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
        }
    }

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
        folders = CPYFolder.allObjects()
                    .sortedResultsUsingProperty("index", ascending: true)
                    .flatMap { $0 as? CPYFolder }
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
        guard let folder = selectedFolder else { return }
        let snippet = folder.createSnippet()
        folder.snippets.addObject(snippet)
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
        guard let item = outlineView.itemAtRow(outlineView.selectedRow) else { return }
        if let folder = item as? CPYFolder {
            folders.removeObject(folder)
            folder.remove()
            HotKeyManager.sharedManager.removeFolderHotKey(folder.identifier)
        } else if let snippet = item as? CPYSnippet, folder = outlineView.parentForItem(item) as? CPYFolder {
            folder.snippets.removeObject(snippet)
            snippet.remove()
        }
        outlineView.reloadData()
        changeItemFocus()
    }

    @IBAction func changeStatusButtonTapped(sender: AnyObject) {
        guard let item = outlineView.itemAtRow(outlineView.selectedRow) else { return }
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

    @IBAction func settingButtonTapped(sender: AnyObject) {}

    @IBAction func outlineViewTapped(sender: AnyObject) {
        changeItemFocus()
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
            return folder.snippets.objectAtIndex(UInt(index))
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
}

// MARK: - NSOutlineView Delegate
extension CPYSnippetsEditorWindowController: NSOutlineViewDelegate, NSTableViewDelegate {
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

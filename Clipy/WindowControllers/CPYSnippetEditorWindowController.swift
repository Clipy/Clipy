//
//  CPYSnippetEditorWindowController.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/28.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa

private let ADD_SNIPPET_IDENTIFIER      = "AddSnippet"
private let DELETE_SNIPPET_IDENTIFIER   = "DeleteSnippet"
private let CHECK_SNIPPET_IDENTIFIER    = "CheckSnippet"
private let DEFAULT_EXPORT_FILENAME     = "snippets"

class CPYSnippetEditorWindowController: NSWindowController {
    
    // MARK: - Properties
    private var foldersFromFile = [AnyObject]()
    private var currentElementContent = ""
    private var currentFolder = [String: AnyObject]()
    private var currentSnippet = [String: String]()
    
    @IBOutlet weak var folderTableView: CPYFolderTableView! {
        didSet {
            self.folderTableView.tableDelegate = self
        }
    }
    @IBOutlet weak var snippetTableView: CPYSnippetTableView! {
        didSet {
            self.snippetTableView.tableDelegate = self
        }
    }
    @IBOutlet var snippetContentTextView: NSTextView! {
        didSet {
            self.snippetContentTextView.delegate = self
        }
    }
    
    // MARK: - View Life Cycle
    override func windowDidLoad() {
        super.windowDidLoad()
        
        let results = CPYSnippetManager.sharedManager.loadFolders()
        if results.count != 0 {
            self.folderTableView.selectRowIndexes(NSIndexSet(index: 0), byExtendingSelection: false)
            self.selectFolder(0)
        }
    }
    
    // MARK: - Override Methods
    override func showWindow(sender: AnyObject?) {
        super.showWindow(sender)
        self.window?.makeKeyAndOrderFront(self)
    }

    override func validateToolbarItem(theItem: NSToolbarItem) -> Bool {
        let itemIdentifier = theItem.itemIdentifier
        
        if itemIdentifier == ADD_SNIPPET_IDENTIFIER {
            let results = CPYSnippetManager.sharedManager.loadFolders()
            return (results.count > 0) ? true : false
        } else if itemIdentifier == CHECK_SNIPPET_IDENTIFIER || itemIdentifier == DELETE_SNIPPET_IDENTIFIER {
            let folders = CPYSnippetManager.sharedManager.loadSortedFolders()
            if folders.count > 0 && self.folderTableView.selectedRow != -1 {
                if let folder = folders.objectAtIndex(UInt(self.folderTableView.selectedRow)) as? CPYFolder {
                    return (folder.snippets.count > 0)
                }
            }
            return false
        }
        return true
    }
    
    @IBAction func addSnippet(sender: AnyObject) {
        if !self.endEditingForWindow() || self.folderTableView.selectedRow < 0 || self.folderTableView.selectedRowIndexes.count > 1 {
            return
        }
        
        if let folder = CPYSnippetManager.sharedManager.loadSortedFolders().objectAtIndex(UInt(self.folderTableView.selectedRow)) as? CPYFolder {
            
            CPYSnippetManager.sharedManager.addSnippet(nil, folder: folder)
            self.snippetTableView.reloadData()
            
            let rowIndex = Int(folder.snippets.count - 1)
            self.snippetTableView.selectRowIndexes(NSIndexSet(index: rowIndex), byExtendingSelection: false)
            self.selectSnippet(rowIndex, folder: folder)
            self.snippetTableView.editColumn(0, row: rowIndex, withEvent: nil, select: true)
        }
    }
    
    @IBAction func removeSnippet(sender: AnyObject) {
        let selectIndexPaths = self.snippetTableView.selectedRowIndexes
        if selectIndexPaths.count == 0 || self.folderTableView.selectedRow < 0 || self.folderTableView.selectedRowIndexes.count > 1 {
            return
        }
        
        if let folder = CPYSnippetManager.sharedManager.loadSortedFolders().objectAtIndex(UInt(self.folderTableView.selectedRow)) as? CPYFolder {
            
            var snippets = [RLMObject]()
            selectIndexPaths.enumerateIndexesUsingBlock { (index, stop) -> Void in
                let snippet = folder.snippets.sortedResultsUsingProperty("index", ascending: true).objectAtIndex(UInt(index)) as! CPYSnippet
                snippets.append(snippet)
            }
            
            CPYSnippetManager.sharedManager.removeSnippets(snippets)
            self.snippetTableView.reloadData()
            
            // FIXME: やり方強引すぎ
            var indexSet: NSIndexSet!
            if Int(folder.snippets.count) >= selectIndexPaths.firstIndex {
                indexSet = NSIndexSet(index: Int(selectIndexPaths.firstIndex - 1))
            } else {
                indexSet = NSIndexSet(index: Int(folder.snippets.count - 1))
            }
            if indexSet.firstIndex == -1 {
                indexSet = NSIndexSet(index: 0)
            }
            if folder.snippets.count != 0 {
                self.snippetTableView.selectRowIndexes(indexSet, byExtendingSelection: false)
                self.selectSnippet(indexSet.firstIndex, folder: folder)
            } else {
                self.snippetContentTextView.string = ""
            }
            
        }
        
        NSNotificationCenter.defaultCenter().postNotificationName(kCPYChangeContentsNotification, object: nil)
    }
    
    @IBAction func toggleSnippetEnable(sender: AnyObject) {
        let selectIndexPaths = self.snippetTableView.selectedRowIndexes
        if selectIndexPaths.count == 0 || self.folderTableView.selectedRow < 0 || self.folderTableView.selectedRowIndexes.count > 1 {
            return
        }
        
        if let folder = CPYSnippetManager.sharedManager.loadSortedFolders().objectAtIndex(UInt(self.folderTableView.selectedRow)) as? CPYFolder {
            
            var snippets = [RLMObject]()
            selectIndexPaths.enumerateIndexesUsingBlock { (index, stop) -> Void in
                let snippet = folder.snippets.sortedResultsUsingProperty("index", ascending: true).objectAtIndex(UInt(index)) as! CPYSnippet
                snippets.append(snippet)
            }
            
            CPYSnippetManager.sharedManager.updateSnippetEnable(snippets)
            self.snippetTableView.reloadData()
        }
    }
    
    @IBAction func toggleFolderEnabled(sender: AnyObject) {
        let selectIndexPaths = self.folderTableView.selectedRowIndexes
        if selectIndexPaths.count == 0 {
            return
        }
        
        var folders = [RLMObject]()
        let results = CPYSnippetManager.sharedManager.loadSortedFolders()
        selectIndexPaths.enumerateIndexesUsingBlock { (index, stop) -> Void in
            let folder = results.objectAtIndex(UInt(index)) as! CPYFolder
            folders.append(folder)
        }
        
        CPYSnippetManager.sharedManager.updateFolderEnable(folders)
        self.folderTableView.reloadData()
    }
    
    @IBAction func removeFolder(sender: AnyObject) {
        let selectIndexPaths = self.folderTableView.selectedRowIndexes
        if selectIndexPaths.count == 0 {
            return
        }
        
        var folders = [RLMObject]()
        let folderResults = CPYSnippetManager.sharedManager.loadSortedFolders()
        selectIndexPaths.enumerateIndexesUsingBlock { (index, stop) -> Void in
            let folder = folderResults.objectAtIndex(UInt(index)) as! CPYFolder
            folders.append(folder)
        }
        
        CPYSnippetManager.sharedManager.removeFolders(folders)
        self.folderTableView.reloadData()
        
        // FIXME: やり方強引すぎ
        let results = CPYSnippetManager.sharedManager.loadSortedFolders()
        var indexSet: NSIndexSet!
        if Int(results.count) >= selectIndexPaths.firstIndex {
            indexSet = NSIndexSet(index: Int(selectIndexPaths.firstIndex - 1))
        } else {
            indexSet = NSIndexSet(index: Int(results.count - 1))
        }
        if indexSet.firstIndex == -1 {
            indexSet = NSIndexSet(index: 0)
        }
        if results.count != 0 {
            self.folderTableView.selectRowIndexes(indexSet, byExtendingSelection: false)
            self.selectFolder(indexSet.firstIndex)
        } else {
            self.snippetTableView.setFolder(nil)
            self.snippetContentTextView.string = ""
        }
        
        NSNotificationCenter.defaultCenter().postNotificationName(kCPYChangeContentsNotification, object: nil)
    }
    
    @IBAction func addFolder(sender: AnyObject) {
        if !self.endEditingForWindow() {
            return
        }
        
        CPYSnippetManager.sharedManager.addFolder(nil)
        self.folderTableView.reloadData()
        
        let rowIndex = Int(CPYSnippetManager.sharedManager.loadSortedFolders().count - 1)
        self.folderTableView.selectRowIndexes(NSIndexSet(index: rowIndex), byExtendingSelection: false)
        self.selectFolder(rowIndex)
        self.folderTableView.editColumn(0, row: rowIndex, withEvent: nil, select: true)
    }
    
    @IBAction func importSnippets(sender: AnyObject) {
        
        let fileTypes = [kXMLFileType]
        let openPanel = NSOpenPanel()
        openPanel.allowsMultipleSelection = false
        
        var returnCode = 0
        
        openPanel.directoryURL = NSURL(fileURLWithPath: NSHomeDirectory())
        openPanel.allowedFileTypes = fileTypes
        returnCode = openPanel.runModal()
        
        if returnCode != NSOKButton {
            return
        }
        
        let fileURLs = openPanel.URLs
        if fileURLs.isEmpty {
            return
        }
        
        let url = fileURLs[0]
        self.parseXMLFileAtURL(url)
            
        if !self.foldersFromFile.isEmpty {
            self.addImportedFolders(self.foldersFromFile)
        }
        
    }
    
    @IBAction func exportSnippets(sender: AnyObject) {
        
        let results = CPYSnippetManager.sharedManager.loadSortedFolders()
        let rootElement = NSXMLNode.elementWithName(kRootElement) as! NSXMLElement
        
        for object in results {
            let folder = object as! CPYFolder
            let folderTitle = folder.title
            let snippets = folder.snippets.sortedResultsUsingProperty("index", ascending: true)
            
            let folderElement = NSXMLNode.elementWithName(kFolderElement) as! NSXMLElement
            folderElement.addChild(NSXMLNode.elementWithName(kTitleElement, stringValue: folderTitle) as! NSXMLNode)
            
            let snippetsElement = NSXMLNode.elementWithName(kSnippetsElement) as! NSXMLElement
            
            for snippetObject in snippets {
                let snippet = snippetObject as! CPYSnippet
                let snippetTitle = snippet.title
                let content = snippet.content
                
                let snippetElement = NSXMLNode.elementWithName(kSnippetElement) as! NSXMLElement
                snippetElement.addChild(NSXMLNode.elementWithName(kTitleElement, stringValue: snippetTitle) as! NSXMLNode)
                
                let contentElement = NSXMLElement(kind: .ElementKind, options: Int(NSXMLNodePreserveWhitespace))
                contentElement.name = kContentElement
                contentElement.stringValue = content
                
                snippetElement.addChild(contentElement)
                
                snippetsElement.addChild(snippetElement)
            }
            
            folderElement.addChild(snippetsElement)
            rootElement.addChild(folderElement)
        }
        
        let xmlDocument = NSXMLDocument(rootElement: rootElement)
        xmlDocument.version = "1.0"
        xmlDocument.characterEncoding = "UTF-8"
        
        let savePanel = NSSavePanel()
        savePanel.accessoryView = nil
        savePanel.canSelectHiddenExtension = true
        savePanel.allowedFileTypes = [kXMLFileType]
        savePanel.allowsOtherFileTypes = false
        
        var returnCode = 0
        
        savePanel.directoryURL = NSURL(fileURLWithPath: NSHomeDirectory())
        savePanel.nameFieldStringValue = DEFAULT_EXPORT_FILENAME
        returnCode = savePanel.runModal()
        
        if returnCode == NSOKButton {
            let data = xmlDocument.XMLDataWithOptions(Int(NSXMLNodePrettyPrint))
            if let fileURL = savePanel.URL {
                if !data.writeToFile(fileURL.path!, atomically: true) {
                    NSBeep()
                    //NSRunAlertPanel(nil, NSLocalizedString(@"Could not write document out...", nil), NSLocalizedString(@"OK", nil), nil,  nil);
                    return
                }
            }
        }
    }
    
    // MARK: - Private Methods
    private func endEditingForWindow() -> Bool {
        let editingEnded = self.window!.makeFirstResponder(self.window)
        if !editingEnded {
            return false
        }
        return true
    }
    
}

// MARK: - NSWindow Delegate
extension CPYSnippetEditorWindowController: NSWindowDelegate {
    func windowWillClose(notification: NSNotification) {
        if let window = self.window {
            if !window.makeFirstResponder(window) {
                window.endEditingFor(nil)
            }
        }
        NSApp.deactivate()
        NSNotificationCenter.defaultCenter().postNotificationName(kCPYSnippetEditorWillCloseNotification, object: nil)
    }
}

// MARK: - CPYFolderTableView Delegate
extension CPYSnippetEditorWindowController: CPYFolderTableViewDelegate {
    
    func selectFolder(row: Int) {
        if row < 0 {
            return
        }
        if let folder = CPYSnippetManager.sharedManager.loadSortedFolders().objectAtIndex(UInt(row)) as? CPYFolder {
            self.snippetTableView.setFolder(folder)
            if folder.snippets.count != 0 {
                self.selectSnippet(0, folder: folder)
                self.snippetTableView.selectRowIndexes(NSIndexSet(index: 0), byExtendingSelection: false)
            } else {
                self.snippetContentTextView.string = ""
            }
        }
    }
    
}

// MARK: - CPYSnippetTableView Delegate
extension CPYSnippetEditorWindowController: CPYSnippetTableViewDelegate {
    func selectSnippet(row: Int, folder: CPYFolder?) {
        if row < 0 {
            return
        }
        if folder != nil {
            if let snippet = folder!.snippets.sortedResultsUsingProperty("index", ascending: true).objectAtIndex(UInt(row)) as? CPYSnippet {
                self.snippetContentTextView.string = snippet.content
            }
        }
    }
}

// MARK: - NSTextView Delegate
extension CPYSnippetEditorWindowController: NSTextViewDelegate {
    
    func textView(textView: NSTextView, shouldChangeTextInRange affectedCharRange: NSRange, replacementString: String?) -> Bool {
        if let replacementString = replacementString {
            let string = (textView.string! as NSString).stringByReplacingCharactersInRange(affectedCharRange, withString: replacementString)
            
            if self.folderTableView.selectedRow != -1 && self.snippetTableView.selectedRow != -1 {
                
                if let folder = CPYSnippetManager.sharedManager.loadSortedFolders().objectAtIndex(UInt(self.folderTableView.selectedRow)) as? CPYFolder {
                    if let snippet = folder.snippets.sortedResultsUsingProperty("index", ascending: true).objectAtIndex(UInt(self.snippetTableView.selectedRow)) as? CPYSnippet {
                        CPYSnippetManager.sharedManager.updateSnipeetContent(snippet, content: string)
                    }
                }
                return true
            }
        }
        
        return false
    }
    
}

// MARK: - Import/Export Snippets
extension CPYSnippetEditorWindowController {
    // MARK: - Import XML
    private func parseXMLFileAtURL(url: NSURL) {
        
        if let xmlParser = NSXMLParser(contentsOfURL: url) {
            xmlParser.delegate = self
            let returnCode = xmlParser.parse()
            
            if !returnCode {
                //NSRunAlertPanel(nil, NSLocalizedString(@"Failed to parse XML file", nil), NSLocalizedString(@"OK", nil), nil, nil);
            }
        }
        
    }
    
    private func addImportedFolders(folders: [AnyObject]) {
        if !self.endEditingForWindow() {
            return
        }
        
        for folder in folders as! [[String: AnyObject]] {
            
            let folderModel = CPYFolder()
            
            if let folderName = folder[kTitle] as? String {
                folderModel.title = folderName
            } else {
                folderModel.title = "untitled folder"
            }
            
            if let lastFolder = CPYSnippetManager.sharedManager.loadSortedFolders().lastObject() as? CPYFolder {
                folderModel.index = lastFolder.index + 1
            } else {
                folderModel.index = 0
            }
            
            do {
                let realm = RLMRealm.defaultRealm()
                try realm.transactionWithBlock( { () -> Void in
                    realm.addObject(folderModel)
                })
            } catch {}
            
            if let snippets = folder[kSnippets] as? [AnyObject] {
                for snippet in snippets as! [[String: String]] {
                    
                    let snippetModel = CPYSnippet()
                    
                    if let snippetName = snippet[kTitle] {
                        snippetModel.title = snippetName
                    } else {
                        snippetModel.title = "untitled snippet"
                    }
                    if let snippetContent = snippet[kContent] {
                        snippetModel.content = snippetContent
                    }
                    
                    if let lastSnippet = folderModel.snippets.sortedResultsUsingProperty("index", ascending: true).lastObject() as? CPYSnippet {
                        snippetModel.index = lastSnippet.index + 1
                    } else {
                        snippetModel.index = 0
                    }
                    
                    do {
                        let realm = RLMRealm.defaultRealm()
                        try realm.transactionWithBlock({ () -> Void in
                            folderModel.snippets.addObject(snippetModel)
                        })
                    } catch {}
                }
                
                self.foldersFromFile = [AnyObject]()
            }
        }
        self.folderTableView.reloadData()
    }

}


// MARK: - NSXMLParser Delegate
extension CPYSnippetEditorWindowController: NSXMLParserDelegate {
    
    func parser(parser: NSXMLParser, didStartElement elementName: String, namespaceURI: String?, qualifiedName qName: String?, attributes attributeDict: [String : String]) {
    
        self.currentElementContent = ""
        
        if elementName == kRootElement {
            self.foldersFromFile = [AnyObject]()
        } else if elementName == kFolderElement {
            self.currentFolder = [kType: kFolderElement, kSnippets: [String]()]
        } else if elementName == kSnippetElement {
            self.currentSnippet = [kType: kSnippetElement]
        }
        
    }
    
    func parser(parser: NSXMLParser, foundCharacters string: String) {
        if !string.isEmpty {
            self.currentElementContent += string
        }
    }
    
    func parser(parser: NSXMLParser, didEndElement elementName: String, namespaceURI: String?, qualifiedName qName: String?) {
        if elementName == kFolderElement {
            self.foldersFromFile.append(self.currentFolder)
            self.currentFolder = [String: AnyObject]()
            return
        } else if elementName == kSnippetElement {
            if var snippets = self.currentFolder[kSnippets] as? [AnyObject] {
                snippets.append(self.currentSnippet)
                self.currentFolder.updateValue(snippets, forKey: kSnippets)
            }
            self.currentSnippet = [String: String]()
            return
        }
        
        if elementName == kTitleElement {
            if self.currentElementContent.isEmpty {
                return
            }
            if !self.currentSnippet.isEmpty {
                self.currentSnippet.updateValue(self.currentElementContent, forKey: elementName)
            } else {
                self.currentFolder.updateValue(self.currentElementContent, forKey: elementName)
            }
        } else if elementName == kContentElement {
            if self.currentElementContent.isEmpty {
                return
            }
            self.currentSnippet.updateValue(self.currentElementContent, forKey: elementName)
        }
        
        self.currentElementContent = ""
    }
    
}
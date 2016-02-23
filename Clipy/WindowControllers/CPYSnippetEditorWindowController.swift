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
            folderTableView.tableDelegate = self
        }
    }
    @IBOutlet weak var snippetTableView: CPYSnippetTableView! {
        didSet {
            snippetTableView.tableDelegate = self
        }
    }
    @IBOutlet var snippetContentTextView: NSTextView! {
        didSet {
            snippetContentTextView.delegate = self
        }
    }
    
    // MARK: - View Life Cycle
    override func windowDidLoad() {
        super.windowDidLoad()
        
        let results = CPYSnippetManager.sharedManager.loadFolders()
        if results.count != 0 {
            folderTableView.selectRowIndexes(NSIndexSet(index: 0), byExtendingSelection: false)
            selectFolder(0)
        }
    }
    
    // MARK: - Override Methods
    override func showWindow(sender: AnyObject?) {
        super.showWindow(sender)
        window?.makeKeyAndOrderFront(self)
    }

    override func validateToolbarItem(theItem: NSToolbarItem) -> Bool {
        let itemIdentifier = theItem.itemIdentifier
        
        if itemIdentifier == ADD_SNIPPET_IDENTIFIER {
            let results = CPYSnippetManager.sharedManager.loadFolders()
            return (results.count > 0) ? true : false
        } else if itemIdentifier == CHECK_SNIPPET_IDENTIFIER || itemIdentifier == DELETE_SNIPPET_IDENTIFIER {
            let folders = CPYSnippetManager.sharedManager.loadSortedFolders()
            if folders.count > 0 && folderTableView.selectedRow != -1 {
                if let folder = folders.objectAtIndex(UInt(folderTableView.selectedRow)) as? CPYFolder {
                    return (folder.snippets.count > 0)
                }
            }
            return false
        }
        return true
    }
    
    @IBAction func addSnippet(sender: AnyObject) {
        if !endEditingForWindow() || folderTableView.selectedRow < 0 || folderTableView.selectedRowIndexes.count > 1 {
            return
        }
        
        if let folder = CPYSnippetManager.sharedManager.loadSortedFolders().objectAtIndex(UInt(folderTableView.selectedRow)) as? CPYFolder {
            
            CPYSnippetManager.sharedManager.addSnippet(nil, folder: folder)
            snippetTableView.reloadData()
            
            let rowIndex = Int(folder.snippets.count - 1)
            snippetTableView.selectRowIndexes(NSIndexSet(index: rowIndex), byExtendingSelection: false)
            selectSnippet(rowIndex, folder: folder)
            snippetTableView.editColumn(0, row: rowIndex, withEvent: nil, select: true)
        }
    }
    
    @IBAction func removeSnippet(sender: AnyObject) {
        let selectIndexPaths = snippetTableView.selectedRowIndexes
        if selectIndexPaths.count == 0 || folderTableView.selectedRow < 0 || folderTableView.selectedRowIndexes.count > 1 {
            return
        }
        
        if let folder = CPYSnippetManager.sharedManager.loadSortedFolders().objectAtIndex(UInt(folderTableView.selectedRow)) as? CPYFolder {
            
            var snippets = [RLMObject]()
            selectIndexPaths.enumerateIndexesUsingBlock { (index, stop) -> Void in
                let snippet = folder.snippets.sortedResultsUsingProperty("index", ascending: true).objectAtIndex(UInt(index)) as! CPYSnippet
                snippets.append(snippet)
            }
            
            CPYSnippetManager.sharedManager.removeSnippets(snippets)
            snippetTableView.reloadData()
            
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
                snippetTableView.selectRowIndexes(indexSet, byExtendingSelection: false)
                selectSnippet(indexSet.firstIndex, folder: folder)
            } else {
                snippetContentTextView.string = ""
            }
            
        }
        
        NSNotificationCenter.defaultCenter().postNotificationName(kCPYChangeContentsNotification, object: nil)
    }
    
    @IBAction func toggleSnippetEnable(sender: AnyObject) {
        let selectIndexPaths = snippetTableView.selectedRowIndexes
        if selectIndexPaths.count == 0 || folderTableView.selectedRow < 0 || folderTableView.selectedRowIndexes.count > 1 {
            return
        }
        
        if let folder = CPYSnippetManager.sharedManager.loadSortedFolders().objectAtIndex(UInt(folderTableView.selectedRow)) as? CPYFolder {
            
            var snippets = [RLMObject]()
            selectIndexPaths.enumerateIndexesUsingBlock { (index, stop) -> Void in
                let snippet = folder.snippets.sortedResultsUsingProperty("index", ascending: true).objectAtIndex(UInt(index)) as! CPYSnippet
                snippets.append(snippet)
            }
            
            CPYSnippetManager.sharedManager.updateSnippetEnable(snippets)
            snippetTableView.reloadData()
        }
    }
    
    @IBAction func toggleFolderEnabled(sender: AnyObject) {
        let selectIndexPaths = folderTableView.selectedRowIndexes
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
        folderTableView.reloadData()
    }
    
    @IBAction func removeFolder(sender: AnyObject) {
        let selectIndexPaths = folderTableView.selectedRowIndexes
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
        folderTableView.reloadData()
        
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
            folderTableView.selectRowIndexes(indexSet, byExtendingSelection: false)
            selectFolder(indexSet.firstIndex)
        } else {
            snippetTableView.setFolder(nil)
            snippetContentTextView.string = ""
        }
        
        NSNotificationCenter.defaultCenter().postNotificationName(kCPYChangeContentsNotification, object: nil)
    }
    
    @IBAction func addFolder(sender: AnyObject) {
        if !endEditingForWindow() {
            return
        }
        
        CPYSnippetManager.sharedManager.addFolder(nil)
        folderTableView.reloadData()
        
        let rowIndex = Int(CPYSnippetManager.sharedManager.loadSortedFolders().count - 1)
        folderTableView.selectRowIndexes(NSIndexSet(index: rowIndex), byExtendingSelection: false)
        selectFolder(rowIndex)
        folderTableView.editColumn(0, row: rowIndex, withEvent: nil, select: true)
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
        parseXMLFileAtURL(url)
            
        if !foldersFromFile.isEmpty {
            addImportedFolders(foldersFromFile)
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
        let editingEnded = window!.makeFirstResponder(window)
        if !editingEnded {
            return false
        }
        return true
    }
    
}

// MARK: - NSWindow Delegate
extension CPYSnippetEditorWindowController: NSWindowDelegate {
    func windowWillClose(notification: NSNotification) {
        if let window = window {
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
            snippetTableView.setFolder(folder)
            if folder.snippets.count != 0 {
                selectSnippet(0, folder: folder)
                snippetTableView.selectRowIndexes(NSIndexSet(index: 0), byExtendingSelection: false)
            } else {
                snippetContentTextView.string = ""
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
                snippetContentTextView.string = snippet.content
            }
        }
    }
}

// MARK: - NSTextView Delegate
extension CPYSnippetEditorWindowController: NSTextViewDelegate {
    
    func textView(textView: NSTextView, shouldChangeTextInRange affectedCharRange: NSRange, replacementString: String?) -> Bool {
        if let replacementString = replacementString {
            let string = (textView.string! as NSString).stringByReplacingCharactersInRange(affectedCharRange, withString: replacementString)
            
            if folderTableView.selectedRow != -1 && snippetTableView.selectedRow != -1 {
                
                if let folder = CPYSnippetManager.sharedManager.loadSortedFolders().objectAtIndex(UInt(folderTableView.selectedRow)) as? CPYFolder {
                    if let snippet = folder.snippets.sortedResultsUsingProperty("index", ascending: true).objectAtIndex(UInt(snippetTableView.selectedRow)) as? CPYSnippet {
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
        if !endEditingForWindow() {
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
                
                foldersFromFile = [AnyObject]()
            }
        }
        folderTableView.reloadData()
    }

}


// MARK: - NSXMLParser Delegate
extension CPYSnippetEditorWindowController: NSXMLParserDelegate {
    
    func parser(parser: NSXMLParser, didStartElement elementName: String, namespaceURI: String?, qualifiedName qName: String?, attributes attributeDict: [String : String]) {
    
        currentElementContent = ""
        
        if elementName == kRootElement {
            foldersFromFile = [AnyObject]()
        } else if elementName == kFolderElement {
            currentFolder = [kType: kFolderElement, kSnippets: [String]()]
        } else if elementName == kSnippetElement {
            currentSnippet = [kType: kSnippetElement]
        }
        
    }
    
    func parser(parser: NSXMLParser, foundCharacters string: String) {
        if !string.isEmpty {
            currentElementContent += string
        }
    }
    
    func parser(parser: NSXMLParser, didEndElement elementName: String, namespaceURI: String?, qualifiedName qName: String?) {
        if elementName == kFolderElement {
            foldersFromFile.append(currentFolder)
            currentFolder = [String: AnyObject]()
            return
        } else if elementName == kSnippetElement {
            if var snippets = currentFolder[kSnippets] as? [AnyObject] {
                snippets.append(currentSnippet)
                currentFolder.updateValue(snippets, forKey: kSnippets)
            }
            currentSnippet = [String: String]()
            return
        }
        
        if elementName == kTitleElement {
            if currentElementContent.isEmpty {
                return
            }
            if !currentSnippet.isEmpty {
                currentSnippet.updateValue(currentElementContent, forKey: elementName)
            } else {
                currentFolder.updateValue(currentElementContent, forKey: elementName)
            }
        } else if elementName == kContentElement {
            if currentElementContent.isEmpty {
                return
            }
            currentSnippet.updateValue(currentElementContent, forKey: elementName)
        }
        
        currentElementContent = ""
    }
    
}
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
    
    // MARK: - View Life Cycle
    override func windowDidLoad() {
        super.windowDidLoad()
        self.window?.collectionBehavior = NSWindowCollectionBehavior.CanJoinAllSpaces
        /*
        if let results = CPYSnippetManager.sharedManager.loadFolders() {
            if results.count != 0 {
                self.folderTableView.selectRowIndexes(NSIndexSet(index: 0), byExtendingSelection: false)
                self.selectFolder(0)
            }
        }
        */
    }
    
    
    
    // MARK: - Override Methods
    override func showWindow(sender: AnyObject?) {
        NSApp.activateIgnoringOtherApps(true)
        if let window = self.window {
            window.center()
            self.window?.makeKeyAndOrderFront(true)
        }
    }
    
    override func validateToolbarItem(theItem: NSToolbarItem) -> Bool {
        let itemIdentifier = theItem.itemIdentifier
        
        if itemIdentifier == ADD_SNIPPET_IDENTIFIER {
            if let results = CPYSnippetManager.sharedManager.loadFolders() {
                return (results.count > 0) ? true : false
            }
            return false
        } else if itemIdentifier == CHECK_SNIPPET_IDENTIFIER || itemIdentifier == DELETE_SNIPPET_IDENTIFIER {
            if let folders = CPYSnippetManager.sharedManager.loadSortedFolders() {
                /*if folders.count > 0 && self.folderTableView.selectedRow != -1 {
                    if let folder = folders.objectAtIndex(UInt(self.folderTableView.selectedRow)) as? CPYFolder {
                        return (folder.snippets.count > 0)
                    }
                }*/
            }
            return false
        }
        return true
    }
    
    
    
    
    
    
    // MARK: - Private Methods
    private func endEditingForWindow() -> Bool {
        var editingEnded = self.window!.makeFirstResponder(self.window)
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
        
        for folder in folders {
            
            let folderModel = CPYFolder()
            
            if let folderName = folder[kTitle] as? String {
                folderModel.title = folderName
            } else {
                folderModel.title = "untitled folder"
            }
            
            if let lastFolder = CPYSnippetManager.sharedManager.loadSortedFolders()?.lastObject() as? CPYFolder {
                folderModel.index = lastFolder.index + 1
            } else {
                folderModel.index = 0
            }
            
            let realm = RLMRealm.defaultRealm()
            realm.transactionWithBlock( { () -> Void in
                realm.addObject(folderModel)
            })
            
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
                    
                    let realm = RLMRealm.defaultRealm()
                    realm.transactionWithBlock({ () -> Void in
                        folderModel.snippets.addObject(snippetModel)
                    })
                }
                
                self.foldersFromFile = [AnyObject]()
            }
        }
        //self.folderTableView.reloadData()
    }

}


// MARK: - NSXMLParser Delegate
extension CPYSnippetEditorWindowController: NSXMLParserDelegate {
    
    func parser(parser: NSXMLParser, didStartElement elementName: String, namespaceURI: String?, qualifiedName qName: String?, attributes attributeDict: [NSObject : AnyObject]) {
        
        self.currentElementContent = ""
        
        if elementName == kRootElement {
            self.foldersFromFile = [AnyObject]()
        } else if elementName == kFolderElement {
            self.currentFolder = [kType: kFolderElement, kSnippets: [String]()]
        } else if elementName == kSnippetElement {
            self.currentSnippet = [kType: kSnippetElement]
        }
        
    }
    
    func parser(parser: NSXMLParser, foundCharacters string: String?) {
        if string != nil {
            self.currentElementContent += string!
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
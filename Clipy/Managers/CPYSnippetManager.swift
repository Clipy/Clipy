//
//  CPYSnippetManager.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/21.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa

class CPYSnippetManager: NSObject {

    // MARK: - Properties
    static let sharedManager = CPYSnippetManager()
    
    // MARK: - Init
    override init() {
        super.init()
        self.initManager()
    }
    
    private func initManager() {
        
    }
    
    deinit {
        
    }
    
    // MARK: - Public Methods
    func loadFolders() -> RLMResults {
        return CPYFolder.allObjects()
    }
    
    func loadSortedFolders() -> RLMResults {
        return CPYFolder.allObjects().sortedResultsUsingProperty("index", ascending: true)
    }
    
    func removeSnippet(snippet: CPYSnippet) {
        let realm = RLMRealm.defaultRealm()
        realm.transactionWithBlock( { () -> Void in
            realm.deleteObject(snippet)
        })
    }
    
    func addFolder(title: String?) {
        let folder = CPYFolder()
        folder.title = title ?? "untitled folder"
        
        if let lastFolder = self.loadSortedFolders().lastObject() as? CPYFolder {
            folder.index = lastFolder.index + 1
        } else {
            folder.index = 0
        }
        
        let realm = RLMRealm.defaultRealm()
        realm.transactionWithBlock( { () -> Void in
            realm.addObject(folder)
        })
    }
    
    func addSnippet(title: String?, folder: CPYFolder!) {
        let snippet = CPYSnippet()
        snippet.title = "untitled snippet"
        if let lastSnippet = folder.snippets.sortedResultsUsingProperty("index", ascending: true).lastObject() as? CPYSnippet {
            snippet.index = lastSnippet.index + 1
        } else {
            snippet.index = 0
        }
        
        let realm = RLMRealm.defaultRealm()
        realm.transactionWithBlock({ () -> Void in
            folder.snippets.addObject(snippet)
        })
    }
    
    func updateSnipeetContent(snippet: CPYSnippet, content: String) {
        let realm = RLMRealm.defaultRealm()
        realm.transactionWithBlock({ () -> Void in
            snippet.content = content
        })
    }
    
    func removeFolders(folders: [RLMObject]) {
        var snippets = [RLMObject]()
        for folder in folders as! [CPYFolder]{
            for snippet in folder.snippets {
                snippets.append(snippet)
            }
        }
        
        let realm = RLMRealm.defaultRealm()
        realm.transactionWithBlock( { () -> Void in
            realm.deleteObjects(snippets)
            realm.deleteObjects(folders)
        })
    }
    
    func removeSnippets(snippets: [RLMObject]) {
        let realm = RLMRealm.defaultRealm()
        realm.transactionWithBlock( { () -> Void in
            realm.deleteObjects(snippets)
        })
    }
    
    func updateFolderEnable(folders: [RLMObject]) {
        let realm = RLMRealm.defaultRealm()
        realm.transactionWithBlock( { () -> Void in
            for folder in folders as! [CPYFolder] {
                folder.enable = !folder.enable
            }
        })
    }
    
    func updateSnippetEnable(snippets: [RLMObject]) {
        let realm = RLMRealm.defaultRealm()
        realm.transactionWithBlock( { () -> Void in
            for snippet in snippets as! [CPYSnippet] {
                snippet.enable = !snippet.enable
            }
        })
    }
    
    func updateFolderIndex(var toIndex: Int, selectIndexes: NSIndexSet) {
        
        if toIndex > selectIndexes.firstIndex {
            toIndex = toIndex - 1
        }
        
        var folders = [CPYFolder]()
        let reuslts = CPYSnippetManager.sharedManager.loadSortedFolders()
        var index = 0
        var updateFolder: CPYFolder!
        for folder in reuslts {
            if index != selectIndexes.firstIndex {
                folders.append(folder as! CPYFolder)
            } else {
                updateFolder = folder as! CPYFolder
            }
            index = index + 1
        }
        folders.insert(updateFolder, atIndex: toIndex)
        
        index = 0
        for folder in folders {
            let realm = RLMRealm.defaultRealm()
            realm.transactionWithBlock({ () -> Void in
                folder.index = index
            })
            index = index + 1
        }
    }
    
    func updateSnippetIndex(var toIndex: Int, selectIndexes: NSIndexSet, folder: CPYFolder?) {
        if folder == nil {
            return
        }
        
        if toIndex > selectIndexes.firstIndex {
            toIndex = toIndex - 1
        }
        
        var snippets = [CPYSnippet]()
        let reuslts = folder!.snippets.sortedResultsUsingProperty("index", ascending: true)
        var index = 0
        var updateSnippet: CPYSnippet!
        for snippet in reuslts {
            if index != selectIndexes.firstIndex {
                snippets.append(snippet as! CPYSnippet)
            } else {
                updateSnippet = snippet as! CPYSnippet
            }
            index = index + 1
        }
        snippets.insert(updateSnippet, atIndex: toIndex)
        
        index = 0
        for snippet in snippets {
            let realm = RLMRealm.defaultRealm()
            realm.transactionWithBlock({ () -> Void in
                snippet.index = index
            })
            index = index + 1
        }
        
    }
    
}

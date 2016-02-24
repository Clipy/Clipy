//
//  CPYSnippetManager.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/21.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import RealmSwift

class CPYSnippetManager: NSObject {

    // MARK: - Properties
    static let sharedManager = CPYSnippetManager()
    
    // MARK: - Init
    override init() {
        super.init()
    }
    
    // MARK: - Public Methods
    func loadFolders() -> Results<CPYFolder> {
        let realm = try! Realm()
        return realm.objects(CPYFolder)
    }
    
    func loadSortedFolders() -> Results<CPYFolder> {
        let realm = try! Realm()
        return realm.objects(CPYFolder).sorted("index", ascending: true)
    }
    
    func removeSnippet(snippet: CPYSnippet) {
        do {
            let realm = try Realm()
            try realm.write {
                realm.delete(snippet)
            }
        } catch {}
    }
    
    func addFolder(title: String?) {
        let folder = CPYFolder()
        folder.title = title ?? "untitled folder"
        
        if let lastFolder = loadSortedFolders().last {
            folder.index = lastFolder.index + 1
        } else {
            folder.index = 0
        }
        
        do {
            let realm = try Realm()
            try realm.write {
                realm.add(folder, update: true)
            }
        } catch {}
    }
    
    func addSnippet(title: String?, folder: CPYFolder!) {
        let snippet = CPYSnippet()
        snippet.title = "untitled snippet"
        if let lastSnippet = folder.snippets.sorted("index", ascending: true).last {
            snippet.index = lastSnippet.index + 1
        } else {
            snippet.index = 0
        }
        
        do {
            let realm = try Realm()
            try realm.write {
                folder.snippets.append(snippet)
            }
        } catch {}
    }
    
    func updateSnipeetContent(snippet: CPYSnippet, content: String) {
        do {
            let realm = try Realm()
            try realm.write {
                snippet.content = content
            }
        } catch {}
    }
    
    func removeFolders(folders: [CPYFolder]) {
        var snippets = [CPYSnippet]()
        for folder in folders {
            for snippet in folder.snippets {
                snippets.append(snippet)
            }
        }
        
        do {
            let realm = try Realm()
            try realm.write {
                realm.delete(snippets)
                realm.delete(folders)
            }
        } catch {}
    }
    
    func removeSnippets(snippets: [CPYSnippet]) {
        do {
            let realm = try Realm()
            try realm.write {
                realm.delete(snippets)
            }
        } catch {}
    }
    
    func updateFolderEnable(folders: [CPYFolder]) {
        do {
            let realm = try Realm()
            try realm.write {
                for folder in folders {
                    folder.enable = !folder.enable
                }
            }
        } catch {}
    }
    
    func updateSnippetEnable(snippets: [CPYSnippet]) {
        do {
            let realm = try Realm()
            try realm.write {
                for snippet in snippets {
                    snippet.enable = !snippet.enable
                }
            }
        } catch {}
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
                folders.append(folder)
            } else {
                updateFolder = folder
            }
            index = index + 1
        }
        folders.insert(updateFolder, atIndex: toIndex)
        
        index = 0
        for folder in folders {
            do {
                let realm = try Realm()
                try realm.write {
                    folder.index = index
                }
            } catch {}
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
        let reuslts = folder!.snippets.sorted("index", ascending: true)
        var index = 0
        var updateSnippet: CPYSnippet!
        for snippet in reuslts {
            if index != selectIndexes.firstIndex {
                snippets.append(snippet)
            } else {
                updateSnippet = snippet
            }
            index = index + 1
        }
        snippets.insert(updateSnippet, atIndex: toIndex)
        
        index = 0
        for snippet in snippets {
            do {
                let realm = try Realm()
                try realm.write {
                    snippet.index = index
                }
            } catch {}
            index = index + 1
        }
    }
}

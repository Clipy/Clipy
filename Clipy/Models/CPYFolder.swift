//
//  CPYFolder.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/21.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import Realm

class CPYFolder: RLMObject {

    // MARK: - Properties
    dynamic var index       = 0
    dynamic var enable      = true
    dynamic var title       = ""
    dynamic var identifier = NSUUID().UUIDString
    dynamic var snippets    = RLMArray(objectClassName: CPYSnippet.className())

    // MARK: Primary Key
    override class func primaryKey() -> String {
        return "identifier"
    }

    // MARK: - Ignore Properties
    override static func ignoredProperties() -> [String] {
        return ["lastSnippet", "lastFolder"]
    }
}

// MARK: - Copy
extension CPYFolder {
    func deepCopy() -> CPYFolder {
        let folder = CPYFolder(value: self)
        var snippets = [CPYSnippet]()
        folder.snippets.forEach {
            let snippet = CPYSnippet(value: $0)
            snippets.append(snippet)
        }
        folder.snippets.removeAllObjects()
        folder.snippets.addObjects(snippets)
        return folder
    }
}

// MARK: - Add Snippet
extension CPYFolder {
    func createSnippet() -> CPYSnippet {
        let snippet = CPYSnippet()
        snippet.title = "untitled snippet"
        snippet.index = Int(snippets.count)
        return snippet
    }

    func mergeSnippet(snippet: CPYSnippet) {
        guard let folder = CPYFolder(forPrimaryKey: identifier) else { return }
        guard let realm = folder.realm else { return }
        let copySnippet = CPYSnippet(value: snippet)
        realm.transaction { folder.snippets.addObject(copySnippet) }
    }
}

// MARK: - Add Folder
extension CPYFolder {
    static func create() -> CPYFolder {
        let folder = CPYFolder()
        folder.title = "untitled folder"
        let lastFolder = CPYFolder.allObjects().sortedResultsUsingProperty("index", ascending: true).lastObject() as? CPYFolder
        folder.index = lastFolder?.index ?? -1
        folder.index += 1
        return folder
    }

    func merge() {
        if let folder = CPYFolder(forPrimaryKey: identifier) {
            folder.realm?.transaction {
                folder.index = index
                folder.enable = enable
                folder.title = title
            }
        } else {
            let realm = RLMRealm.defaultRealm()
            let copyFolder = CPYFolder(value: self)
            realm.transaction { realm.addObject(copyFolder) }
        }
    }
}

// MARK: - Remove Folder
extension CPYFolder {
    func remove() {
        guard let folder = CPYFolder(forPrimaryKey: identifier) else { return }
        guard let realm = folder.realm else { return }
        realm.transaction { realm.deleteObjects(folder.snippets) }
        realm.transaction { realm.deleteObject(folder) }
    }
}

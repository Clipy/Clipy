//
//  CPYFolder.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/21.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import RealmSwift

final class CPYFolder: Object {

    // MARK: - Properties
    dynamic var index       = 0
    dynamic var enable      = true
    dynamic var title       = ""
    dynamic var identifier  = NSUUID().UUIDString
    let snippets            = List<CPYSnippet>()

    // MARK: Primary Key
    override static func primaryKey() -> String? {
        return "identifier"
    }

}

// MARK: - Copy
extension CPYFolder {
    func deepCopy() -> CPYFolder {
        let folder = CPYFolder(value: self)
        var snippets = [CPYSnippet]()
        if realm == nil {
            snippets.forEach {
                let snippet = CPYSnippet(value: $0)
                snippets.append(snippet)
            }
        } else {
            self.snippets.sorted("index", ascending: true).forEach {
                let snippet = CPYSnippet(value: $0)
                snippets.append(snippet)
            }
        }
        folder.snippets.removeAll()
        folder.snippets.appendContentsOf(snippets)
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
        let realm = try! Realm()
        guard let folder = realm.objectForPrimaryKey(CPYFolder.self, key: identifier) else { return }
        let copySnippet = CPYSnippet(value: snippet)
        folder.realm?.transaction { folder.snippets.append(copySnippet) }
    }

    func insertSnippet(snippet: CPYSnippet, index: Int) {
        let realm = try! Realm()
        guard let folder = realm.objectForPrimaryKey(CPYFolder.self, key: identifier) else { return }
        guard let savedSnippet = realm.objectForPrimaryKey(CPYSnippet.self, key: snippet.identifier) else { return }
        folder.realm?.transaction { folder.snippets.insert(savedSnippet, atIndex: index) }
        folder.rearrangesSnippetIndex()
    }

    func removeSnippet(snippet: CPYSnippet) {
        let realm = try! Realm()
        guard let folder = realm.objectForPrimaryKey(CPYFolder.self, key: identifier) else { return }
        guard let savedSnippet = realm.objectForPrimaryKey(CPYSnippet.self, key: snippet.identifier), let index = folder.snippets.indexOf(savedSnippet) else { return }
        folder.realm?.transaction { folder.snippets.removeAtIndex(index) }
        folder.rearrangesSnippetIndex()
    }
}

// MARK: - Add Folder
extension CPYFolder {
    static func create() -> CPYFolder {
        let realm = try! Realm()
        let folder = CPYFolder()
        folder.title = "untitled folder"
        let lastFolder = realm.objects(CPYFolder.self).sorted("index", ascending: true).last
        folder.index = lastFolder?.index ?? -1
        folder.index += 1
        return folder
    }

    func merge() {
        let realm = try! Realm()
        if let folder = realm.objectForPrimaryKey(CPYFolder.self, key: identifier) {
            folder.realm?.transaction {
                folder.index = index
                folder.enable = enable
                folder.title = title
            }
        } else {
            let copyFolder = CPYFolder(value: self)
            realm.transaction { realm.add(copyFolder, update: true) }
        }
    }
}

// MARK: - Remove Folder
extension CPYFolder {
    func remove() {
        let realm = try! Realm()
        guard let folder = realm.objectForPrimaryKey(CPYFolder.self, key: identifier) else { return }
        folder.realm?.transaction { folder.realm?.delete(folder.snippets) }
        folder.realm?.transaction { folder.realm?.delete(folder) }
    }
}

// MARK: - Migrate Index
extension CPYFolder {
    static func rearrangesIndex(folders: [CPYFolder]) {
        for (index, folder) in folders.enumerate() {
            if folder.realm == nil { folder.index = index }
            let realm = try! Realm()
            guard let savedFolder = realm.objectForPrimaryKey(CPYFolder.self, key: folder.identifier) else { return }
            savedFolder.realm?.transaction {
                savedFolder.index = index
            }
        }
    }

    func rearrangesSnippetIndex() {
        for (index, snippet) in snippets.enumerate() {
            if snippet.realm == nil { snippet.index = index }
            let realm = try! Realm()
            guard let savedSnippet = realm.objectForPrimaryKey(CPYSnippet.self, key: snippet.identifier) else { return }
            savedSnippet.realm?.transaction {
                savedSnippet.index = index
            }
        }
    }
}

//
//  CPYFolder.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2015/06/21.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Cocoa
import RealmSwift

final class CPYFolder: Object {

    // MARK: - Properties
    @objc dynamic var index = 0
    @objc dynamic var enable = true
    @objc dynamic var title = ""
    @objc dynamic var identifier = UUID().uuidString
    let snippets = List<CPYSnippet>()

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
            self.snippets.sorted(byKeyPath: #keyPath(CPYSnippet.index), ascending: true).forEach {
                let snippet = CPYSnippet(value: $0)
                snippets.append(snippet)
            }
        }
        folder.snippets.removeAll()
        folder.snippets.append(objectsIn: snippets)
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

    func mergeSnippet(_ snippet: CPYSnippet) {
        let realm = try! Realm()
        guard let folder = realm.object(ofType: CPYFolder.self, forPrimaryKey: identifier) else { return }
        let copySnippet = CPYSnippet(value: snippet)
        folder.realm?.transaction { folder.snippets.append(copySnippet) }
    }

    func insertSnippet(_ snippet: CPYSnippet, index: Int) {
        let realm = try! Realm()
        guard let folder = realm.object(ofType: CPYFolder.self, forPrimaryKey: identifier) else { return }
        guard let savedSnippet = realm.object(ofType: CPYSnippet.self, forPrimaryKey: snippet.identifier) else { return }
        folder.realm?.transaction { folder.snippets.insert(savedSnippet, at: index) }
        folder.rearrangesSnippetIndex()
    }

    func removeSnippet(_ snippet: CPYSnippet) {
        let realm = try! Realm()
        guard let folder = realm.object(ofType: CPYFolder.self, forPrimaryKey: identifier) else { return }
        guard let savedSnippet = realm.object(ofType: CPYSnippet.self, forPrimaryKey: snippet.identifier), let index = folder.snippets.index(of: savedSnippet) else { return }
        folder.realm?.transaction { folder.snippets.remove(at: index) }
        folder.rearrangesSnippetIndex()
    }
}

// MARK: - Add Folder
extension CPYFolder {
    static func create() -> CPYFolder {
        let realm = try! Realm()
        let folder = CPYFolder()
        folder.title = "untitled folder"
        let lastFolder = realm.objects(CPYFolder.self).sorted(byKeyPath: #keyPath(CPYFolder.index), ascending: true).last
        folder.index = lastFolder?.index ?? -1
        folder.index += 1
        return folder
    }

    func merge() {
        let realm = try! Realm()
        if let folder = realm.object(ofType: CPYFolder.self, forPrimaryKey: identifier) {
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
        guard let folder = realm.object(ofType: CPYFolder.self, forPrimaryKey: identifier) else { return }
        folder.realm?.transaction { folder.realm?.delete(folder.snippets) }
        folder.realm?.transaction { folder.realm?.delete(folder) }
    }
}

// MARK: - Migrate Index
extension CPYFolder {
    static func rearrangesIndex(_ folders: [CPYFolder]) {
        for (index, folder) in folders.enumerated() {
            if folder.realm == nil { folder.index = index }
            let realm = try! Realm()
            guard let savedFolder = realm.object(ofType: CPYFolder.self, forPrimaryKey: folder.identifier) else { return }
            savedFolder.realm?.transaction {
                savedFolder.index = index
            }
        }
    }

    func rearrangesSnippetIndex() {
        for (index, snippet) in snippets.enumerated() {
            if snippet.realm == nil { snippet.index = index }
            let realm = try! Realm()
            guard let savedSnippet = realm.object(ofType: CPYSnippet.self, forPrimaryKey: snippet.identifier) else { return }
            savedSnippet.realm?.transaction {
                savedSnippet.index = index
            }
        }
    }
}

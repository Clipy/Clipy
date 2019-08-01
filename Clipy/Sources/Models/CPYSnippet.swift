//
//  CPYSnippet.swift
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

final class CPYSnippet: Object {

    // MARK: - Properties
    @objc dynamic var index = 0
    @objc dynamic var enable = true
    @objc dynamic var title = ""
    @objc dynamic var content = ""
    @objc dynamic var identifier = UUID().uuidString
    let folders = LinkingObjects(fromType: CPYFolder.self, property: "snippets")

    var folder: CPYFolder? {
        return folders.first
    }

    // MARK: Primary Key
    override static func primaryKey() -> String? {
        return "identifier"
    }

    // MARK: - Ignore Properties
    override static func ignoredProperties() -> [String] {
        return ["folder"]
    }

}

// MARK: - Add Snippet
extension CPYSnippet {
    func merge() {
        let realm = try! Realm()
        let copySnippet = CPYSnippet(value: self)
        realm.transaction { realm.add(copySnippet, update: .modified) }
    }
}

// MARK: - Remove Snippet
extension CPYSnippet {
    func remove() {
        let realm = try! Realm()
        guard let snippet = realm.object(ofType: CPYSnippet.self, forPrimaryKey: identifier) else { return }
        snippet.realm?.transaction { snippet.realm?.delete(snippet) }
    }
}

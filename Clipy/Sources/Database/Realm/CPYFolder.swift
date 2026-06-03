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

//
//  CPYClip.swift
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

final class CPYClip: Object {

    // MARK: - Properties
    @objc dynamic var dataPath = ""
    @objc dynamic var title = ""
    @objc dynamic var dataHash = ""
    @objc dynamic var primaryType = ""
    @objc dynamic var updateTime = 0
    @objc dynamic var thumbnailPath = ""
    @objc dynamic var isColorCode = false
    // Pinned index
    @objc dynamic var pinIndex = 0

    // MARK: Primary Key
    override static func primaryKey() -> String? {
        return "dataHash"
    }

    var isPinned: Bool {
        return pinIndex > 0
    }

    static let predicateNotPinned = NSPredicate(format: "pinIndex = 0")
    static let predicatePinned = NSPredicate(format: "pinIndex > 0")
    static let predicateAny = NSPredicate(value: true)
}

//
//  CPYClip.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/21.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import RealmSwift

final class CPYClip: Object {

    // MARK: - Properties
    @objc dynamic var dataPath        = ""
    @objc dynamic var title           = ""
    @objc dynamic var dataHash        = ""
    @objc dynamic var primaryType     = ""
    @objc dynamic var updateTime      = 0
    @objc dynamic var thumbnailPath   = ""
    @objc dynamic var isColorCode     = false

    // MARK: Primary Key
    override static func primaryKey() -> String? {
        return "dataHash"
    }

}

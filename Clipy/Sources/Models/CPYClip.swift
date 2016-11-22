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
    dynamic var dataPath        = ""
    dynamic var title           = ""
    dynamic var dataHash        = ""
    dynamic var primaryType     = ""
    dynamic var updateTime      = 0
    dynamic var thumbnailPath   = ""
    dynamic var isColorCode     = false

    // MARK: Primary Key
    override static func primaryKey() -> String? {
        return "dataHash"
    }

}

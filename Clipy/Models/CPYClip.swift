//
//  CPYClip.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/21.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa

class CPYClip: RLMObject {

    dynamic var dataPath    = ""
    dynamic var title       = ""
    dynamic var dataHash    = ""
    dynamic var primaryType = ""
    dynamic var updateTime  = 0
    
    // MARK: Primary Key
    override class func primaryKey() -> String {
        return "dataHash"
    }
    
}

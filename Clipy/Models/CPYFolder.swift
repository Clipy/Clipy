//
//  CPYFolder.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/21.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa

class CPYFolder: RLMObject {

    // MARK: - Properties
    dynamic var index       = 0
    dynamic var enable      = true
    dynamic var title       = ""
    dynamic var snippets    = RLMArray(objectClassName: CPYSnippet.className())
    
}

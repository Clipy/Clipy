//
//  NSLock+Clipy.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/01/20.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation

extension NSRecursiveLock {
    public convenience init(name: String) {
        self.init()
        self.name = name
    }
}

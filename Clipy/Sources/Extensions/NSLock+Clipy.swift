//
//  NSLock+Clipy.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2016/01/20.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Foundation

extension NSRecursiveLock {
    public convenience init(name: String) {
        self.init()
        self.name = name
    }
}

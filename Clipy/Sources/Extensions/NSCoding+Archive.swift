//
//  NSCoding+Archive.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/11/19.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation

extension NSCoding {
    func archive() -> Data {
        return NSKeyedArchiver.archivedData(withRootObject: self)
    }
}

extension Array where Element: NSCoding {
    func archive() -> Data {
        return NSKeyedArchiver.archivedData(withRootObject: self)
    }
}

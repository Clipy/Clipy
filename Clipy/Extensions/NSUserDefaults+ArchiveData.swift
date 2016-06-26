//
//  NSUserDefaults+ArchiveData.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/06/23.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation
import Cocoa

extension NSUserDefaults {
    func setArchiveData<T: NSCoding>(object: T, forKey key: String) {
        let data = NSKeyedArchiver.archivedDataWithRootObject(object)
        setObject(data, forKey: key)
    }

    func archiveDataForKey<T: NSCoding>(_: T.Type, key: String) -> T? {
        guard let data = objectForKey(key) as? NSData else { return nil }
        guard let object = NSKeyedUnarchiver.unarchiveObjectWithData(data) as? T else { return nil }
        return object
    }
}

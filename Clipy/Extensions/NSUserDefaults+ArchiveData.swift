//
//  NSUserDefaults+ArchiveData.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/06/23.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation
import Cocoa

extension UserDefaults {
    func setArchiveData<T: NSCoding>(_ object: T, forKey key: String) {
        let data = NSKeyedArchiver.archivedData(withRootObject: object)
        set(data, forKey: key)
    }

    func archiveDataForKey<T: NSCoding>(_: T.Type, key: String) -> T? {
        guard let data = object(forKey: key) as? Data else { return nil }
        guard let object = NSKeyedUnarchiver.unarchiveObject(with: data) as? T else { return nil }
        return object
    }
}

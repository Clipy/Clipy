//
//  CPYAppInfo.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/08/08.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa

final class CPYAppInfo: NSObject, NSCoding {

    // MARK: - Properties
    let identifier: String
    let name: String

    init?(info: [String: AnyObject]) {
        guard let identifier = info[kCFBundleIdentifierKey as String] as? String else { return nil }
        guard let name = info[kCFBundleNameKey as String] as? String ?? info[kCFBundleExecutableKey as String] as? String else { return nil }

        self.identifier = identifier
        self.name = name
    }

    // MARK: - NSCoding
    init?(coder aDecoder: NSCoder) {
        guard let identifier = aDecoder.decodeObjectForKey("identifier") as? String else { return nil }
        guard let name = aDecoder.decodeObjectForKey("name") as? String else { return nil }

        self.identifier = identifier
        self.name       = name
    }

    func encodeWithCoder(aCoder: NSCoder) {
        aCoder.encodeObject(identifier, forKey: "identifier")
        aCoder.encodeObject(name, forKey: "name")
    }

    // MARK: - Equatable
    override func isEqual(object: AnyObject?) -> Bool {
        guard let object = object as? CPYAppInfo else { return false }
        return identifier == object.identifier && name == object.name
    }

}

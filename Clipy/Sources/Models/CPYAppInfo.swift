//
//  CPYAppInfo.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2016/08/08.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Cocoa

final class CPYAppInfo: NSObject, NSCoding {

    // MARK: - Properties
    let identifier: String
    let name: String

    // MARK: - Initialize
    init?(info: [String: AnyObject]) {
        guard let identifier = info[kCFBundleIdentifierKey as String] as? String else { return nil }
        guard let name = info[kCFBundleNameKey as String] as? String ?? info[kCFBundleExecutableKey as String] as? String else { return nil }

        self.identifier = identifier
        self.name = name
    }

    // MARK: - NSCoding
    init?(coder aDecoder: NSCoder) {
        guard let identifier = aDecoder.decodeObject(forKey: "identifier") as? String else { return nil }
        guard let name = aDecoder.decodeObject(forKey: "name") as? String else { return nil }

        self.identifier = identifier
        self.name = name
    }

    func encode(with aCoder: NSCoder) {
        aCoder.encode(identifier, forKey: "identifier")
        aCoder.encode(name, forKey: "name")
    }

    // MARK: - Equatable
    override func isEqual(_ object: Any?) -> Bool {
        guard let object = object as? CPYAppInfo else { return false }
        return identifier == object.identifier && name == object.name
    }

}

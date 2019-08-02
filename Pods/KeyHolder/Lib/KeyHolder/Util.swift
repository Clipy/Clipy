//
//  Util.swift
//
//  KeyHolder
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2016/06/18.
//
//  Copyright © 2016-2018 Clipy Project.
//

import Cocoa

final class Util {
    static func bundleImage(name: String) -> NSImage? {
        var bundle = Bundle(identifier: "com.clipy-app.KeyHolder")
        if bundle == nil {
            let frameworkBundle = Bundle(for: RecordView.self)
            let path = frameworkBundle.path(forResource: "KeyHolder", ofType: "bundle")!
            bundle = Bundle(path: path)
        }
        guard let resourceBundle = bundle else { return nil }
        return resourceBundle.image(forResource: name)
    }
}

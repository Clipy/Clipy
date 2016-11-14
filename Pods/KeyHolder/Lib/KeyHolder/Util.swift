//
//  Util.swift
//  KeyHolder
//
//  Created by 古林俊佑 on 2016/06/18.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
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

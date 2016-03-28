//
//  NSBundle+Version.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/03/29.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation

extension NSBundle {
    var appVersion: String? {
        return infoDictionary?["CFBundleShortVersionString"] as? String
    }
}

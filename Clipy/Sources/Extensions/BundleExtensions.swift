//
//  BundleExtensions.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2016/03/29.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Foundation

extension Bundle {
    var appName: String? {
        guard let name = infoDictionary?["CFBundleName"] as? String else { return nil }
        #if DEBUG
        return name + "DEBUG"
        #else
        return name
        #endif
    }

    var appVersion: String? {
        return infoDictionary?["CFBundleShortVersionString"] as? String
    }
}

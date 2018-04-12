//
//  NSColor+Clipy.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2016/02/25.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Foundation
import Cocoa

extension NSColor {
    static func clipyColor() -> NSColor {
        return NSColor(red: 0.164, green: 0.517, blue: 0.823, alpha: 1)
    }

    static func titleColor() -> NSColor {
        return NSColor(white: 0.266, alpha: 1)
    }

    static func tabTitleColor() -> NSColor {
        return NSColor(white: 0.6, alpha: 1)
    }
}

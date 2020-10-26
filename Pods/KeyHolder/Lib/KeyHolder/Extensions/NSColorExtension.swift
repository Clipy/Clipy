//
//  NSColorExtension.swift
//
//  KeyHolder
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Copyright © 2015-2020 Clipy Project.
//

import AppKit

extension NSColor {
    static let controlAccentPolyfill: NSColor = {
        if #available(macOS 10.14, *) {
            return NSColor.controlAccentColor
        } else {
            // nmacOS 10.14 polyfill
            return NSColor(red: 0.10, green: 0.47, blue: 0.98, alpha: 1)
        }
    }()
    static let clearBackgroundFill: NSColor = {
        return NSColor(red: 0.749019608, green: 0.749019608, blue: 0.749019608, alpha: 1)
    }()
    static let clearHighlightedBackgroundFill: NSColor = {
        return NSColor(red: 0.525490196, green: 0.525490196, blue: 0.525490196, alpha: 1)
    }()
}

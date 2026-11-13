//
//  NSImage+Extensions.swift
//  AlternativeMenu
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit

extension NSImage {
    var isSymbolImage: Bool {
        let key = "_isSymbolImage"
        if responds(to: NSSelectorFromString(key)), let result = value(forKey: key) as? Bool {
            return result
        }

        let small = withSymbolConfiguration(.init(scale: .small)) ?? self
        let large = withSymbolConfiguration(.init(scale: .large)) ?? self
        return small.size != large.size
    }
}

//
//  NSMenuItemExtensions.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2016/03/06.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Foundation
import Cocoa

extension NSMenuItem {
    convenience init(title: String, action: Selector?) {
        self.init(title: title, action: action, keyEquivalent: "")
    }

    convenience init(
        attributedTitle: NSAttributedString,
        action: Selector,
        representedObject: Any,
        keyEquivalent: String = "",
        image: NSImage?,
        toolTip: String?
    ) {
        self.init(title: attributedTitle.string, action: action, keyEquivalent: keyEquivalent)
        self.attributedTitle = attributedTitle
        self.representedObject = representedObject
        self.image = image
        self.toolTip = toolTip
        preferVisibleImage()
    }

    func preferVisibleImage() {
        guard #available(macOS 27, *), image != nil else { return }
        preferredImageVisibility = .visible
    }
}

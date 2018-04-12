//
//  NSMenuItem+Initialize.swift
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
}

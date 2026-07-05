//
//  SnippetExtensions.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/07/05.
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import Dependencies
import Sharing

extension Snippet {
    var toolTip: String? {
        @Dependency(\.defaultAppStorage) var appStorage

        guard appStorage.bool(forKey: Constants.UserDefaults.showToolTipOnMenuItem) else { return nil }

        let maxLengthOfToolTip = appStorage.integer(forKey: Constants.UserDefaults.maxLengthOfToolTip)
        return String(content.prefix(maxLengthOfToolTip))
    }
}

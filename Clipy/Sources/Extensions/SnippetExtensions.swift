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
import Sharing

extension Snippet {
    var toolTip: String? {
        @Shared(.showsToolTipsOnMenuItems) var showsToolTipsOnMenuItems
        @Shared(.maximumToolTipLength) var maximumToolTipLength

        guard showsToolTipsOnMenuItems else { return nil }

        return String(content.prefix(maximumToolTipLength))
    }
}

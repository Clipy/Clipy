//
//  PasteboardHistoryExtensions.swift
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

extension PasteboardHistory {
    var typedTitle: String {
        let primaryType = pasteboardTypes.first
        let prefix: String?
        if primaryType == .png || primaryType == .tiff || primaryType == .deprecatedTIFF {
            prefix = "(Image)"
        } else if primaryType == .pdf || primaryType == .deprecatedPDF {
            prefix = "(PDF)"
        } else if primaryType == .fileURL || primaryType == .deprecatedFilenames {
            prefix = "(Files)"
        } else {
            prefix = nil
        }
        return [prefix, title.trimmedMenuTitle]
            .compactMap { $0 }
            .filter { !$0.isEmpty }
            .joined(separator: " ")
    }
    var toolTip: String? {
        @Shared(.showsToolTipsOnMenuItems) var showsToolTipsOnMenuItems
        @Shared(.maximumToolTipLength) var maximumToolTipLength

        guard showsToolTipsOnMenuItems else { return nil }

        return String(title.prefix(maximumToolTipLength))
    }
}

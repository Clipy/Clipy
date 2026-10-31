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
    var primaryAvailableType: PasteboardAvailableType? {
        pasteboardTypes.first?.availableType
    }
    var menuTitle: String {
        let trimmedTitle = title.trimmedMenuTitle
        guard !trimmedTitle.isEmpty else {
            return primaryAvailableType?.fallbackTitle.map { "(\($0))" } ?? ""
        }
        return trimmedTitle
    }
    var toolTip: String? {
        @Shared(.showsToolTipsOnMenuItems) var showsToolTipsOnMenuItems
        @Shared(.maximumToolTipLength) var maximumToolTipLength

        guard showsToolTipsOnMenuItems else { return nil }

        return String(title.prefix(maximumToolTipLength))
    }
}

private extension PasteboardAvailableType {
    var fallbackTitle: String? {
        switch self {
        case .filenames:
            return "Files"
        case .tiff:
            return "Image"
        case .pdf:
            return "PDF"
        case .string, .rtf, .rtfd, .html, .url:
            return nil
        }
    }
}

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
import Dependencies
import Sharing

extension PasteboardHistory {
    /// Prefix marking non-text clips, e.g. "(Image)", "(PDF)", "(Files)".
    private var typePrefix: String? {
        let primaryType = pasteboardTypes.first
        if primaryType == .png || primaryType == .tiff || primaryType == .deprecatedTIFF {
            return "(Image)"
        } else if primaryType == .pdf || primaryType == .deprecatedPDF {
            return "(PDF)"
        } else if primaryType == .fileURL || primaryType == .deprecatedFilenames {
            return "(Files)"
        }
        return nil
    }

    var typedTitle: String {
        return [typePrefix, title.trimmedMenuTitle]
            .compactMap { $0 }
            .filter { !$0.isEmpty }
            .joined(separator: " ")
    }

    /// Prefix + the full (multi-line) clip text with only the outer whitespace
    /// trimmed, for UIs that linearize and truncate the text themselves — unlike
    /// `typedTitle`, this keeps interior line breaks and applies no length cap.
    var fullDisplaySource: String {
        let body = title.trimmingCharacters(in: .whitespacesAndNewlines)
        return [typePrefix, body]
            .compactMap { $0 }
            .filter { !$0.isEmpty }
            .joined(separator: " ")
    }
    var toolTip: String? {
        @Dependency(\.defaultAppStorage) var appStorage

        guard appStorage.bool(forKey: Constants.UserDefaults.showToolTipOnMenuItem) else { return nil }

        let maxLengthOfToolTip = appStorage.integer(forKey: Constants.UserDefaults.maxLengthOfToolTip)
        return String(title.prefix(maxLengthOfToolTip))
    }
}

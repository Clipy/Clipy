//
//  StringExtensions.swift
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

extension String {
    private static let shortenSymbol = "…"

    var trimmedMenuTitle: String {
        @Shared(.maximumMenuItemTitleLength) var maximumMenuItemTitleLength

        return trimmedMenuTitle(maximumLength: maximumMenuItemTitleLength)
    }

    func trimmedSearchSnippetMenuTitle(folderTitle: String) -> String {
        @Shared(.maximumMenuItemTitleLength) var maximumMenuItemTitleLength

        let maximumLength = maximumMenuItemTitleLength
        let folderLimit = Int(Double(maximumLength) * 0.3)
        let folderTitle = folderTitle.trimmedMenuTitle(maximumLength: folderLimit)
        guard !folderTitle.isEmpty else {
            return trimmedMenuTitle
        }

        let separator = " - "
        let title = trimmedMenuTitle(maximumLength: maximumLength - separator.count - folderTitle.count)
        return title + separator + folderTitle
    }

    private func trimmedMenuTitle(maximumLength: Int) -> String {
        let maximumLength = Swift.max(maximumLength, 1)
        var title = trimmingCharacters(in: .whitespacesAndNewlines)
        let firstLineEnd = title.firstIndex(where: \.isNewline) ?? title.endIndex
        title = String(title[..<firstLineEnd])

        guard title.count > maximumLength else {
            return title
        }
        return String(title.prefix(maximumLength)) + Self.shortenSymbol
    }
}

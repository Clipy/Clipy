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
import Dependencies
import Sharing

extension String {
    var trimmedMenuTitle: String {
        @Dependency(\.defaultAppStorage) var appStorage

        var title = trimmingCharacters(in: .whitespacesAndNewlines)
        let firstLineEnd = title.firstIndex(where: \.isNewline) ?? title.endIndex
        title = String(title[..<firstLineEnd])

        let shortenSymbol = "..."
        let maxMenuItemTitleLength = Swift.max(
            appStorage.integer(forKey: Constants.UserDefaults.maxMenuItemTitleLength),
            shortenSymbol.count
        )
        guard title.count > maxMenuItemTitleLength else {
            return title
        }
        return String(title.prefix(maxMenuItemTitleLength - shortenSymbol.count)) + shortenSymbol
    }
}

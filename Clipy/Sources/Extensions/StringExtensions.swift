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

    /// Wraps the receiver in an FTS5 phrase so that it is matched literally.
    ///
    /// Characters such as `"`, `*`, `:`, `-`, `(`, `)` and bare keywords like `NEAR` are part of the
    /// FTS5 query syntax. Passing raw user input to `MATCH` makes SQLite throw a syntax error, which
    /// `withErrorReporting` swallows into an empty result. Quoting the whole input as a single phrase
    /// (with embedded double quotes doubled) keeps every query valid.
    var fts5PhraseQuery: String {
        "\"\(replacingOccurrences(of: "\"", with: "\"\""))\""
    }
}

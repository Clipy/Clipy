//
//  StringExtensionsTests.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/07/05.
//
//  Copyright © 2015-2026 Clipy Project.
//

import Dependencies
import Sharing
import Testing
@testable import Clipy

@Suite(.dependencies)
struct StringExtensionsTests {
    @Test
    func trimmedMenuTitleUsesFirstTrimmedLine() {
        @Shared(.maximumMenuItemTitleLength) var maximumMenuItemTitleLength = 20

        #expect("  First line\nSecond line  ".trimmedMenuTitle == "First line")
    }

    @Test
    func trimmedMenuTitleShortensLongTitles() {
        @Shared(.maximumMenuItemTitleLength) var maximumMenuItemTitleLength = 8

        #expect("Clipboard title".trimmedMenuTitle == "Clipboar…")
        #expect("12345678".trimmedMenuTitle == "12345678")
        #expect("123456789".trimmedMenuTitle == "12345678…")
    }

    @Test(arguments: [
        (1, "A", "B", "A - B"),
        (2, "A", "B", "A - B"),
        (8, "Clipboard title", "Long folder", "Cl… - Lo…"),
        (9, "Clipboard title", "Long folder", "Cli… - Lo…")
    ])
    func combinedTitleKeepsTheFolderEvenAtSmallLimits(maxLength: Int, title: String, folder: String, expected: String) {
        @Shared(.maximumMenuItemTitleLength) var maximumMenuItemTitleLength = maxLength

        #expect(title.trimmedSearchSnippetMenuTitle(folderTitle: folder) == expected)
    }
}

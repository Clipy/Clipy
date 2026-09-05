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

        #expect("Clipboard title".trimmedMenuTitle == "Clipb...")
    }

    @Test(arguments: [0, 1, 2, 3])
    func trimmedMenuTitleShortensToSymbolWhenMaxLengthIsThreeOrLess(maxLength: Int) {
        @Shared(.maximumMenuItemTitleLength) var maximumMenuItemTitleLength = maxLength

        #expect("Clipboard title".trimmedMenuTitle == "...")
    }
}

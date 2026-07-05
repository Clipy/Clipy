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
        withDependencies {
            $0.defaultAppStorage.set(20, forKey: Constants.UserDefaults.maxMenuItemTitleLength)
        } operation: {
            #expect("  First line\nSecond line  ".trimmedMenuTitle == "First line")
        }
    }

    @Test
    func trimmedMenuTitleShortensLongTitles() {
        withDependencies {
            $0.defaultAppStorage.set(8, forKey: Constants.UserDefaults.maxMenuItemTitleLength)
        } operation: {
            #expect("Clipboard title".trimmedMenuTitle == "Clipb...")
        }
    }

    @Test(arguments: [0, 1, 2, 3])
    func trimmedMenuTitleShortensToSymbolWhenMaxLengthIsThreeOrLess(maxLength: Int) {
        withDependencies {
            $0.defaultAppStorage.set(maxLength, forKey: Constants.UserDefaults.maxMenuItemTitleLength)
        } operation: {
            #expect("Clipboard title".trimmedMenuTitle == "...")
        }
    }
}

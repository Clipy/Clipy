//
//  SnippetExtensionsTests.swift
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
import Testing
@testable import Clipy

@Suite(.dependencies)
struct SnippetExtensionsTests {
    private let snippet = Snippet(
        id: Snippet.ID(rawValue: UUID()),
        folderID: SnippetFolder.ID(rawValue: UUID()),
        title: "Snippet",
        content: "Snippet content",
        index: 0,
        isEnabled: true
    )

    @Test
    func toolTipShowsFullContentWhenEnabled() {
        withDependencies {
            $0.defaultAppStorage.set(true, forKey: Constants.UserDefaults.showToolTipOnMenuItem)
            $0.defaultAppStorage.set(20, forKey: Constants.UserDefaults.maxLengthOfToolTip)
        } operation: {
            #expect(snippet.toolTip == "Snippet content")
        }
    }

    @Test
    func toolTipShortensWhenEnabled() {
        withDependencies {
            $0.defaultAppStorage.set(true, forKey: Constants.UserDefaults.showToolTipOnMenuItem)
            $0.defaultAppStorage.set(9, forKey: Constants.UserDefaults.maxLengthOfToolTip)
        } operation: {
            #expect(snippet.toolTip == "Snippet c")
        }
    }

    @Test
    func toolTipIsNilWhenDisabled() {
        withDependencies {
            $0.defaultAppStorage.set(false, forKey: Constants.UserDefaults.showToolTipOnMenuItem)
            $0.defaultAppStorage.set(9, forKey: Constants.UserDefaults.maxLengthOfToolTip)
        } operation: {
            #expect(snippet.toolTip == nil)
        }
    }
}

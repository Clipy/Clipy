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
        @Shared(.showsToolTipsOnMenuItems) var showsToolTipsOnMenuItems = true
        @Shared(.maximumToolTipLength) var maximumToolTipLength = 20

        #expect(snippet.toolTip == "Snippet content")
    }

    @Test
    func toolTipShortensWhenEnabled() {
        @Shared(.showsToolTipsOnMenuItems) var showsToolTipsOnMenuItems = true
        @Shared(.maximumToolTipLength) var maximumToolTipLength = 9

        #expect(snippet.toolTip == "Snippet c")
    }

    @Test
    func toolTipIsNilWhenDisabled() {
        @Shared(.showsToolTipsOnMenuItems) var showsToolTipsOnMenuItems = false

        #expect(snippet.toolTip == nil)
    }
}

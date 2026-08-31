//
//  AppStorageValuesTests.swift
//
//  ClipyTests
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/08/08.
//
//  Copyright © 2015-2026 Clipy Project.
//

import DependenciesTestSupport
import Sharing
import Testing
@testable import Clipy

@Suite(.dependencies)
struct AppStorageValuesTests {
    @Test
    func loadDefaultPasteboardTypeSettings() {
        @Shared(.pasteboardTypeSettings) var pasteboardTypeSettings

        #expect(PasteboardAvailableType.allCases.allSatisfy { pasteboardTypeSettings[$0] })
    }

    @Test
    func loadDefaultKeyCombos() throws {
        @Shared(.mainKeyCombo) var mainKeyCombo
        @Shared(.historyKeyCombo) var historyKeyCombo
        @Shared(.snippetKeyCombo) var snippetKeyCombo
        @Shared(.editSnippetsKeyCombo) var editSnippetsKeyCombo
        @Shared(.clearHistoryKeyCombo) var clearHistoryKeyCombo
        @Shared(.folderKeyCombos) var folderKeyCombos

        let defaultMainKeyCombo = try #require(mainKeyCombo)
        #expect(defaultMainKeyCombo.QWERTYKeyCode == 9)
        #expect(defaultMainKeyCombo.modifiers == 768)

        let defaultHistoryKeyCombo = try #require(historyKeyCombo)
        #expect(defaultHistoryKeyCombo.QWERTYKeyCode == 9)
        #expect(defaultHistoryKeyCombo.modifiers == 4352)

        let defaultSnippetKeyCombo = try #require(snippetKeyCombo)
        #expect(defaultSnippetKeyCombo.QWERTYKeyCode == 11)
        #expect(defaultSnippetKeyCombo.modifiers == 768)

        #expect(editSnippetsKeyCombo == nil)
        #expect(clearHistoryKeyCombo == nil)
        #expect(folderKeyCombos.isEmpty)
    }

    @Test
    func loadDefaultExcludedApplications() {
        @Shared(.excludedApplications) var excludedApplications

        #expect(excludedApplications.isEmpty)
    }
}

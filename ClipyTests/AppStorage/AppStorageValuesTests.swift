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
    func loadDefaultBooleanSettings() {
        @Shared(.isLaunchAtLogin) var isLaunchAtLogin
        @Shared(.suppressesLoginItemAlert) var suppressesLoginItemAlert
        @Shared(.pastesAutomatically) var pastesAutomatically
        @Shared(.reordersClipsAfterPasting) var reordersClipsAfterPasting
        @Shared(.collectsCrashReports) var collectsCrashReports
        @Shared(.startsMenuItemTitlesAtZero) var startsMenuItemTitlesAtZero
        @Shared(.showsClearHistoryAlert) var showsClearHistoryAlert
        @Shared(.showsClearHistoryMenuItem) var showsClearHistoryMenuItem
        @Shared(.showsIconsInMenu) var showsIconsInMenu
        @Shared(.marksMenuItemsWithNumbers) var marksMenuItemsWithNumbers
        @Shared(.showsToolTipsOnMenuItems) var showsToolTipsOnMenuItems
        @Shared(.showsImagesInMenu) var showsImagesInMenu
        @Shared(.addsNumericKeyEquivalents) var addsNumericKeyEquivalents
        @Shared(.overwritesDuplicateHistory) var overwritesDuplicateHistory
        @Shared(.allowsDuplicateHistory) var allowsDuplicateHistory
        @Shared(.showsColorPreviewInMenu) var showsColorPreviewInMenu
        @Shared(.ignoresConcealedPasteboardTypes) var ignoresConcealedPasteboardTypes
        @Shared(.checksForUpdatesAutomatically) var checksForUpdatesAutomatically
        @Shared(.pastesPlainTextWithModifier) var pastesPlainTextWithModifier
        @Shared(.deletesHistoryWithModifier) var deletesHistoryWithModifier
        @Shared(.pastesAndDeletesHistoryWithModifier) var pastesAndDeletesHistoryWithModifier
        @Shared(.observesScreenshots) var observesScreenshots

        #expect(!isLaunchAtLogin)
        #expect(!suppressesLoginItemAlert)
        #expect(pastesAutomatically)
        #expect(reordersClipsAfterPasting)
        #expect(collectsCrashReports)
        #expect(!startsMenuItemTitlesAtZero)
        #expect(showsClearHistoryAlert)
        #expect(showsClearHistoryMenuItem)
        #expect(showsIconsInMenu)
        #expect(marksMenuItemsWithNumbers)
        #expect(showsToolTipsOnMenuItems)
        #expect(showsImagesInMenu)
        #expect(!addsNumericKeyEquivalents)
        #expect(overwritesDuplicateHistory)
        #expect(allowsDuplicateHistory)
        #expect(showsColorPreviewInMenu)
        #expect(!ignoresConcealedPasteboardTypes)
        #expect(checksForUpdatesAutomatically)
        #expect(pastesPlainTextWithModifier)
        #expect(!deletesHistoryWithModifier)
        #expect(!pastesAndDeletesHistoryWithModifier)
        #expect(!observesScreenshots)
    }

    @Test
    func loadDefaultIntegerSettings() {
        @Shared(.maximumHistoryCount) var maximumHistoryCount
        @Shared(.statusItemDisplayMode) var statusItemDisplayMode
        @Shared(.menuIconSize) var menuIconSize
        @Shared(.maximumMenuItemTitleLength) var maximumMenuItemTitleLength
        @Shared(.inlineMenuItemLimit) var inlineMenuItemLimit
        @Shared(.folderMenuItemLimit) var folderMenuItemLimit
        @Shared(.maximumToolTipLength) var maximumToolTipLength
        @Shared(.thumbnailWidth) var thumbnailWidth
        @Shared(.thumbnailHeight) var thumbnailHeight
        @Shared(.updateCheckInterval) var updateCheckInterval
        @Shared(.plainTextPasteModifier) var plainTextPasteModifier
        @Shared(.historyDeletionModifier) var historyDeletionModifier
        @Shared(.pasteAndDeleteHistoryModifier) var pasteAndDeleteHistoryModifier

        #expect(maximumHistoryCount == 30)
        #expect(statusItemDisplayMode == 1)
        #expect(menuIconSize == 16)
        #expect(maximumMenuItemTitleLength == 20)
        #expect(inlineMenuItemLimit == 0)
        #expect(folderMenuItemLimit == 10)
        #expect(maximumToolTipLength == 200)
        #expect(thumbnailWidth == 100)
        #expect(thumbnailHeight == 32)
        #expect(updateCheckInterval == 86_400)
        #expect(plainTextPasteModifier == 0)
        #expect(historyDeletionModifier == 0)
        #expect(pasteAndDeleteHistoryModifier == 0)
    }

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

//
//  UserDefaultsCodableMigrationTests.swift
//
//  ClipyTests
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/08/01.
//
//  Copyright © 2015-2026 Clipy Project.
//

import Dependencies
import DependenciesTestSupport
import Foundation
import Magnet
import Sharing
import Testing
@testable import Clipy

@Suite(.dependencies)
final class UserDefaultsCodableMigrationTests {
    @Dependency(\.defaultAppStorage)
    private var appStorage

    @Test
    func migratesLegacyValuesToCodableStorage() throws {
        let mainKeyCombo = try #require(KeyCombo(QWERTYKeyCode: 0, carbonModifiers: 4352))
        let historyKeyCombo = try #require(KeyCombo(doubledCocoaModifiers: .command))
        let snippetKeyCombo = try #require(KeyCombo(QWERTYKeyCode: 6, carbonModifiers: 256))
        let clearHistoryKeyCombo = try #require(KeyCombo(QWERTYKeyCode: 10, carbonModifiers: 256))
        let folderKeyCombos = [UUID().uuidString: snippetKeyCombo]
        let excludedApplications = [
            try applicationInformation(identifier: "com.example.first", name: "First"),
            try applicationInformation(identifier: "com.example.second", name: "Second")
        ]
        let legacyEnabledPasteboardTypes = [
            PasteboardAvailableType.string.rawValue: NSNumber(value: true),
            PasteboardAvailableType.pdf.rawValue: NSNumber(value: false)
        ]
        let pasteboardTypeSettings = PasteboardTypeSettings(
            values: [
                PasteboardAvailableType.string.rawValue: true,
                PasteboardAvailableType.pdf.rawValue: false
            ]
        )

        appStorage.set(
            archive(mainKeyCombo),
            forKey: UserDefaultsCodableMigration.LegacyAppStorageKey.mainKeyCombo.rawValue
        )
        appStorage.set(
            archive(historyKeyCombo),
            forKey: UserDefaultsCodableMigration.LegacyAppStorageKey.historyKeyCombo.rawValue
        )
        appStorage.set(
            archive(snippetKeyCombo),
            forKey: UserDefaultsCodableMigration.LegacyAppStorageKey.snippetKeyCombo.rawValue
        )
        appStorage.set(
            archive(clearHistoryKeyCombo),
            forKey: UserDefaultsCodableMigration.LegacyAppStorageKey.clearHistoryKeyCombo.rawValue
        )
        appStorage.set(
            archive(folderKeyCombos),
            forKey: UserDefaultsCodableMigration.LegacyAppStorageKey.folderKeyCombos.rawValue
        )
        NSKeyedArchiver.setClassName("Clipy.CPYAppInfo", for: ApplicationInformation.self)
        appStorage.set(
            archive(excludedApplications),
            forKey: UserDefaultsCodableMigration.LegacyAppStorageKey.excludedApplications.rawValue
        )
        appStorage.set(
            legacyEnabledPasteboardTypes,
            forKey: UserDefaultsCodableMigration.LegacyAppStorageKey.enabledPasteboardTypes.rawValue
        )

        @Shared(.mainKeyCombo) var migratedMainKeyCombo
        @Shared(.historyKeyCombo) var migratedHistoryKeyCombo
        @Shared(.snippetKeyCombo) var migratedSnippetKeyCombo
        @Shared(.clearHistoryKeyCombo) var migratedClearHistoryKeyCombo
        @Shared(.folderKeyCombos) var migratedFolderKeyCombos
        @Shared(.excludedApplications) var migratedExcludedApplications
        @Shared(.pasteboardTypeSettings) var migratedPasteboardTypeSettings

        #expect(mainKeyCombo != migratedMainKeyCombo)
        #expect(historyKeyCombo != migratedHistoryKeyCombo)
        #expect(snippetKeyCombo != migratedSnippetKeyCombo)
        #expect(clearHistoryKeyCombo != migratedClearHistoryKeyCombo)
        #expect(folderKeyCombos != migratedFolderKeyCombos)
        #expect(excludedApplications != migratedExcludedApplications)
        #expect(pasteboardTypeSettings != migratedPasteboardTypeSettings)

        try UserDefaultsCodableMigration().run()

        #expect(migratedMainKeyCombo == mainKeyCombo)
        #expect(migratedHistoryKeyCombo == historyKeyCombo)
        #expect(migratedSnippetKeyCombo == snippetKeyCombo)
        #expect(migratedClearHistoryKeyCombo == clearHistoryKeyCombo)
        #expect(migratedFolderKeyCombos == folderKeyCombos)
        #expect(migratedExcludedApplications == excludedApplications)
        #expect(migratedPasteboardTypeSettings == pasteboardTypeSettings)
    }

    private func applicationInformation(identifier: String, name: String) throws -> ApplicationInformation {
        try #require(
            ApplicationInformation(
                info: [
                    kCFBundleIdentifierKey as String: identifier as NSString,
                    kCFBundleNameKey as String: name as NSString
                ]
            )
        )
    }

    private func archive<Value>(_ value: Value) -> Data {
        NSKeyedArchiver.archivedData(withRootObject: value)
    }
}

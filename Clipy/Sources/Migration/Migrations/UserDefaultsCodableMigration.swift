//
//  UserDefaultsCodableMigration.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/08/01.
//
//  Copyright © 2015-2026 Clipy Project.
//

import Dependencies
import Foundation
import Magnet
import Sharing

final class UserDefaultsCodableMigration: AppMigration {
    enum LegacyAppStorageKey: String {
        case enabledPasteboardTypes = "kCPYPrefStoreTypesKey"
        case excludedApplications = "kCPYExcludeApplications"
        case mainKeyCombo = "kCPYHotKeyMainKeyCombo"
        case historyKeyCombo = "kCPYHotKeyHistoryKeyCombo"
        case snippetKeyCombo = "kCPYHotKeySnippetKeyCombo"
        case clearHistoryKeyCombo = "kCPYClearHistoryKeyCombo"
        case folderKeyCombos = "kCPYFolderKeyCombos"
        case didMigrateKeyCombosToMagnet = "kCPYMigrateNewKeyCombo"
    }

    // MARK: - Properties
    let id: AppMigrationID = .userDefaultsCodable

    @Dependency(\.defaultAppStorage)
    private var appStorage

    // MARK: - Migration
    func run() throws {
        @Shared(.mainKeyCombo) var mainKeyCombo
        @Shared(.historyKeyCombo) var historyKeyCombo
        @Shared(.snippetKeyCombo) var snippetKeyCombo
        @Shared(.clearHistoryKeyCombo) var clearHistoryKeyCombo
        @Shared(.folderKeyCombos) var folderKeyCombos
        @Shared(.excludedApplications) var excludedApplications
        @Shared(.pasteboardTypeSettings) var pasteboardTypeSettings

        let didMigrateKeyCombosToMagnet = appStorage.bool(forKey: LegacyAppStorageKey.didMigrateKeyCombosToMagnet.rawValue)
        migrateKeyCombo(
            from: .mainKeyCombo,
            to: $mainKeyCombo,
            isMissingValueDisabled: didMigrateKeyCombosToMagnet
        )
        migrateKeyCombo(
            from: .historyKeyCombo,
            to: $historyKeyCombo,
            isMissingValueDisabled: didMigrateKeyCombosToMagnet
        )
        migrateKeyCombo(
            from: .snippetKeyCombo,
            to: $snippetKeyCombo,
            isMissingValueDisabled: didMigrateKeyCombosToMagnet
        )
        migrateKeyCombo(
            from: .clearHistoryKeyCombo,
            to: $clearHistoryKeyCombo,
            isMissingValueDisabled: false
        )
        migrateValue(
            from: .folderKeyCombos,
            to: $folderKeyCombos
        )
        NSKeyedUnarchiver.setClass(ApplicationInformation.self, forClassName: "Clipy.CPYAppInfo")
        migrateValue(
            from: .excludedApplications,
            to: $excludedApplications
        )
        migratePasteboardTypeSettings(
            from: .enabledPasteboardTypes,
            to: $pasteboardTypeSettings
        )
    }
}

private extension UserDefaultsCodableMigration {
    func migrateKeyCombo(
        from legacyKey: LegacyAppStorageKey,
        to destination: Shared<KeyCombo?>,
        isMissingValueDisabled: Bool
    ) {
        guard let data = appStorage.object(forKey: legacyKey.rawValue) as? Data else {
            guard isMissingValueDisabled else { return }
            destination.withLock { $0 = nil }
            return
        }
        guard let keyCombo = NSKeyedUnarchiver.unarchiveObject(with: data) as? KeyCombo else { return }
        destination.withLock { $0 = keyCombo }
    }

    func migrateValue<Value>(
        from legacyKey: LegacyAppStorageKey,
        to destination: Shared<Value>
    ) {
        guard let data = appStorage.object(forKey: legacyKey.rawValue) as? Data else { return }
        guard let value = NSKeyedUnarchiver.unarchiveObject(with: data) as? Value else { return }
        destination.withLock { $0 = value }
    }

    func migratePasteboardTypeSettings(
        from legacyKey: LegacyAppStorageKey,
        to destination: Shared<PasteboardTypeSettings>
    ) {
        guard let values = appStorage.object(forKey: legacyKey.rawValue) as? [String: NSNumber] else { return }
        let settings = PasteboardTypeSettings(values: values.mapValues(\.boolValue))
        destination.withLock { $0 = settings }
    }
}

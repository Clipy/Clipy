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

final class UserDefaultsCodableMigration: AppMigration {
    enum LegacyAppStorageKey: String {
        case enabledPasteboardTypes = "kCPYPrefStoreTypesKey"
        case excludedApplications = "kCPYExcludeApplications"
        case mainKeyCombo = "kCPYHotKeyMainKeyCombo"
        case historyKeyCombo = "kCPYHotKeyHistoryKeyCombo"
        case snippetKeyCombo = "kCPYHotKeySnippetKeyCombo"
        case clearHistoryKeyCombo = "kCPYClearHistoryKeyCombo"
        case folderKeyCombos = "kCPYFolderKeyCombos"
    }

    // MARK: - Properties
    let id: AppMigrationID = .userDefaultsCodable

    @Dependency(\.defaultAppStorage)
    private var appStorage

    // MARK: - Migration
    func run() throws {
        migrateValue(KeyCombo.self, from: .mainKeyCombo, to: .mainKeyCombo)
        migrateValue(KeyCombo.self, from: .historyKeyCombo, to: .historyKeyCombo)
        migrateValue(KeyCombo.self, from: .snippetKeyCombo, to: .snippetKeyCombo)
        migrateValue(KeyCombo.self, from: .clearHistoryKeyCombo, to: .clearHistoryKeyCombo)
        migrateValue([String: KeyCombo].self, from: .folderKeyCombos, to: .folderKeyCombos)
        NSKeyedUnarchiver.setClass(ApplicationInformation.self, forClassName: "Clipy.CPYAppInfo")
        migrateValue([ApplicationInformation].self, from: .excludedApplications, to: .excludedApplications)
        migratePasteboardTypeSettings(from: .enabledPasteboardTypes, to: .pasteboardTypeSettings)
    }
}

private extension UserDefaultsCodableMigration {
    func migrateValue<Value: Encodable>(
        _ type: Value.Type,
        from legacyKey: LegacyAppStorageKey,
        to key: AppStorageKeys
    ) {
        guard let data = appStorage.object(forKey: legacyKey.rawValue) as? Data else { return }
        guard let value = NSKeyedUnarchiver.unarchiveObject(with: data) as? Value else { return }
        guard let encoded = try? JSONEncoder().encode(value) else { return }

        appStorage.set(encoded, forKey: key.rawValue)
    }

    func migratePasteboardTypeSettings(
        from legacyKey: LegacyAppStorageKey,
        to key: AppStorageKeys
    ) {
        guard let values = appStorage.object(forKey: legacyKey.rawValue) as? [String: NSNumber] else { return }
        let settings = PasteboardTypeSettings(values: values.mapValues(\.boolValue))
        guard let encoded = try? JSONEncoder().encode(settings) else { return }

        appStorage.set(encoded, forKey: key.rawValue)
    }
}

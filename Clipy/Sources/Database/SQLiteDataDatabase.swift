//
//  SQLiteDataDatabase.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/05/22.
//
//  Copyright © 2015-2026 Clipy Project.
//

import Dependencies
import Foundation
import SQLiteData
import SwiftData

enum SQLiteDataDatabase {
    static func databaseURL() throws -> URL {
        var applicationSupportDirectory = try FileManager.default.url(
            for: .applicationSupportDirectory,
            in: .userDomainMask,
            appropriateFor: nil,
            create: true
        )
        if let bundleIdentifier = Bundle.main.bundleIdentifier {
            applicationSupportDirectory.append(path: bundleIdentifier)
            try? FileManager.default.createDirectory(
                at: applicationSupportDirectory,
                withIntermediateDirectories: true,
                attributes: nil
            )
        }
        return applicationSupportDirectory.appendingPathComponent("sqlite.db")
    }

    @available(macOS 14, *)
    static var isCloudKitEnabled: Bool {
        ModelConfiguration(groupContainer: .automatic).cloudKitContainerIdentifier != nil
    }
}

extension DependencyValues {
    mutating func bootstrapDatabase() throws {
        @Dependency(\.context) var context

        var configuration = Configuration()
        #if DEBUG
        configuration.prepareDatabase {
            switch context {
            case .live, .preview:
                $0.trace { print($0.expandedDescription) }
            case .test:
                break
            }
        }
        #endif
        let database = try SQLiteData.defaultDatabase(
            path: SQLiteDataDatabase.databaseURL().absoluteString,
            configuration: configuration
        )

        var migrator = DatabaseMigrator()
        migrator.registerMigration()
        try migrator.migrate(database)

        defaultDatabase = database
        if #available(macOS 14, *), SQLiteDataDatabase.isCloudKitEnabled {
            defaultSyncEngine = try SyncEngine(
                for: database,
                tables: PasteboardHistory.self, PasteboardHistoryAsset.self, PasteboardHistoryThumbnailAsset.self, SnippetFolder.self, Snippet.self,
                // Keep iCloud synchronization disabled for now. Setting this to true starts
                // synchronization, and a future release will make this user-configurable.
                startImmediately: false
            )
        }
    }
}

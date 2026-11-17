//
//  SQLiteDataMigratorTests+V5.swift
//  ClipyTests
//
//  Copyright © 2015-2026 Clipy Project.
//

import SQLiteData
import Testing
@testable import Clipy

extension SQLiteDataMigratorTests {
    @Test
    func migrationV5() throws {
        let database = try DatabaseQueue(configuration: SQLiteDataDatabase.configuration)
        var migrator = DatabaseMigrator()
        migrator.registerMigrationV1()
        migrator.registerMigrationV2()
        migrator.registerMigrationV3()
        migrator.registerMigrationV4()
        migrator.registerMigrationV5()
        try migrator.migrate(database)

        try expectV5TableNames(database)
        try expectV5Triggers(database)
        try expectV3Indexes(database)
        try expectV4Tables(database)
        try expectV5Tables(database)
    }

    func expectV5TableNames(_ database: DatabaseQueue) throws {
        try database.read { database in
            let tables = try tableNames(database)
            #expect(
                tables == [
                    "grdb_migrations",
                    "pasteboardHistories",
                    "pasteboardHistoryAssets",
                    "pasteboardHistorySearches",
                    "pasteboardHistorySearches_config",
                    "pasteboardHistorySearches_content",
                    "pasteboardHistorySearches_data",
                    "pasteboardHistorySearches_docsize",
                    "pasteboardHistorySearches_idx",
                    "pasteboardHistoryThumbnailAssets",
                    "snippetFolderSearches",
                    "snippetFolderSearches_config",
                    "snippetFolderSearches_content",
                    "snippetFolderSearches_data",
                    "snippetFolderSearches_docsize",
                    "snippetFolderSearches_idx",
                    "snippetFolders",
                    "snippetSearches",
                    "snippetSearches_config",
                    "snippetSearches_content",
                    "snippetSearches_data",
                    "snippetSearches_docsize",
                    "snippetSearches_idx",
                    "snippets"
                ]
            )
        }
    }

    func expectV5Triggers(_ database: DatabaseQueue) throws {
        try database.read { database in
            let triggers = try triggers(database)
            #expect(
                triggers == [
                    "delete_pasteboardHistories_from_pasteboardHistorySearches",
                    "delete_snippetFolders_from_snippetFolderSearches",
                    "delete_snippets_from_snippetSearches",
                    "insert_pasteboardHistories_into_pasteboardHistorySearches",
                    "insert_snippetFolders_into_snippetFolderSearches",
                    "insert_snippets_into_snippetSearches",
                    "update_pasteboardHistories_in_pasteboardHistorySearches",
                    "update_snippetFolders_in_snippetFolderSearches",
                    "update_snippets_in_snippetSearches"
                ]
            )
        }
    }

    func expectV5Tables(_ database: DatabaseQueue) throws {
        try database.read { database in
            let columnNames = try columnNames(of: "snippetFolderSearches", database: database)
            #expect(columnNames == ["id", "title"])
        }
    }
}

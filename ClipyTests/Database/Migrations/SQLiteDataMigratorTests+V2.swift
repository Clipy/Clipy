//
//  SQLiteDataMigratorTests+V2.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/07/07.
//
//  Copyright © 2015-2026 Clipy Project.
//

import SQLiteData
import Testing
@testable import Clipy

extension SQLiteDataMigratorTests {
    @Test
    func migrationV2() throws {
        let database = try DatabaseQueue()
        var migrator = DatabaseMigrator()
        migrator.registerMigrationV1()
        migrator.registerMigrationV2()
        try migrator.migrate(database)

        try expectV1Indexes(database)
        try expectV1Tables(database)
        try expectV2TableNames(database)
        try expectV2Triggers(database)
        try expectV2Tables(database)
    }

    func expectV2TableNames(_ database: DatabaseQueue) throws {
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

    func expectV2Triggers(_ database: DatabaseQueue) throws {
        try database.read { database in
            let triggers = try triggers(database)
            #expect(
                triggers == [
                    "delete_pasteboardHistories_from_pasteboardHistorySearches",
                    "delete_snippets_from_snippetSearches",
                    "insert_pasteboardHistories_into_pasteboardHistorySearches",
                    "insert_snippets_into_snippetSearches",
                    "update_pasteboardHistories_in_pasteboardHistorySearches",
                    "update_snippets_in_snippetSearches"
                ]
            )
        }
    }

    func expectV2Tables(_ database: DatabaseQueue) throws {
        try database.read { database in
            let columnNames = try columnNames(of: "pasteboardHistorySearches", database: database)
            #expect(
                columnNames == [
                    "id",
                    "title"
                ]
            )
        }
        try database.read { database in
            let columnNames = try columnNames(of: "snippetSearches", database: database)
            #expect(
                columnNames == [
                    "content",
                    "id",
                    "title"
                ]
            )
        }
    }
}

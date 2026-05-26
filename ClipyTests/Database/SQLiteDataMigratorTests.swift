//
//  SQLiteDataMigratorTests.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/05/26.
//
//  Copyright © 2015-2026 Clipy Project.
//

import SQLiteData
import Testing
@testable import Clipy

@MainActor
@Suite
struct SQLiteDataMigratorTests {
    @Test
    func migrationV1() throws {
        let database = try DatabaseQueue()
        var migrator = DatabaseMigrator()
        migrator.registerMigrationV1()
        try migrator.migrate(database)

        try database.read { database in
            let tables = try #sql(
                """
                SELECT "name"
                FROM "sqlite_master"
                WHERE "type" = 'table'
                ORDER BY "name"
                """,
                as: String.self
            )
            .fetchAll(database)
            #expect(
                tables == [
                    "grdb_migrations",
                    "pasteboardHistories",
                    "pasteboardHistoryAssets",
                    "pasteboardHistoryThumbnailAssets",
                    "snippetFolders",
                    "snippets"
                ]
            )
        }

        try database.read { database in
            let indexes = try #sql(
                """
                SELECT "name"
                FROM "sqlite_master"
                WHERE "type" = 'index'
                AND "name" GLOB 'index_*'
                ORDER BY "name"
                """,
                as: String.self
            )
            .fetchAll(database)
            #expect(
                indexes == [
                    "index_pasteboardHistoryAssets_on_pasteboardHistoryID",
                    "index_snippetFolders_on_index",
                    "index_snippets_on_folderID",
                    "index_snippets_on_folderID_index",
                    "index_snippets_on_index"
                ]
            )
        }
    }
}

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
            let tables = try tableNames(database)
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

        try expectV1Indexes(database)
        try expectV1Tables(database)
    }

    @Test
    func migrationV2() throws {
        let database = try DatabaseQueue()
        var migrator = DatabaseMigrator()
        migrator.registerMigrationV1()
        migrator.registerMigrationV2()
        try migrator.migrate(database)

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

        try expectV1Indexes(database)
        try expectV1Tables(database)
        try expectV2Triggers(database)
        try expectV2Tables(database)
    }
}

private extension SQLiteDataMigratorTests {
    func expectV1Indexes(_ database: DatabaseQueue) throws {
        try database.read { database in
            let indexes = try indexes(database)
            #expect(
                indexes == [
                    "index_pasteboardHistories_on_updateAt",
                    "index_pasteboardHistoryAssets_on_pasteboardHistoryID_index",
                    "index_snippetFolders_on_index",
                    "index_snippets_on_folderID",
                    "index_snippets_on_folderID_index",
                    "index_snippets_on_index"
                ]
            )
        }
    }

    func expectV1Tables(_ database: DatabaseQueue) throws {
        try database.read { database in
            let columnNames = try columnNames(of: "pasteboardHistories", database: database)
            #expect(
                columnNames == [
                    "deviceID",
                    "id",
                    "pasteboardTypes",
                    "title",
                    "updateAt"
                ]
            )
        }
        try database.read { database in
            let columnNames = try columnNames(of: "pasteboardHistoryAssets", database: database)
            #expect(
                columnNames == [
                    "data",
                    "id",
                    "index",
                    "pasteboardHistoryID",
                    "pasteboardType"
                ]
            )
        }
        try database.read { database in
            let columnNames = try columnNames(of: "pasteboardHistoryThumbnailAssets", database: database)
            #expect(
                columnNames == [
                    "data",
                    "kind",
                    "pasteboardHistoryID"
                ]
            )
        }
        try database.read { database in
            let columnNames = try columnNames(of: "snippetFolders", database: database)
            #expect(
                columnNames == [
                    "id",
                    "index",
                    "isEnabled",
                    "title"
                ]
            )
        }
        try database.read { database in
            let columnNames = try columnNames(of: "snippets", database: database)
            #expect(
                columnNames == [
                    "content",
                    "folderID",
                    "id",
                    "index",
                    "isEnabled",
                    "title"
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

private extension SQLiteDataMigratorTests {
    func tableNames(_ database: Database) throws -> [String] {
        try #sql(
            """
            SELECT "name"
            FROM "sqlite_master"
            WHERE "type" = 'table'
            ORDER BY "name"
            """,
            as: String.self
        )
        .fetchAll(database)
    }

    func indexes(_ database: Database) throws -> [String] {
        try #sql(
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
    }

    func triggers(_ database: Database) throws -> [String] {
        try #sql(
            """
            SELECT "name"
            FROM "sqlite_master"
            WHERE "type" = 'trigger'
            ORDER BY "name"
            """,
            as: String.self
        )
        .fetchAll(database)
    }

    func columnNames(of tableName: String, database: Database) throws -> [String] {
        try #sql(
            """
            SELECT "name"
            FROM pragma_table_info('\(raw: tableName)')
            ORDER BY "name"
            """,
            as: String.self
        )
        .fetchAll(database)
    }
}

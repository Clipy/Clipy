//
//  SQLiteDataMigratorTests+V3.swift
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
    func migrationV3() throws {
        let database = try DatabaseQueue()
        var migrator = DatabaseMigrator()
        migrator.registerMigrationV1()
        migrator.registerMigrationV2()
        migrator.registerMigrationV3()
        try migrator.migrate(database)

        try expectV2TableNames(database)
        try expectV2Triggers(database)
        try expectV2Tables(database)
        try expectV3Indexes(database)
        try expectV3Tables(database)
    }

    func expectV3Indexes(_ database: DatabaseQueue) throws {
        try database.read { database in
            let indexes = try indexes(database)
            #expect(
                indexes == [
                    "index_pasteboardHistories_on_createdAt",
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

    func expectV3Tables(_ database: DatabaseQueue) throws {
        try database.read { database in
            let columnNames = try columnNames(of: "pasteboardHistories", database: database)
            #expect(
                columnNames == [
                    "createdAt",
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
}

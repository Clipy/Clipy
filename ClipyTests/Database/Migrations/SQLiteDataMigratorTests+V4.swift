//
//  SQLiteDataMigratorTests+V4.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/07/09.
//
//  Copyright © 2015-2026 Clipy Project.
//

import SQLiteData
import Testing
@testable import Clipy

extension SQLiteDataMigratorTests {
    @Test
    func migrationV4() throws {
        let database = try DatabaseQueue()
        var migrator = DatabaseMigrator()
        migrator.registerMigrationV1()
        migrator.registerMigrationV2()
        migrator.registerMigrationV3()
        migrator.registerMigrationV4()
        try migrator.migrate(database)

        try expectV2TableNames(database)
        try expectV2Triggers(database)
        try expectV3Indexes(database)
        try expectV4Tables(database)
    }

    func expectV4Tables(_ database: DatabaseQueue) throws {
        try database.read { database in
            let columnNames = try columnNames(of: "pasteboardHistories", database: database)
            #expect(
                columnNames == [
                    "createdAt",
                    "deviceID",
                    "id",
                    "ocrText",
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
        try database.read { database in
            let columnNames = try columnNames(of: "pasteboardHistorySearches", database: database)
            #expect(
                columnNames == [
                    "id",
                    "ocrText",
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

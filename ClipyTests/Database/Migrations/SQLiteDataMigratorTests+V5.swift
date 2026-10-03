//
//  SQLiteDataMigratorTests+V5.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/09/28.
//
//  Copyright © 2015-2026 Clipy Project.
//

import SQLiteData
import Testing
@testable import Clipy

extension SQLiteDataMigratorTests {
    @Test
    func migrationV5() throws {
        let database = try DatabaseQueue()
        var migrator = DatabaseMigrator()
        migrator.registerMigrationV1()
        migrator.registerMigrationV2()
        migrator.registerMigrationV3()
        migrator.registerMigrationV4()
        migrator.registerMigrationV5()
        try migrator.migrate(database)

        try expectV2TableNames(database)
        try expectV2Triggers(database)
        try expectV5Tables(database)
    }

    @Test
    func migrationV5BackfillsExistingRows() throws {
        let database = try DatabaseQueue()
        var migrator = DatabaseMigrator()
        migrator.registerMigrationV1()
        migrator.registerMigrationV2()
        migrator.registerMigrationV3()
        migrator.registerMigrationV4()
        try migrator.migrate(database)

        try database.write { database in
            try #sql(
                """
                INSERT INTO "snippetFolders" ("id", "title", "index", "isEnabled")
                VALUES ('folder-1', 'Folder', 0, 1)
                """
            )
            .execute(database)
            try #sql(
                """
                INSERT INTO "snippets" ("id", "folderID", "title", "content", "index", "isEnabled")
                VALUES ('snippet-1', 'folder-1', 'Snippet', 'Content', 0, 1)
                """
            )
            .execute(database)
        }

        migrator.registerMigrationV5()
        try migrator.migrate(database)

        try database.read { database in
            let folderUpdatedAt = try #sql(
                """
                SELECT "updatedAt" FROM "snippetFolders" WHERE "id" = 'folder-1'
                """,
                as: Int.self
            )
            .fetchOne(database)
            let snippetUpdatedAt = try #sql(
                """
                SELECT "updatedAt" FROM "snippets" WHERE "id" = 'snippet-1'
                """,
                as: Int.self
            )
            .fetchOne(database)
            #expect((folderUpdatedAt ?? 0) > 1_000_000_000_000)
            #expect((snippetUpdatedAt ?? 0) > 1_000_000_000_000)
        }
    }

    func expectV5Tables(_ database: DatabaseQueue) throws {
        try database.read { database in
            let columnNames = try columnNames(of: "snippetFolders", database: database)
            #expect(
                columnNames == [
                    "id",
                    "index",
                    "isEnabled",
                    "title",
                    "updatedAt"
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
                    "title",
                    "updatedAt"
                ]
            )
        }
    }

}

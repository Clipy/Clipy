//
//  SQLiteDataMigratorTests+V5.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//
//  Copyright © 2015-2026 Clipy Project.
//

import SQLiteData
import Testing
@testable import Clipy

extension SQLiteDataMigratorTests {
    @Test
    func migrationV5AddsDeviceIDToExistingDatabase() throws {
        let database = try DatabaseQueue()
        try database.write { database in
            try #sql(
                """
                CREATE TABLE "pasteboardHistories" (
                  "id" TEXT PRIMARY KEY NOT NULL,
                  "title" TEXT NOT NULL,
                  "pasteboardTypes" TEXT NOT NULL,
                  "updateAt" INTEGER NOT NULL,
                  "createdAt" INTEGER NOT NULL,
                  "ocrText" TEXT
                ) STRICT
                """
            )
            .execute(database)
            try #sql(
                """
                INSERT INTO "pasteboardHistories"
                  ("id", "title", "pasteboardTypes", "updateAt", "createdAt", "ocrText")
                VALUES ('existing-history', 'Existing history', '[]', 1, 1, NULL)
                """
            )
            .execute(database)
        }

        var migrator = DatabaseMigrator()
        migrator.registerMigrationV5()
        try migrator.migrate(database)

        try database.read { database in
            let columns = try columnNames(of: "pasteboardHistories", database: database)
            #expect(columns == [
                "createdAt",
                "deviceID",
                "id",
                "ocrText",
                "pasteboardTypes",
                "title",
                "updateAt"
            ])
            let title = try #sql(
                """
                SELECT "title"
                FROM "pasteboardHistories"
                WHERE "id" = 'existing-history'
                """,
                as: String.self
            )
            .fetchOne(database)
            #expect(title == "Existing history")
        }
    }
}

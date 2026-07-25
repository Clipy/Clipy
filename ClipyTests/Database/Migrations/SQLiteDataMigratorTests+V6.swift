//
//  SQLiteDataMigratorTests+V6.swift
//
//  Clipy
//  GitHub: https://github.com/Clipy/Clipy
//
//  Copyright © 2015-2026 Clipy Project.
//

import SQLiteData
import Testing
@testable import Clipy

extension SQLiteDataMigratorTests {
    @Test
    func migrationV6RepairsLegacyAssetColumns() throws {
        let database = try DatabaseQueue()
        try database.write { database in
            try #sql(
                """
                CREATE TABLE "pasteboardHistoryAssets" (
                  "id" TEXT PRIMARY KEY NOT NULL,
                  "pasteboardHistoryID" TEXT NOT NULL,
                  "pasteboardType" TEXT NOT NULL,
                  "data" BLOB NOT NULL
                ) STRICT
                """
            )
            .execute(database)
            try #sql(
                """
                INSERT INTO "pasteboardHistoryAssets"
                  ("id", "pasteboardHistoryID", "pasteboardType", "data")
                VALUES
                  ('asset-1', 'history-1', 'public.utf8-plain-text', X'01'),
                  ('asset-2', 'history-1', 'public.utf8-plain-text', X'02')
                """
            )
            .execute(database)
            try #sql(
                """
                CREATE TABLE "pasteboardHistoryThumbnailAssets" (
                  "pasteboardHistoryID" TEXT PRIMARY KEY NOT NULL,
                  "data" BLOB NOT NULL
                ) STRICT
                """
            )
            .execute(database)
            try #sql(
                """
                INSERT INTO "pasteboardHistoryThumbnailAssets"
                  ("pasteboardHistoryID", "data")
                VALUES ('history-1', X'01')
                """
            )
            .execute(database)
        }

        var migrator = DatabaseMigrator()
        migrator.registerMigrationV6()
        try migrator.migrate(database)

        try database.read { database in
            let assetColumnNames = try columnNames(of: "pasteboardHistoryAssets", database: database)
            #expect(assetColumnNames == [
                "data", "id", "index", "pasteboardHistoryID", "pasteboardType"
            ])
            let thumbnailColumnNames = try columnNames(of: "pasteboardHistoryThumbnailAssets", database: database)
            #expect(thumbnailColumnNames == [
                "data", "kind", "pasteboardHistoryID"
            ])
            let indexes = try #sql(
                """
                SELECT "index"
                FROM "pasteboardHistoryAssets"
                ORDER BY rowid
                """,
                as: Int.self
            )
            .fetchAll(database)
            #expect(indexes == [0, 1])
            let kind = try #sql(
                "SELECT \"kind\" FROM \"pasteboardHistoryThumbnailAssets\"",
                as: String.self
            )
            .fetchOne(database)
            #expect(kind == "image")
        }
    }
}

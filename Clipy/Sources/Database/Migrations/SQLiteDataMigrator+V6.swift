//
//  SQLiteDataMigrator+V6.swift
//
//  Clipy
//  GitHub: https://github.com/Clipy/Clipy
//
//  Copyright © 2015-2026 Clipy Project.
//

import SQLiteData

extension DatabaseMigrator {
    mutating func registerMigrationV6() {
        registerMigration("Repair legacy pasteboard asset columns") { database in
            let assetColumns = try #sql(
                "SELECT \"name\" FROM pragma_table_info('pasteboardHistoryAssets')",
                as: String.self
            )
            .fetchAll(database)
            if !assetColumns.contains("index") {
                try #sql(
                    """
                    ALTER TABLE "pasteboardHistoryAssets"
                    ADD COLUMN "index" INTEGER NOT NULL ON CONFLICT REPLACE DEFAULT 0
                    """
                )
                .execute(database)

                // Preserve the original asset order for databases that predate
                // the explicit index column.
                try #sql(
                    """
                    UPDATE "pasteboardHistoryAssets" AS target
                    SET "index" = (
                      SELECT COUNT(*) - 1
                      FROM "pasteboardHistoryAssets" AS previous
                      WHERE previous."pasteboardHistoryID" = target."pasteboardHistoryID"
                        AND previous.rowid <= target.rowid
                    )
                    """
                )
                .execute(database)
            }

            let thumbnailColumns = try #sql(
                "SELECT \"name\" FROM pragma_table_info('pasteboardHistoryThumbnailAssets')",
                as: String.self
            )
            .fetchAll(database)
            if !thumbnailColumns.contains("kind") {
                try #sql(
                    """
                    ALTER TABLE "pasteboardHistoryThumbnailAssets"
                    ADD COLUMN "kind" TEXT NOT NULL ON CONFLICT REPLACE DEFAULT 'image'
                    """
                )
                .execute(database)
            }
        }
    }
}

//
//  SQLiteDataMigrator+V5.swift
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

extension DatabaseMigrator {
    mutating func registerMigrationV5() {
        registerMigration("Add updatedAt to snippetFolders and snippets") { database in
            try #sql(
                """
                ALTER TABLE "snippetFolders"
                ADD COLUMN "updatedAt" INTEGER NOT NULL ON CONFLICT REPLACE DEFAULT 0
                """
            )
            .execute(database)

            try #sql(
                """
                UPDATE "snippetFolders"
                SET "updatedAt" = CAST((julianday('now') - 2440587.5) * 86400000 AS INTEGER)
                """
            )
            .execute(database)

            try #sql(
                """
                ALTER TABLE "snippets"
                ADD COLUMN "updatedAt" INTEGER NOT NULL ON CONFLICT REPLACE DEFAULT 0
                """
            )
            .execute(database)

            try #sql(
                """
                UPDATE "snippets"
                SET "updatedAt" = CAST((julianday('now') - 2440587.5) * 86400000 AS INTEGER)
                """
            )
            .execute(database)

        }
    }
}

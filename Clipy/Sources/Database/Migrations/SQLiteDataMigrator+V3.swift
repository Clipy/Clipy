//
//  SQLiteDataMigrator+V3.swift
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

extension DatabaseMigrator {
    mutating func registerMigrationV3() {
        registerMigration("Add createdAt to pasteboardHistories") { database in
            try #sql(
                """
                ALTER TABLE "pasteboardHistories"
                ADD COLUMN "createdAt" INTEGER NOT NULL ON CONFLICT REPLACE DEFAULT 0
                """
            )
            .execute(database)

            try #sql(
                """
                UPDATE "pasteboardHistories"
                SET "createdAt" = "updateAt"
                """
            )
            .execute(database)

            try #sql(
                """
                CREATE INDEX "index_pasteboardHistories_on_createdAt"
                ON "pasteboardHistories" ("createdAt")
                """
            )
            .execute(database)
        }
    }
}

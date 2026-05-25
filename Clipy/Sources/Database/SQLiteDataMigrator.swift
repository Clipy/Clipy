//
//  SQLiteDataMigrator.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/05/22.
//
//  Copyright © 2015-2026 Clipy Project.
//

import SQLiteData

extension DatabaseMigrator {
    mutating func registerMigration() {
        registerMigration("Create initial tables") { database in
            try #sql(
                """
                CREATE TABLE "pasteboardHistories" (
                  "id" TEXT PRIMARY KEY NOT NULL ON CONFLICT REPLACE DEFAULT (uuid()),
                  "title" TEXT NOT NULL ON CONFLICT REPLACE DEFAULT '',
                  "pasteboardTypes" TEXT NOT NULL ON CONFLICT REPLACE DEFAULT '[]',
                  "updateAt" INTEGER NOT NULL ON CONFLICT REPLACE DEFAULT (unixepoch())
                ) STRICT
                """
            )
            .execute(database)

            try #sql(
                """
                CREATE TABLE "pasteboardHistoryAssets" (
                  "id" TEXT PRIMARY KEY NOT NULL ON CONFLICT REPLACE DEFAULT (uuid()),
                  "pasteboardHistoryID" TEXT NOT NULL,
                  "pasteboardType" TEXT NOT NULL ON CONFLICT REPLACE DEFAULT '',
                  "data" BLOB NOT NULL,
                  FOREIGN KEY ("pasteboardHistoryID")
                    REFERENCES "pasteboardHistories" ("id")
                    ON DELETE CASCADE
                ) STRICT
                """
            )
            .execute(database)

            try #sql(
                """
                CREATE INDEX "index_pasteboardHistoryAssets_on_pasteboardHistoryID"
                ON "pasteboardHistoryAssets" ("pasteboardHistoryID")
                """
            )
            .execute(database)

            try #sql(
                """
                CREATE TABLE "pasteboardHistoryThumbnailAssets" (
                  "pasteboardHistoryID" TEXT PRIMARY KEY NOT NULL,
                  "data" BLOB NOT NULL,
                  FOREIGN KEY ("pasteboardHistoryID")
                    REFERENCES "pasteboardHistories" ("id")
                    ON DELETE CASCADE
                ) STRICT
                """
            )
            .execute(database)

            try #sql(
                """
                CREATE TABLE "snippetFolders" (
                  "id" TEXT PRIMARY KEY NOT NULL ON CONFLICT REPLACE DEFAULT (uuid()),
                  "title" TEXT NOT NULL ON CONFLICT REPLACE DEFAULT '',
                  "index" INTEGER NOT NULL ON CONFLICT REPLACE DEFAULT 0,
                  "isEnabled" INTEGER NOT NULL ON CONFLICT REPLACE DEFAULT 1
                ) STRICT
                """
            )
            .execute(database)

            try #sql(
                """
                CREATE TABLE "snippets" (
                  "id" TEXT PRIMARY KEY NOT NULL ON CONFLICT REPLACE DEFAULT (uuid()),
                  "folderID" TEXT NOT NULL,
                  "title" TEXT NOT NULL ON CONFLICT REPLACE DEFAULT '',
                  "content" TEXT NOT NULL ON CONFLICT REPLACE DEFAULT '',
                  "index" INTEGER NOT NULL ON CONFLICT REPLACE DEFAULT 0,
                  "isEnabled" INTEGER NOT NULL ON CONFLICT REPLACE DEFAULT 1,
                  FOREIGN KEY ("folderID")
                    REFERENCES "snippetFolders" ("id")
                    ON DELETE CASCADE
                ) STRICT
                """
            )
            .execute(database)

            try #sql(
                """
                CREATE INDEX "index_snippets_on_folderID"
                ON "snippets" ("folderID")
                """
            )
            .execute(database)
        }
    }
}

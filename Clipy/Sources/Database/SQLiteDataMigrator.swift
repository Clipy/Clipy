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
        registerMigrationV1()
    }

    // swiftlint:disable:next function_body_length
    mutating func registerMigrationV1() {
        registerMigration("Create initial tables") { database in
            try #sql(
                """
                CREATE TABLE "pasteboardHistories" (
                  "id" TEXT PRIMARY KEY NOT NULL ON CONFLICT REPLACE DEFAULT (uuid()),
                  "title" TEXT NOT NULL ON CONFLICT REPLACE DEFAULT '',
                  "pasteboardTypes" TEXT NOT NULL ON CONFLICT REPLACE DEFAULT '[]',
                  "deviceID" TEXT,
                  "updateAt" INTEGER NOT NULL ON CONFLICT REPLACE DEFAULT (unixepoch())
                ) STRICT
                """
            )
            .execute(database)

            try #sql(
                """
                CREATE INDEX "index_pasteboardHistories_on_updateAt"
                ON "pasteboardHistories" ("updateAt")
                """
            )
            .execute(database)

            try #sql(
                """
                CREATE TABLE "pasteboardHistoryAssets" (
                  "id" TEXT PRIMARY KEY NOT NULL ON CONFLICT REPLACE DEFAULT (uuid()),
                  "pasteboardHistoryID" TEXT NOT NULL,
                  "index" INTEGER NOT NULL ON CONFLICT REPLACE DEFAULT 0,
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
                CREATE INDEX "index_pasteboardHistoryAssets_on_pasteboardHistoryID_index"
                ON "pasteboardHistoryAssets" ("pasteboardHistoryID", "index")
                """
            )
            .execute(database)

            try #sql(
                """
                CREATE TABLE "pasteboardHistoryThumbnailAssets" (
                  "pasteboardHistoryID" TEXT PRIMARY KEY NOT NULL,
                  "kind" TEXT NOT NULL ON CONFLICT REPLACE DEFAULT '',
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
                CREATE INDEX "index_snippetFolders_on_index"
                ON "snippetFolders" ("index")
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

            try #sql(
                """
                CREATE INDEX "index_snippets_on_index"
                ON "snippets" ("index")
                """
            )
            .execute(database)

            try #sql(
                """
                CREATE INDEX "index_snippets_on_folderID_index"
                ON "snippets" ("folderID", "index")
                """
            )
            .execute(database)
        }
    }
}

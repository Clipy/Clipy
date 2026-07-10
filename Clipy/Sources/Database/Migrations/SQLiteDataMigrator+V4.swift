//
//  SQLiteDataMigrator+V4.swift
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

extension DatabaseMigrator {
    mutating func registerMigrationV4() {
        registerMigration("Add OCR text to history search") { database in
            try #sql(
                """
                ALTER TABLE "pasteboardHistories"
                ADD COLUMN "ocrText" TEXT
                """
            )
            .execute(database)

            try #sql(
                """
                DROP TRIGGER "insert_pasteboardHistories_into_pasteboardHistorySearches"
                """
            )
            .execute(database)

            try #sql(
                """
                DROP TRIGGER "update_pasteboardHistories_in_pasteboardHistorySearches"
                """
            )
            .execute(database)

            try #sql(
                """
                DROP TRIGGER "delete_pasteboardHistories_from_pasteboardHistorySearches"
                """
            )
            .execute(database)

            try #sql(
                """
                DROP TABLE "pasteboardHistorySearches"
                """
            )
            .execute(database)

            try #sql(
                """
                CREATE VIRTUAL TABLE "pasteboardHistorySearches"
                USING fts5(
                  "id" UNINDEXED,
                  "title",
                  "ocrText",
                  tokenize = 'trigram'
                )
                """
            )
            .execute(database)

            try #sql(
                """
                CREATE TRIGGER "insert_pasteboardHistories_into_pasteboardHistorySearches"
                AFTER INSERT ON "pasteboardHistories"
                BEGIN
                  DELETE FROM "pasteboardHistorySearches"
                  WHERE "id" = new."id";

                  INSERT INTO "pasteboardHistorySearches" ("id", "title", "ocrText")
                  VALUES (new."id", new."title", coalesce(new."ocrText", ''));
                END
                """
            )
            .execute(database)

            try #sql(
                """
                CREATE TRIGGER "update_pasteboardHistories_in_pasteboardHistorySearches"
                AFTER UPDATE OF "id", "title", "ocrText" ON "pasteboardHistories"
                BEGIN
                  DELETE FROM "pasteboardHistorySearches"
                  WHERE "id" = old."id";

                  INSERT INTO "pasteboardHistorySearches" ("id", "title", "ocrText")
                  VALUES (new."id", new."title", coalesce(new."ocrText", ''));
                END
                """
            )
            .execute(database)

            try #sql(
                """
                CREATE TRIGGER "delete_pasteboardHistories_from_pasteboardHistorySearches"
                AFTER DELETE ON "pasteboardHistories"
                BEGIN
                  DELETE FROM "pasteboardHistorySearches"
                  WHERE "id" = old."id";
                END
                """
            )
            .execute(database)

            try #sql(
                """
                INSERT INTO "pasteboardHistorySearches" ("id", "title", "ocrText")
                SELECT "id", "title", coalesce("ocrText", '')
                FROM "pasteboardHistories"
                """
            )
            .execute(database)
        }
    }
}

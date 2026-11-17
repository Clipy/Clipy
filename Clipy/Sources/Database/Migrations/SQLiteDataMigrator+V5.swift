//
//  SQLiteDataMigrator+V5.swift
//  Clipy
//
//  Copyright © 2015-2026 Clipy Project.
//

import SQLiteData

extension DatabaseMigrator {
    // swiftlint:disable:next function_body_length
    mutating func registerMigrationV5() {
        registerMigration("Normalize search text") { database in
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
                CREATE TRIGGER "insert_pasteboardHistories_into_pasteboardHistorySearches"
                AFTER INSERT ON "pasteboardHistories"
                BEGIN
                  DELETE FROM "pasteboardHistorySearches"
                  WHERE "id" = new."id";

                  INSERT INTO "pasteboardHistorySearches" ("id", "title", "ocrText")
                  VALUES (new."id", normalizeSearchText(new."title"), normalizeSearchText(new."ocrText"));
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
                  VALUES (new."id", normalizeSearchText(new."title"), normalizeSearchText(new."ocrText"));
                END
                """
            )
            .execute(database)

            try #sql(
                """
                DELETE FROM "pasteboardHistorySearches"
                """
            )
            .execute(database)

            try #sql(
                """
                INSERT INTO "pasteboardHistorySearches" ("id", "title", "ocrText")
                SELECT "id", normalizeSearchText("title"), normalizeSearchText("ocrText")
                FROM "pasteboardHistories"
                """
            )
            .execute(database)

            try #sql(
                """
                DROP TRIGGER "insert_snippets_into_snippetSearches"
                """
            )
            .execute(database)

            try #sql(
                """
                DROP TRIGGER "update_snippets_in_snippetSearches"
                """
            )
            .execute(database)

            try #sql(
                """
                CREATE TRIGGER "insert_snippets_into_snippetSearches"
                AFTER INSERT ON "snippets"
                BEGIN
                  DELETE FROM "snippetSearches"
                  WHERE "id" = new."id";

                  INSERT INTO "snippetSearches" ("id", "title", "content")
                  VALUES (new."id", normalizeSearchText(new."title"), normalizeSearchText(new."content"));
                END
                """
            )
            .execute(database)

            try #sql(
                """
                CREATE TRIGGER "update_snippets_in_snippetSearches"
                AFTER UPDATE OF "id", "title", "content" ON "snippets"
                BEGIN
                  DELETE FROM "snippetSearches"
                  WHERE "id" = old."id";

                  INSERT INTO "snippetSearches" ("id", "title", "content")
                  VALUES (new."id", normalizeSearchText(new."title"), normalizeSearchText(new."content"));
                END
                """
            )
            .execute(database)

            try #sql(
                """
                DELETE FROM "snippetSearches"
                """
            )
            .execute(database)

            try #sql(
                """
                INSERT INTO "snippetSearches" ("id", "title", "content")
                SELECT "id", normalizeSearchText("title"), normalizeSearchText("content")
                FROM "snippets"
                """
            )
            .execute(database)

            try #sql(
                """
                CREATE VIRTUAL TABLE "snippetFolderSearches"
                USING fts5(
                  "id" UNINDEXED,
                  "title",
                  tokenize = 'trigram'
                )
                """
            )
            .execute(database)

            try #sql(
                """
                CREATE TRIGGER "insert_snippetFolders_into_snippetFolderSearches"
                AFTER INSERT ON "snippetFolders"
                BEGIN
                  DELETE FROM "snippetFolderSearches"
                  WHERE "id" = new."id";

                  INSERT INTO "snippetFolderSearches" ("id", "title")
                  VALUES (new."id", normalizeSearchText(new."title"));
                END
                """
            )
            .execute(database)

            try #sql(
                """
                CREATE TRIGGER "update_snippetFolders_in_snippetFolderSearches"
                AFTER UPDATE OF "id", "title" ON "snippetFolders"
                BEGIN
                  DELETE FROM "snippetFolderSearches"
                  WHERE "id" = old."id";

                  INSERT INTO "snippetFolderSearches" ("id", "title")
                  VALUES (new."id", normalizeSearchText(new."title"));
                END
                """
            )
            .execute(database)

            try #sql(
                """
                CREATE TRIGGER "delete_snippetFolders_from_snippetFolderSearches"
                AFTER DELETE ON "snippetFolders"
                BEGIN
                  DELETE FROM "snippetFolderSearches"
                  WHERE "id" = old."id";
                END
                """
            )
            .execute(database)

            try #sql(
                """
                INSERT INTO "snippetFolderSearches" ("id", "title")
                SELECT "id", normalizeSearchText("title")
                FROM "snippetFolders"
                """
            )
            .execute(database)
        }
    }
}

//
//  SQLiteDataMigrator+V2.swift
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
    // swiftlint:disable:next function_body_length
    mutating func registerMigrationV2() {
        registerMigration("Create search indexes") { database in
            try #sql(
                """
                CREATE VIRTUAL TABLE "pasteboardHistorySearches"
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
                CREATE TRIGGER "insert_pasteboardHistories_into_pasteboardHistorySearches"
                AFTER INSERT ON "pasteboardHistories"
                BEGIN
                  DELETE FROM "pasteboardHistorySearches"
                  WHERE "id" = new."id";

                  INSERT INTO "pasteboardHistorySearches" ("id", "title")
                  VALUES (new."id", new."title");
                END
                """
            )
            .execute(database)

            try #sql(
                """
                CREATE TRIGGER "update_pasteboardHistories_in_pasteboardHistorySearches"
                AFTER UPDATE OF "id", "title" ON "pasteboardHistories"
                BEGIN
                  DELETE FROM "pasteboardHistorySearches"
                  WHERE "id" = old."id";

                  INSERT INTO "pasteboardHistorySearches" ("id", "title")
                  VALUES (new."id", new."title");
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
                CREATE VIRTUAL TABLE "snippetSearches"
                USING fts5(
                  "id" UNINDEXED,
                  "title",
                  "content",
                  tokenize = 'trigram'
                )
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
                  VALUES (new."id", new."title", new."content");
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
                  VALUES (new."id", new."title", new."content");
                END
                """
            )
            .execute(database)

            try #sql(
                """
                CREATE TRIGGER "delete_snippets_from_snippetSearches"
                AFTER DELETE ON "snippets"
                BEGIN
                  DELETE FROM "snippetSearches"
                  WHERE "id" = old."id";
                END
                """
            )
            .execute(database)
        }
    }
}

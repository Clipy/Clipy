//
//  SQLiteData+TriggerTests.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/06/11.
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import DependenciesTestSupport
import SQLiteData
import Testing
@testable import Clipy

@MainActor
@Suite(
    .dependencies {
        try $0.bootstrapDatabase()
    }
)
struct SQLiteDataDatabaseTriggerTests {
    @Dependency(\.defaultDatabase)
    var database

    @Test
    func pasteboardHistorySearchesIndexIsUpdatedByTriggers() throws {
        let historyID = PasteboardHistory.ID(rawValue: "history-1")

        try database.write { database in
            try PasteboardHistory.insert {
                PasteboardHistory(
                    id: historyID,
                    title: "Xqa History Start",
                    pasteboardTypes: [.string],
                    updateAt: 1,
                    deviceID: nil
                )
            }
            .execute(database)
        }

        try database.read { database in
            let historyIDs = try pasteboardHistories(matching: "xqa", database: database)
                .map(\.history?.id)
            #expect(historyIDs == [historyID])
        }

        try database.write { database in
            try PasteboardHistory.where { $0.id.eq(historyID) }
                .update { $0.title = "Gamma Delta" }
                .execute(database)
        }

        try database.read { database in
            let oldHistoryIDs = try pasteboardHistories(matching: "xqa", database: database)
                .map(\.history?.id)
            #expect(oldHistoryIDs == [])

            let updatedHistoryIDs = try pasteboardHistories(matching: "mma", database: database)
                .map(\.history?.id)
            #expect(updatedHistoryIDs == [historyID])
        }

        try database.write { database in
            try PasteboardHistory.upsert {
                PasteboardHistory(
                    id: historyID,
                    title: "Rpb History Finish",
                    pasteboardTypes: [.string],
                    updateAt: 2,
                    deviceID: nil
                )
            }
            .execute(database)
        }

        try database.read { database in
            let oldHistoryIDs = try pasteboardHistories(matching: "mma", database: database)
                .map(\.history?.id)
            #expect(oldHistoryIDs == [])

            let upsertedHistoryIDs = try pasteboardHistories(matching: "rpb", database: database)
                .map(\.history?.id)
            #expect(upsertedHistoryIDs == [historyID])
        }

        try database.write { database in
            try PasteboardHistory.delete()
                .where { $0.id.eq(historyID) }
                .execute(database)
        }

        try database.read { database in
            let historyCount = try #sql(
                """
                SELECT count(*)
                FROM "pasteboardHistorySearches"
                """,
                as: Int.self
            )
            .fetchOne(database)
            #expect(historyCount == 0)
        }
    }

    @Test
    func snippetSearchesIndexIsUpdatedByTriggers() throws {
        let folderID = SnippetFolder.ID(rawValue: UUID())
        let snippetID = Snippet.ID(rawValue: UUID())

        try database.write { database in
            try SnippetFolder.insert {
                SnippetFolder(
                    id: folderID,
                    title: "Folder",
                    index: 0,
                    isEnabled: true
                )
            }
            .execute(database)

            try Snippet.insert {
                Snippet(
                    id: snippetID,
                    folderID: folderID,
                    title: "Xqa Snippet Start",
                    content: "nuv package",
                    index: 0,
                    isEnabled: true
                )
            }
            .execute(database)
        }

        try database.read { database in
            let titleIDs = try snippets(matching: "xqa", database: database)
                .map(\.snippet?.id)
            #expect(titleIDs == [snippetID])

            let contentIDs = try snippets(matching: "nuv", database: database)
                .map(\.snippet?.id)
            #expect(contentIDs == [snippetID])
        }

        try database.write { database in
            try Snippet.where { $0.id.eq(snippetID) }
                .update {
                    $0.title = "Archive Note"
                    $0.content = "rotating token"
                }
                .execute(database)
        }

        try database.read { database in
            let oldIDs = try snippets(matching: "nuv", database: database)
                .map(\.snippet?.id)
            #expect(oldIDs == [])

            let updatedIDs = try snippets(matching: "tat", database: database)
                .map(\.snippet?.id)
            #expect(updatedIDs == [snippetID])
        }

        try database.write { database in
            try Snippet.upsert {
                Snippet(
                    id: snippetID,
                    folderID: folderID,
                    title: "Rpb Snippet Finish",
                    content: "rst token",
                    index: 0,
                    isEnabled: true
                )
            }
            .execute(database)
        }

        try database.read { database in
            let oldIDs = try snippets(matching: "tat", database: database)
                .map(\.snippet?.id)
            #expect(oldIDs == [])

            let upsertedIDs = try snippets(matching: "rpb", database: database)
                .map(\.snippet?.id)
            #expect(upsertedIDs == [snippetID])
        }

        try database.write { database in
            try Snippet.delete()
                .where { $0.id.eq(snippetID) }
                .execute(database)
        }

        try database.read { database in
            let snippetCount = try #sql(
                """
                SELECT count(*)
                FROM "snippetSearches"
                """,
                as: Int.self
            )
                .fetchOne(database)
            #expect(snippetCount == 0)
        }
    }
}

private extension SQLiteDataDatabaseTriggerTests {
    private func pasteboardHistories(
        matching query: String,
        database: Database
    ) throws -> [PasteboardHistorySearchResult] {
        try PasteboardHistorySearch
            .where { $0.match(query) }
            .leftJoin(PasteboardHistory.all) { $0.id.eq($1.id) }
            .leftJoin(PasteboardHistoryThumbnailAsset.all) { $0.id.eq($2.pasteboardHistoryID) }
            .select { PasteboardHistorySearchResult.Columns(history: $1, thumbnailAsset: $2) }
            .fetchAll(database)
    }

    private func snippets(
        matching query: String,
        database: Database
    ) throws -> [SnippetSearchResult] {
        try SnippetSearch
            .where { $0.match(query) }
            .leftJoin(Snippet.all) { $0.id.eq($1.id) }
            .select { SnippetSearchResult.Columns(snippet: $1) }
            .fetchAll(database)
    }
}

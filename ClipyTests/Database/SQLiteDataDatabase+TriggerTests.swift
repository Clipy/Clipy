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
                    ocrText: nil,
                    pasteboardTypes: [.string],
                    createdAt: 1,
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
            try PasteboardHistory.where { $0.id.eq(historyID) }
                .update {
                    $0.title = #bind("Rpb History Finish")
                    $0.updateAt = 2
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
            try PasteboardHistory.where { $0.id.eq(historyID) }
                .update { $0.ocrText = #bind("Zvw Screenshot Text") }
                .execute(database)
        }

        try database.read { database in
            let ocrHistoryIDs = try pasteboardHistories(matching: "zvw", database: database)
                .map(\.history?.id)
            #expect(ocrHistoryIDs == [historyID])

            let titleHistoryIDs = try pasteboardHistories(matching: "rpb", database: database)
                .map(\.history?.id)
            #expect(titleHistoryIDs == [historyID])
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
    func snippetFolderSearchesIndexIsUpdatedByTriggers() throws {
        let folderID = SnippetFolder.ID(rawValue: UUID())

        try database.write { database in
            try SnippetFolder.insert {
                SnippetFolder(
                    id: folderID,
                    title: "Xqa Folder Start",
                    index: 0,
                    isEnabled: true
                )
            }
            .execute(database)
        }

        try database.read { database in
            let folderIDs = try snippetFolders(matching: "xqa", database: database)
                .map { $0?.id }
            #expect(folderIDs == [folderID])
        }

        try database.write { database in
            try SnippetFolder.where { $0.id.eq(folderID) }
                .update { $0.title = "Gamma Delta" }
                .execute(database)
        }

        try database.read { database in
            let oldFolderIDs = try snippetFolders(matching: "xqa", database: database)
                .map { $0?.id }
            #expect(oldFolderIDs == [])

            let updatedFolderIDs = try snippetFolders(matching: "mma", database: database)
                .map { $0?.id }
            #expect(updatedFolderIDs == [folderID])
        }

        let updatedFolderID = SnippetFolder.ID(rawValue: UUID())
        try database.write { database in
            try SnippetFolder.where { $0.id.eq(folderID) }
                .update {
                    $0.id = updatedFolderID
                    $0.title = "Rpb Folder Finish"
                }
                .execute(database)
        }

        try database.read { database in
            let oldFolderIDs = try snippetFolders(matching: "mma", database: database)
                .map { $0?.id }
            #expect(oldFolderIDs == [])

            let updatedFolderIDs = try snippetFolders(matching: "rpb", database: database)
                .map { $0?.id }
            #expect(updatedFolderIDs == [updatedFolderID])
        }

        try database.write { database in
            try SnippetFolder.delete()
                .where { $0.id.eq(updatedFolderID) }
                .execute(database)
        }

        try database.read { database in
            let folderCount = try #sql(
                """
                SELECT count(*)
                FROM "snippetFolderSearches"
                """,
                as: Int.self
            )
            .fetchOne(database)
            #expect(folderCount == 0)
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
            try Snippet.where { $0.id.eq(snippetID) }
                .update {
                    $0.title = "Rpb Snippet Finish"
                    $0.content = "rst token"
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
    ) throws -> [PasteboardHistorySearchTestResult] {
        try PasteboardHistorySearch
            .where { $0.match(query) }
            .leftJoin(PasteboardHistory.all) { $0.id.eq($1.id) }
            .leftJoin(PasteboardHistoryThumbnailAsset.all) { $0.id.eq($2.pasteboardHistoryID) }
            .select { PasteboardHistorySearchTestResult.Columns(history: $1, thumbnailAsset: $2) }
            .fetchAll(database)
    }

    private func snippetFolders(
        matching query: String,
        database: Database
    ) throws -> [SnippetFolder?] {
        try SnippetFolderSearch
            .where { $0.match(query) }
            .leftJoin(SnippetFolder.all) { $0.id.eq($1.id) }
            .select { $1 }
            .fetchAll(database)
    }

    private func snippets(
        matching query: String,
        database: Database
    ) throws -> [SnippetSearchTestResult] {
        try SnippetSearch
            .where { $0.match(query) }
            .leftJoin(Snippet.all) { $0.id.eq($1.id) }
            .select { SnippetSearchTestResult.Columns(snippet: $1) }
            .fetchAll(database)
    }
}

@Selection
private struct PasteboardHistorySearchTestResult: Equatable {
    let history: PasteboardHistory?
    let thumbnailAsset: PasteboardHistoryThumbnailAsset?
}

@Selection
private struct SnippetSearchTestResult: Equatable {
    let snippet: Snippet?
}

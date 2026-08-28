//
//  SnippetRepositoryTests.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/05/26.
//
//  Copyright © 2015-2026 Clipy Project.
//

import Combine
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
struct SnippetRepositoryTests {
    let repository: SnippetRepository

    init() {
        self.repository = SnippetRepository()
    }

    @Test(.timeLimit(.minutes(1)))
    func observeFolderDetails() async throws {
        var folderDetails = [[SnippetFolderDetail]]()
        let cancellable = repository.observeFolderDetails().sink { value in
            folderDetails.append(value)
        }
        defer { _ = cancellable }

        try await waitUntil { folderDetails.count >= 1 }

        let folder = try #require(repository.insertFolder())
        try await waitUntil { folderDetails.count >= 2 }

        let snippet = try #require(repository.insertSnippet(to: folder.id))
        try await waitUntil { folderDetails.count >= 3 }

        let snippet2 = try #require(repository.insertSnippet(to: folder.id))
        try await waitUntil { folderDetails.count >= 4 }

        let folder2 = try #require(repository.insertFolder())
        try await waitUntil { folderDetails.count >= 5 }

        #expect(
            folderDetails == [
                [],
                [SnippetFolderDetail(folder: folder, snippets: [])],
                [SnippetFolderDetail(folder: folder, snippets: [snippet])],
                [SnippetFolderDetail(folder: folder, snippets: [snippet, snippet2])],
                [SnippetFolderDetail(folder: folder, snippets: [snippet, snippet2]), SnippetFolderDetail(folder: folder2, snippets: [])]
            ]
        )
    }

    @Test
    func insertFoldersAndSnippetsMaintainsOrderedFolderDetails() throws {
        #expect(repository.fetchFolderDetails().isEmpty)

        let folder = try #require(repository.insertFolder())
        #expect(folder.title == "untitled folder")
        #expect(folder.index == 0)
        #expect(folder.isEnabled)

        #expect(repository.fetchFolderDetails() == [SnippetFolderDetail(folder: folder, snippets: [])])
        #expect(repository.fetchFolderDetail(id: folder.id) == SnippetFolderDetail(folder: folder, snippets: []))

        let snippet = try #require(repository.insertSnippet(to: folder.id))
        #expect(snippet.folderID == folder.id)
        #expect(snippet.title == "untitled snippet")
        #expect(snippet.content == "")
        #expect(snippet.index == 0)
        #expect(snippet.isEnabled)

        #expect(repository.fetchFolderDetails() == [SnippetFolderDetail(folder: folder, snippets: [snippet])])
        #expect(repository.fetchFolderDetail(id: folder.id) == SnippetFolderDetail(folder: folder, snippets: [snippet]))
        #expect(repository.fetchSnippet(id: snippet.id) == snippet)

        let snippet2 = try #require(repository.insertSnippet(to: folder.id))
        #expect(snippet2.folderID == folder.id)
        #expect(snippet2.title == "untitled snippet")
        #expect(snippet2.content == "")
        #expect(snippet2.index == 1)
        #expect(snippet2.isEnabled)

        #expect(repository.fetchFolderDetails() == [SnippetFolderDetail(folder: folder, snippets: [snippet, snippet2])])
        #expect(repository.fetchFolderDetail(id: folder.id) == SnippetFolderDetail(folder: folder, snippets: [snippet, snippet2]))
        #expect(repository.fetchSnippet(id: snippet2.id) == snippet2)

        let folder2 = try #require(repository.insertFolder())
        #expect(folder2.title == "untitled folder")
        #expect(folder2.index == 1)
        #expect(folder2.isEnabled)

        #expect(
            repository.fetchFolderDetails() == [
                SnippetFolderDetail(folder: folder, snippets: [snippet, snippet2]),
                SnippetFolderDetail(folder: folder2, snippets: [])
            ]
        )
        #expect(repository.fetchFolderDetail(id: folder2.id) == SnippetFolderDetail(folder: folder2, snippets: []))
    }

    @Test
    func insertFolders() throws {
        let inserted = try #require(
            repository.insertFolders([
                (title: "Empty", snippets: []),
                (
                    title: "Filled",
                    snippets: [
                        (title: "First", content: "one"),
                        (title: "Second", content: "two")
                    ]
                )
            ])
        )
        #expect(inserted.count == 2)
        #expect(inserted.map(\.folder.title) == ["Empty", "Filled"])
        #expect(inserted.map(\.folder.index) == [0, 1])
        #expect(inserted[0].snippets.isEmpty)
        #expect(inserted[1].snippets.map(\.title) == ["First", "Second"])
        #expect(inserted[1].snippets.map(\.content) == ["one", "two"])
        #expect(inserted[1].snippets.map(\.index) == [0, 1])
    }

    @Test
    func updateFolder() throws {
        let folder = try #require(repository.insertFolder())

        repository.updateFolderTitle(folder.id, title: "Updated")
        #expect(repository.fetchFolderDetail(id: folder.id)?.folder.title == "Updated")

        repository.updateFolderIsEnabled(folder.id, isEnabled: false)
        #expect(repository.fetchFolderDetail(id: folder.id)?.folder.isEnabled == false)

        let folder2 = try #require(repository.insertFolder())
        repository.updateFolderIndexes([folder2.id, folder.id])
        #expect(repository.fetchFolderDetail(id: folder2.id)?.folder.index == 0)
        #expect(repository.fetchFolderDetail(id: folder.id)?.folder.index == 1)
        #expect(repository.fetchFolderDetails().map(\.folder.id) == [folder2.id, folder.id])
    }

    @Test
    func deleteFolder() throws {
        let folder = try #require(repository.insertFolder())
        let snippet = try #require(repository.insertSnippet(to: folder.id))

        repository.deleteFolder(folder.id)
        #expect(repository.fetchFolderDetails().isEmpty)
        #expect(repository.fetchSnippet(id: snippet.id) == nil)
    }

    @Test
    func updateSnippet() throws {
        let folder = try #require(repository.insertFolder())
        let snippet = try #require(repository.insertSnippet(to: folder.id))

        repository.updateSnippetTitle(snippet.id, title: "Updated")
        #expect(repository.fetchSnippet(id: snippet.id)?.title == "Updated")

        repository.updateSnippetContent(snippet.id, content: "Updated Content")
        #expect(repository.fetchSnippet(id: snippet.id)?.content == "Updated Content")

        repository.updateSnippetIsEnabled(snippet.id, isEnabled: false)
        #expect(repository.fetchSnippet(id: snippet.id)?.isEnabled == false)

        let snippet2 = try #require(repository.insertSnippet(to: folder.id))
        repository.updateSnippetIndexes([snippet2.id, snippet.id])
        #expect(repository.fetchSnippet(id: snippet2.id)?.index == 0)
        #expect(repository.fetchSnippet(id: snippet.id)?.index == 1)
        #expect(repository.fetchFolderDetail(id: folder.id)?.snippets.map(\.id) == [snippet2.id, snippet.id])
    }

    @Test
    func moveSnippet() throws {
        let folder = try #require(repository.insertFolder())
        let snippet = try #require(repository.insertSnippet(to: folder.id))
        let snippet2 = try #require(repository.insertSnippet(to: folder.id))

        let folder2 = try #require(repository.insertFolder())
        let snippet3 = try #require(repository.insertSnippet(to: folder2.id))
        let snippet4 = try #require(repository.insertSnippet(to: folder2.id))
        let snippet5 = try #require(repository.insertSnippet(to: folder2.id))

        repository.moveSnippet(snippet4.id, to: folder.id, snippetIDs: [snippet.id, snippet4.id, snippet2.id])
        #expect(repository.fetchFolderDetail(id: folder.id)?.snippets.map(\.id) == [snippet.id, snippet4.id, snippet2.id])
        #expect(repository.fetchFolderDetail(id: folder.id)?.snippets.map(\.index) == [0, 1, 2])
        #expect(repository.fetchFolderDetail(id: folder2.id)?.snippets.map(\.id) == [snippet3.id, snippet5.id])
        #expect(repository.fetchFolderDetail(id: folder2.id)?.snippets.map(\.index) == [0, 2])
    }

    @Test
    func deleteSnippet() throws {
        let folder = try #require(repository.insertFolder())
        let snippet = try #require(repository.insertSnippet(to: folder.id))

        repository.deleteSnippet(snippet.id)
        #expect(repository.fetchFolderDetail(id: folder.id) == SnippetFolderDetail(folder: folder, snippets: []))
        #expect(repository.fetchSnippet(id: snippet.id) == nil)
    }

    @Test
    func searchSnippetsWithFTS5Query() throws {
        let folder = try #require(repository.insertFolder())
        repository.updateFolderTitle(folder.id, title: "Development")

        let snippet1 = try #require(repository.insertSnippet(to: folder.id))
        repository.updateSnippetTitle(snippet1.id, title: "xqa function helper")
        repository.updateSnippetContent(snippet1.id, content: "let a = 1")

        let snippet2 = try #require(repository.insertSnippet(to: folder.id))
        repository.updateSnippetTitle(snippet2.id, title: "utility script")
        repository.updateSnippetContent(snippet2.id, content: "nuv package manager")

        // Match by title
        let titleMatches = repository.searchSnippets(matching: "xqa function", limit: 10)
        #expect(titleMatches.count == 1)
        #expect(titleMatches.first?.snippet.id == snippet1.id)
        #expect(titleMatches.first?.folder.id == folder.id)

        // Match by content
        let contentMatches = repository.searchSnippets(matching: "nuv package", limit: 10)
        #expect(contentMatches.count == 1)
        #expect(contentMatches.first?.snippet.id == snippet2.id)
        #expect(contentMatches.first?.folder.id == folder.id)

        // Non-matching
        #expect(repository.searchSnippets(matching: "zztop", limit: 10).isEmpty)
    }

    @Test
    func searchSnippetsWithShortQueryFallsBackToLIKE() throws {
        let folder = try #require(repository.insertFolder())
        let snippet1 = try #require(repository.insertSnippet(to: folder.id))
        repository.updateSnippetTitle(snippet1.id, title: "ab title")
        repository.updateSnippetContent(snippet1.id, content: "content")

        let snippet2 = try #require(repository.insertSnippet(to: folder.id))
        repository.updateSnippetTitle(snippet2.id, title: "other title")
        repository.updateSnippetContent(snippet2.id, content: "ac body")

        // 2 characters (below trigram length 3)
        let matches2Chars = repository.searchSnippets(matching: "ab", limit: 10)
        #expect(matches2Chars.map(\.snippet.id) == [snippet1.id])

        let matchesContent2Chars = repository.searchSnippets(matching: "ac", limit: 10)
        #expect(matchesContent2Chars.map(\.snippet.id) == [snippet2.id])

        // 1 character
        let matches1Char = repository.searchSnippets(matching: "a", limit: 10)
        #expect(matches1Char.count == 2)
    }

    @Test
    func searchSnippetsWithSpecialCharacters() throws {
        let folder = try #require(repository.insertFolder())
        let snippet = try #require(repository.insertSnippet(to: folder.id))
        repository.updateSnippetTitle(snippet.id, title: "test \"quoted\" (special) -hyphen *star:colon")

        #expect(!repository.searchSnippets(matching: "\"quoted\"", limit: 10).isEmpty)
        #expect(!repository.searchSnippets(matching: "(special)", limit: 10).isEmpty)
        #expect(!repository.searchSnippets(matching: "-hyphen", limit: 10).isEmpty)
        #expect(!repository.searchSnippets(matching: "*star", limit: 10).isEmpty)
        #expect(repository.searchSnippets(matching: "\"\"\"\"\"", limit: 10).isEmpty)
        #expect(repository.searchSnippets(matching: "AND OR NOT", limit: 10).isEmpty)
    }

    @Test
    func searchSnippetsFiltersDisabledSnippetsAndFolders() throws {
        let enabledFolder = try #require(repository.insertFolder())
        repository.updateFolderTitle(enabledFolder.id, title: "Enabled Folder")

        let disabledFolder = try #require(repository.insertFolder())
        repository.updateFolderTitle(disabledFolder.id, title: "Disabled Folder")
        repository.updateFolderIsEnabled(disabledFolder.id, isEnabled: false)

        // Enabled snippet in enabled folder
        let validSnippet = try #require(repository.insertSnippet(to: enabledFolder.id))
        repository.updateSnippetTitle(validSnippet.id, title: "target snippet valid")

        // Disabled snippet in enabled folder
        let disabledSnippet = try #require(repository.insertSnippet(to: enabledFolder.id))
        repository.updateSnippetTitle(disabledSnippet.id, title: "target snippet disabled")
        repository.updateSnippetIsEnabled(disabledSnippet.id, isEnabled: false)

        // Enabled snippet in disabled folder
        let snippetInDisabledFolder = try #require(repository.insertSnippet(to: disabledFolder.id))
        repository.updateSnippetTitle(snippetInDisabledFolder.id, title: "target snippet in disabled folder")

        // Search with FTS (3+ chars)
        let ftsMatches = repository.searchSnippets(matching: "target snippet", limit: 10)
        #expect(ftsMatches.map(\.snippet.id) == [validSnippet.id])

        // Search with LIKE fallback (< 3 chars)
        let likeMatches = repository.searchSnippets(matching: "ta", limit: 10)
        #expect(likeMatches.map(\.snippet.id) == [validSnippet.id])
    }

    @Test
    func searchSnippetsWithLimit() throws {
        let folder = try #require(repository.insertFolder())
        for i in 1...5 {
            let snippet = try #require(repository.insertSnippet(to: folder.id))
            repository.updateSnippetTitle(snippet.id, title: "multi search item \(i)")
        }

        let matches = repository.searchSnippets(matching: "multi search", limit: 2)
        #expect(matches.count == 2)
    }

    @Test
    func searchSnippetsWithEmptyOrWhitespaceQuery() throws {
        let folder = try #require(repository.insertFolder())
        _ = repository.insertSnippet(to: folder.id)

        #expect(repository.searchSnippets(matching: "", limit: 10).isEmpty)
        #expect(repository.searchSnippets(matching: "   \t\n  ", limit: 10).isEmpty)
    }
}

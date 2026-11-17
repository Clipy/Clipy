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
    func searchUsesNormalizedTitleContentAndFolderNames() throws {
        let details = try #require(repository.insertFolders([
            (title: "か\u{3099}きく Folder \"OR\"", snippets: [
                (title: "は\u{309A}ひふ GitHub", content: "Α\u{345}\u{301} 100%_ \"AND\" café")
            ])
        ]))
        let snippet = try #require(details.first?.snippets.first)
        for query in ["が", "がきく", "\"OR\"", "ぱ", "ぱひふ", "g", "gi", "GITHUB", "άι", "%_", "\"AND\"", "café"] {
            let matches = repository.searchSnippets(query: query, folderID: nil, limit: 10)
            #expect(matches.map(\.snippet.id) == [snippet.id], "Query: \(query)")
        }
        for query in ["missing", "かきく", "はひふ", "cafe", "\0", " \n "] {
            #expect(repository.searchSnippets(query: query, folderID: nil, limit: 10).isEmpty, "Query: \(query)")
        }
        #expect(repository.searchSnippets(query: "GitHub", folderID: nil, limit: 0).isEmpty)
    }

    @Test(arguments: ["ma", "match"])
    func searchPrioritizesDirectMatchesBeforeFolderMatchesAndLimiting(query: String) throws {
        let details = try #require(repository.insertFolders([
            (title: "Match first", snippets: [(title: "Match A", content: ""), (title: "B", content: "")]),
            (title: "Second", snippets: [(title: "C", content: "Match"), (title: "Match D", content: "")]),
            (title: "Match disabled", snippets: [(title: "Match E", content: "")])
        ]))
        let first = details[0], second = details[1], disabled = details[2]
        repository.updateFolderIsEnabled(disabled.folder.id, isEnabled: false)
        repository.updateSnippetIsEnabled(second.snippets[1].id, isEnabled: false)
        repository.updateFolderIndexes([second.folder.id, first.folder.id, disabled.folder.id])
        repository.updateSnippetIndexes([first.snippets[1].id, first.snippets[0].id])
        let matches = repository.searchSnippets(query: query, folderID: nil, limit: 3)
        #expect(matches.map(\.snippet.id) == [second.snippets[0].id, first.snippets[0].id, first.snippets[1].id])
        #expect(matches.map(\.folderTitle) == ["Second", "Match first", "Match first"])
        #expect(repository.searchSnippets(query: query, folderID: nil, limit: 2).map(\.snippet.id)
            == [second.snippets[0].id, first.snippets[0].id])
        #expect(repository.searchSnippets(query: query, folderID: first.folder.id, limit: 10).map(\.snippet.id)
            == [first.snippets[0].id, first.snippets[1].id])
        #expect(repository.searchSnippets(query: query, folderID: disabled.folder.id, limit: 10).isEmpty)
    }

    @Test
    func searchRequiresEveryTermAcrossTitleContentAndFolderAndPrioritizesDirectMatches() throws {
        let details = try #require(repository.insertFolders([
            (title: "Work GitHub token 42", snippets: [(title: "Saved", content: "Other")]),
            (title: "Work", snippets: [(title: "GitHub", content: "TOKEN 42"), (title: "GitHub", content: "Missing")]),
            (title: "Work disabled", snippets: [(title: "GitHub", content: "token 42")])
        ]))
        repository.updateFolderIsEnabled(details[2].folder.id, isEnabled: false)
        let directID = details[1].snippets[0].id
        let folderOnlyID = details[0].snippets[0].id

        for query in ["token github work", "  WORK\tgithub\n42  ", "work　token", "wo 42", "work github github token"] {
            let results = repository.searchSnippets(query: query, folderID: nil, limit: 10)
            #expect(results.map(\.snippet.id) == [directID, folderOnlyID], "Query: \(query)")
            #expect(repository.searchSnippets(query: query, folderID: nil, limit: 1).map(\.snippet.id) == [directID])
            #expect(repository.searchSnippets(query: query, folderID: details[1].folder.id, limit: 10).map(\.snippet.id) == [directID])
        }
        #expect(repository.searchSnippets(query: "work nonexistent", folderID: nil, limit: 10).isEmpty)
    }

    @Test(arguments: ["ma", "match"])
    func searchKeepsTitleAndContentMatchesWhenFolderMatchesExceedTheLimit(query: String) throws {
        let folderOnlySnippets = (0..<20).map { (title: "Saved \($0)", content: "Content") }
        let details = try #require(repository.insertFolders([
            (title: "Match folder", snippets: folderOnlySnippets),
            (title: "Other folder", snippets: [
                (title: "Match title", content: ""),
                (title: "Saved text", content: "Match content")
            ])
        ]))

        let matches = repository.searchSnippets(query: query, folderID: nil, limit: 2)

        #expect(matches.map(\.snippet.id) == details[1].snippets.map(\.id))
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
}

//
//  SnippetSyncFileTests.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/09/28.
//
//  Copyright © 2015-2026 Clipy Project.
//

import Foundation
import Testing
@testable import Clipy

struct SnippetSyncFileTests {
    @Test
    func encodesAndDecodesWithTombstonesRoundTrip() throws {
        let folderID = UUID()
        let snippetID = UUID()
        let deletedFolderID = UUID()
        let deletedSnippetID = UUID()

        let file = SnippetSyncFile(
            folders: [
                SnippetSyncFolderRecord(id: folderID, title: "Ideas", index: 0, isEnabled: true, updatedAt: 1_700_000_000)
            ],
            snippets: [
                SnippetSyncSnippetRecord(
                    id: snippetID,
                    folderID: folderID,
                    title: "Sample Snippet",
                    content: "sample content\nwith a newline",
                    index: 0,
                    isEnabled: false,
                    updatedAt: 1_700_000_001
                )
            ],
            deletedFolders: [SnippetSyncTombstone(id: deletedFolderID, deletedAt: 1_700_000_002)],
            deletedSnippets: [SnippetSyncTombstone(id: deletedSnippetID, deletedAt: 1_700_000_003)]
        )

        let data = try JSONEncoder().encode(file)
        let decoded = try JSONDecoder().decode(SnippetSyncFile.self, from: data)

        #expect(decoded == file)
    }

    @Test
    func decodesAMinimalHandWrittenDocument() throws {
        let json = """
        {
          "folders": [
            { "id": "9B1DE1C1-0000-4000-8000-000000000001", "title": "Folder", "index": 0, "isEnabled": true, "updatedAt": 10 }
          ],
          "snippets": [
            { "id": "9B1DE1C1-0000-4000-8000-000000000002", "folderID": "9B1DE1C1-0000-4000-8000-000000000001", "title": "Snippet", "content": "Body", "index": 0, "isEnabled": true, "updatedAt": 11 }
          ],
          "deletedFolders": [],
          "deletedSnippets": [
            { "id": "9B1DE1C1-0000-4000-8000-000000000003", "deletedAt": 12 }
          ]
        }
        """

        let file = try JSONDecoder().decode(SnippetSyncFile.self, from: Data(json.utf8))

        #expect(file.folders.map(\.title) == ["Folder"])
        #expect(file.snippets.map(\.title) == ["Snippet"])
        #expect(file.deletedFolders.isEmpty)
        #expect(file.deletedSnippets.map(\.deletedAt) == [12])
    }

    @Test
    func emptyFileRoundTrips() throws {
        let data = try JSONEncoder().encode(SnippetSyncFile.empty)
        let decoded = try JSONDecoder().decode(SnippetSyncFile.self, from: data)

        #expect(decoded == SnippetSyncFile.empty)
    }

    @Test
    func convertsToAndFromDatabaseModelsPreservingAllFields() {
        let folder = SnippetFolder(
            id: SnippetFolder.ID(rawValue: UUID()),
            title: "Folder",
            index: 3,
            isEnabled: false,
            updatedAt: 42
        )
        let snippet = Snippet(
            id: Snippet.ID(rawValue: UUID()),
            folderID: folder.id,
            title: "Snippet",
            content: "Content",
            index: 2,
            isEnabled: true,
            updatedAt: 43
        )

        let folderRecord = SnippetSyncFolderRecord(folder)
        let snippetRecord = SnippetSyncSnippetRecord(snippet)

        #expect(SnippetFolder(folderRecord) == folder)
        #expect(Snippet(snippetRecord) == snippet)
    }
}

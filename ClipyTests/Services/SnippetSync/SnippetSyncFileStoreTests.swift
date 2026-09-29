//
//  SnippetSyncFileStoreTests.swift
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

final class SnippetSyncFileStoreTests {
    let store = SnippetSyncFileStore()
    let directoryURL: URL

    init() throws {
        directoryURL = FileManager.default.temporaryDirectory.appending(path: UUID().uuidString)
        try FileManager.default.createDirectory(at: directoryURL, withIntermediateDirectories: true)
    }

    deinit {
        try? FileManager.default.removeItem(at: directoryURL)
    }

    @Test
    func readingAMissingFileReturnsNotFound() {
        let url = directoryURL.appendingPathComponent(SnippetSyncFileStore.fileName)

        guard case .notFound = store.read(at: url) else {
            Issue.record("Expected .notFound")
            return
        }
    }

    @Test
    func writingThenReadingRoundTripsTheFile() throws {
        let url = directoryURL.appendingPathComponent(SnippetSyncFileStore.fileName)
        let file = SnippetSyncFile(
            folders: [SnippetSyncFolderRecord(id: UUID(), title: "Folder", index: 0, isEnabled: true, updatedAt: 100)],
            snippets: [],
            deletedFolders: [],
            deletedSnippets: []
        )

        #expect(store.write(file, to: url))
        #expect(FileManager.default.fileExists(atPath: url.path))

        guard case let .success(readFile) = store.read(at: url) else {
            Issue.record("Expected .success")
            return
        }
        #expect(readFile == file)
    }

    @Test
    func writingTwiceReplacesThePreviousContentsAtomically() throws {
        let url = directoryURL.appendingPathComponent(SnippetSyncFileStore.fileName)
        let firstFile = SnippetSyncFile(
            folders: [SnippetSyncFolderRecord(id: UUID(), title: "First", index: 0, isEnabled: true, updatedAt: 1)],
            snippets: [],
            deletedFolders: [],
            deletedSnippets: []
        )
        let secondFile = SnippetSyncFile(
            folders: [SnippetSyncFolderRecord(id: UUID(), title: "Second", index: 0, isEnabled: true, updatedAt: 2)],
            snippets: [],
            deletedFolders: [],
            deletedSnippets: []
        )

        #expect(store.write(firstFile, to: url))
        #expect(store.write(secondFile, to: url))

        guard case let .success(readFile) = store.read(at: url) else {
            Issue.record("Expected .success")
            return
        }
        #expect(readFile == secondFile)

        // No leftover temporary files from either write.
        let remainingFiles = try FileManager.default.contentsOfDirectory(atPath: directoryURL.path)
        #expect(remainingFiles == [SnippetSyncFileStore.fileName])
    }

    @Test
    func readingCorruptJSONReturnsUnreadable() throws {
        let url = directoryURL.appendingPathComponent(SnippetSyncFileStore.fileName)
        try Data("not valid json".utf8).write(to: url)

        guard case .unreadable = store.read(at: url) else {
            Issue.record("Expected .unreadable")
            return
        }
    }
}

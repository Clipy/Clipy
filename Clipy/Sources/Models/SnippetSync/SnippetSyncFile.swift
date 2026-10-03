//
//  SnippetSyncFile.swift
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

/// The JSON document written to the shared sync folder. It carries the full state of every
/// synced snippet folder and snippet, plus tombstones recording deletions so that other Macs
/// know to remove records instead of resurrecting them on the next merge.
struct SnippetSyncFile: Codable, Equatable {
    var folders: [SnippetSyncFolderRecord]
    var snippets: [SnippetSyncSnippetRecord]
    var deletedFolders: [SnippetSyncTombstone]
    var deletedSnippets: [SnippetSyncTombstone]

    static let empty = SnippetSyncFile(folders: [], snippets: [], deletedFolders: [], deletedSnippets: [])
}

/// Records that a folder or snippet with `id` was deleted at `deletedAt` (unix milliseconds).
struct SnippetSyncTombstone: Codable, Equatable, Identifiable {
    let id: UUID
    let deletedAt: Int
}

struct SnippetSyncFolderRecord: Codable, Equatable, Identifiable {
    let id: UUID
    var title: String
    var index: Int
    var isEnabled: Bool
    var updatedAt: Int
}

struct SnippetSyncSnippetRecord: Codable, Equatable, Identifiable {
    let id: UUID
    var folderID: UUID
    var title: String
    var content: String
    var index: Int
    var isEnabled: Bool
    var updatedAt: Int
}

// MARK: - Conversions to/from the local database model

extension SnippetSyncFolderRecord {
    init(_ folder: SnippetFolder) {
        self.init(
            id: folder.id.rawValue,
            title: folder.title,
            index: folder.index,
            isEnabled: folder.isEnabled,
            updatedAt: folder.updatedAt
        )
    }
}

extension SnippetFolder {
    init(_ record: SnippetSyncFolderRecord) {
        self.init(
            id: SnippetFolder.ID(rawValue: record.id),
            title: record.title,
            index: record.index,
            isEnabled: record.isEnabled,
            updatedAt: record.updatedAt
        )
    }
}

extension SnippetSyncSnippetRecord {
    init(_ snippet: Snippet) {
        self.init(
            id: snippet.id.rawValue,
            folderID: snippet.folderID.rawValue,
            title: snippet.title,
            content: snippet.content,
            index: snippet.index,
            isEnabled: snippet.isEnabled,
            updatedAt: snippet.updatedAt
        )
    }
}

extension Snippet {
    init(_ record: SnippetSyncSnippetRecord) {
        self.init(
            id: Snippet.ID(rawValue: record.id),
            folderID: SnippetFolder.ID(rawValue: record.folderID),
            title: record.title,
            content: record.content,
            index: record.index,
            isEnabled: record.isEnabled,
            updatedAt: record.updatedAt
        )
    }
}

// 
//  SnippetRepository.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
// 
//  Created by Shunsuke Furubayashi on 2026/05/23.
// 
//  Copyright © 2015-2026 Clipy Project.
//

import Combine
import Dependencies
import SQLiteData

protocol SnippetRepositoryProtocol {
    func observeFolderDetails() -> AnyPublisher<[SnippetFolderDetail], Never>
    func fetchFolderDetails() -> [SnippetFolderDetail]
    func fetchFolderDetail(id: SnippetFolder.ID) -> SnippetFolderDetail?

    func insertFolder() -> SnippetFolder?
    func insertFolders(_ folders: [(title: String, snippets: [(title: String, content: String)])]) -> [SnippetFolderDetail]?
    func updateFolderTitle(_ id: SnippetFolder.ID, title: String)
    func updateFolderIsEnabled(_ id: SnippetFolder.ID, isEnabled: Bool)
    func updateFolderIndexes(_ folderIDs: [SnippetFolder.ID])
    func deleteFolder(_ id: SnippetFolder.ID)

    func fetchSnippet(id: Snippet.ID) -> Snippet?
    func insertSnippet(to id: SnippetFolder.ID) -> Snippet?
    func updateSnippetTitle(_ id: Snippet.ID, title: String)
    func updateSnippetContent(_ id: Snippet.ID, content: String)
    func updateSnippetIsEnabled(_ id: Snippet.ID, isEnabled: Bool)
    func updateSnippetIndexes(_ snippetIDs: [Snippet.ID])
    func moveSnippet(_ id: Snippet.ID, to folderID: SnippetFolder.ID, snippetIDs: [Snippet.ID])
    func deleteSnippet(_ id: Snippet.ID)
}

final class SnippetRepository: SnippetRepositoryProtocol {
    @Dependency(\.defaultDatabase)
    private var database

    @FetchAll(SnippetFolder.all.order(by: \.index))
    private var folders
    @FetchAll(Snippet.all.order(by: \.index))
    private var snippets

    func observeFolderDetails() -> AnyPublisher<[SnippetFolderDetail], Never> {
        Publishers.CombineLatest(_folders.publisher, _snippets.publisher)
            .map { Self.folderDetails(folders: $0, snippets: $1) }
            .eraseToAnyPublisher()
    }

    func fetchFolderDetails() -> [SnippetFolderDetail] {
        withErrorReporting {
            try database.read { database in
                let folders = try SnippetFolder.all.order(by: \.index)
                    .fetchAll(database)
                let snippets = try Snippet.all.order(by: \.index)
                    .fetchAll(database)
                return Self.folderDetails(folders: folders, snippets: snippets)
            }
        } ?? []
    }

    func fetchFolderDetail(id: SnippetFolder.ID) -> SnippetFolderDetail? {
        withErrorReporting {
            try database.read { database in
                guard let folder = try SnippetFolder.find(id).fetchOne(database) else {
                    return nil
                }
                let snippets = try Snippet.where { $0.folderID.eq(id) }.order(by: \.index).fetchAll(database)
                return SnippetFolderDetail(folder: folder, snippets: snippets)
            }
        }
    }

    func insertFolder() -> SnippetFolder? {
        withErrorReporting {
            return try database.write { database in
                let lastIndex = try SnippetFolder.order { $0.index.desc() }
                    .select { $0.index }
                    .fetchOne(database) ?? -1
                let folder = SnippetFolder.Draft(
                    title: "untitled folder",
                    index: lastIndex + 1,
                    isEnabled: true
                )
                return try SnippetFolder.insert { folder }.returning(\.self).fetchOne(database)
            }
        }
    }

    func insertFolders(_ folders: [(title: String, snippets: [(title: String, content: String)])]) -> [SnippetFolderDetail]? {
        withErrorReporting {
            try database.write { database in
                let lastIndex = try SnippetFolder.order { $0.index.desc() }
                    .select { $0.index }
                    .fetchOne(database) ?? -1
                var details = [SnippetFolderDetail]()
                try folders.enumerated().forEach { index, folders in
                    let folder = SnippetFolder.Draft(
                        title: folders.title,
                        index: lastIndex + index + 1,
                        isEnabled: true
                    )
                    guard let insertedFolder = try SnippetFolder.insert(values: { folder }).returning(\.self).fetchOne(database) else {
                        return
                    }
                    let snippets = folders.snippets.enumerated().map { snippetIndex, snippet in
                        Snippet.Draft(
                            folderID: insertedFolder.id,
                            title: snippet.title,
                            content: snippet.content,
                            index: snippetIndex,
                            isEnabled: true
                        )
                    }
                    let insertedSnippets = try Snippet.insert { snippets }.returning(\.self).fetchAll(database)
                    details.append(SnippetFolderDetail(folder: insertedFolder, snippets: insertedSnippets))
                }
                return details
            }
        }
    }

    func updateFolderTitle(_ id: SnippetFolder.ID, title: String) {
        withErrorReporting {
            try database.write { database in
                try SnippetFolder.where { $0.id.eq(id) }
                    .update { $0.title = title }
                    .execute(database)
            }
        }
    }

    func updateFolderIsEnabled(_ id: SnippetFolder.ID, isEnabled: Bool) {
        withErrorReporting {
            try database.write { database in
                try SnippetFolder.where { $0.id.eq(id) }
                    .update { $0.isEnabled = isEnabled }
                    .execute(database)
            }
        }
    }

    func updateFolderIndexes(_ folderIDs: [SnippetFolder.ID]) {
        withErrorReporting {
            try database.write { database in
                try folderIDs.enumerated().forEach { index, folderID in
                    try SnippetFolder.where { $0.id.eq(folderID) }
                        .update { $0.index = index }
                        .execute(database)
                }
            }
        }
    }

    func deleteFolder(_ id: SnippetFolder.ID) {
        withErrorReporting {
            try database.write { database in
                try SnippetFolder.delete().where { $0.id.eq(id) }.execute(database)
            }
        }
    }

    func fetchSnippet(id: Snippet.ID) -> Snippet? {
        withErrorReporting {
            try database.read { database in
                try Snippet.find(id).fetchOne(database)
            }
        }
    }

    func insertSnippet(to id: SnippetFolder.ID) -> Snippet? {
        withErrorReporting {
            return try database.write { database in
                let lastIndex = try Snippet.where { $0.folderID.eq(id) }
                    .order { $0.index.desc() }
                    .select { $0.index }
                    .fetchOne(database) ?? -1
                let snippet = Snippet.Draft(
                    folderID: id,
                    title: "untitled snippet",
                    content: "",
                    index: lastIndex + 1,
                    isEnabled: true
                )
                return try Snippet.insert { snippet }.returning(\.self).fetchOne(database)
            }
        }
    }

    func updateSnippetTitle(_ id: Snippet.ID, title: String) {
        withErrorReporting {
            try database.write { database in
                try Snippet.where { $0.id.eq(id) }
                    .update { $0.title = title }
                    .execute(database)
            }
        }
    }

    func updateSnippetContent(_ id: Snippet.ID, content: String) {
        withErrorReporting {
            try database.write { database in
                try Snippet.where { $0.id.eq(id) }
                    .update { $0.content = content }
                    .execute(database)
            }
        }
    }

    func updateSnippetIsEnabled(_ id: Snippet.ID, isEnabled: Bool) {
        withErrorReporting {
            try database.write { database in
                try Snippet.where { $0.id.eq(id) }
                    .update { $0.isEnabled = isEnabled }
                    .execute(database)
            }
        }
    }

    func updateSnippetIndexes(_ snippetIDs: [Snippet.ID]) {
        withErrorReporting {
            try database.write { database in
                try snippetIDs.enumerated().forEach { index, snippetID in
                    try Snippet.where { $0.id.eq(snippetID) }
                        .update { $0.index = index }
                        .execute(database)
                }
            }
        }
    }

    func moveSnippet(_ id: Snippet.ID, to folderID: SnippetFolder.ID, snippetIDs: [Snippet.ID]) {
        withErrorReporting {
            try database.write { database in
                try Snippet.where { $0.id.eq(id) }
                    .update { $0.folderID = folderID }
                    .execute(database)
                try snippetIDs.enumerated().forEach { index, snippetID in
                    try Snippet.where { $0.id.eq(snippetID) }
                        .update { $0.index = index }
                        .execute(database)
                }
            }
        }
    }

    func deleteSnippet(_ id: Snippet.ID) {
        withErrorReporting {
            try database.write { database in
                try Snippet.delete().where { $0.id.eq(id) }.execute(database)
            }
        }
    }
}

private extension SnippetRepository {
    static func folderDetails(folders: [SnippetFolder], snippets: [Snippet]) -> [SnippetFolderDetail] {
        let snippetsByFolderID = Dictionary(grouping: snippets, by: \.folderID)
        return folders.map { folder in
            SnippetFolderDetail(
                folder: folder,
                snippets: snippetsByFolderID[folder.id] ?? []
            )
        }
    }
}

private enum SnippetRepositoryKey: DependencyKey {
    static let liveValue: any SnippetRepositoryProtocol = SnippetRepository()
}

extension DependencyValues {
    var snippetRepository: SnippetRepositoryProtocol {
        get { self[SnippetRepositoryKey.self] }
        set { self[SnippetRepositoryKey.self] = newValue }
    }
}

//
//  PasteboardHistoryRepository.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/05/28.
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import Combine
import Dependencies
import SQLiteData

protocol PasteboardHistoryRepositoryProtocol {
    func observeHistories() -> AnyPublisher<[PasteboardHistory], Never>
    func hasHistories() -> Bool
    func fetchHistoryDetails(
        sortsByCreatedAt: Bool,
        includesThumbnailAsset: Bool,
        limit: Int,
    ) -> [PasteboardHistoryDetail]
    func searchHistories(
        matching query: String,
        includesThumbnailAsset: Bool,
        limit: Int,
    ) -> [PasteboardHistoryDetail]
    func fetchHistory(id: PasteboardHistory.ID) -> PasteboardHistory?
    func fetchContent(id: PasteboardHistory.ID) -> PasteboardContent?

    func save(id: PasteboardHistory.ID, content: PasteboardContent, updateAt: Int)
    func updateOCRText(id: PasteboardHistory.ID, ocrText: String)
    func deleteHistory(id: PasteboardHistory.ID)
    func deleteAll()
    func deleteOverflowingHistories(sortsByCreatedAt: Bool, maxHistorySize: Int)
}

final class PasteboardHistoryRepository: PasteboardHistoryRepositoryProtocol {
    @Dependency(\.defaultDatabase)
    private var database
    @Dependency(\.deviceIdentifier)
    private var deviceIdentifier

    @FetchAll(PasteboardHistory.all.order { $0.updateAt.desc() })
    private var histories

    func observeHistories() -> AnyPublisher<[PasteboardHistory], Never> {
        _histories.publisher.eraseToAnyPublisher()
    }

    func hasHistories() -> Bool {
        withErrorReporting {
            try database.read { database in
                try PasteboardHistory
                    .select { $0.id }
                    .limit(1)
                    .fetchOne(database) != nil
            }
        } ?? false
    }

    func fetchHistoryDetails(
        sortsByCreatedAt: Bool,
        includesThumbnailAsset: Bool,
        limit: Int
    ) -> [PasteboardHistoryDetail] {
        withErrorReporting {
            try database.read { database in
                let histories = PasteboardHistory
                    .all
                    .order { columns in
                        if sortsByCreatedAt {
                            columns.createdAt.desc()
                        } else {
                            columns.updateAt.desc()
                        }
                    }
                    .limit(limit)

                guard includesThumbnailAsset else {
                    return try histories
                        .fetchAll(database)
                        .map { PasteboardHistoryDetail(history: $0, thumbnailAsset: nil) }
                }

                return try histories
                    .leftJoin(PasteboardHistoryThumbnailAsset.all) { $0.id.eq($1.pasteboardHistoryID) }
                    .select { PasteboardHistoryDetail.Columns(history: $0, thumbnailAsset: $1) }
                    .fetchAll(database)
            }
        } ?? []
    }

    func searchHistories(
        matching query: String,
        includesThumbnailAsset: Bool,
        limit: Int
    ) -> [PasteboardHistoryDetail] {
        let query = query.trimmingCharacters(in: .whitespacesAndNewlines)
        guard !query.isEmpty else { return [] }

        return withErrorReporting {
            try database.read { database in
                // The search indexes are tokenized with `trigram`, which cannot match queries shorter
                // than three characters. Fall back to a LIKE scan so that typing the first character
                // still narrows the candidates down instead of emptying the list.
                guard query.count >= Constants.Search.minimumTrigramLength else {
                    let histories = try PasteboardHistory
                        .where { $0.title.contains(query) }
                        .order { $0.updateAt.desc() }
                        .limit(limit)
                        .fetchAll(database)
                    return try attachThumbnailAssets(
                        to: histories,
                        includesThumbnailAsset: includesThumbnailAsset,
                        database: database
                    )
                }

                let results = try PasteboardHistorySearch
                    .where { $0.match(query.fts5PhraseQuery) }
                    .leftJoin(PasteboardHistory.all) { $0.id.eq($1.id) }
                    .leftJoin(PasteboardHistoryThumbnailAsset.all) { $0.id.eq($2.pasteboardHistoryID) }
                    .select { PasteboardHistorySearchResult.Columns(history: $1, thumbnailAsset: $2) }
                    .limit(limit)
                    .fetchAll(database)
                return results.compactMap { result in
                    result.history.map {
                        PasteboardHistoryDetail(
                            history: $0,
                            thumbnailAsset: includesThumbnailAsset ? result.thumbnailAsset : nil
                        )
                    }
                }
            }
        } ?? []
    }

    func fetchHistory(id: PasteboardHistory.ID) -> PasteboardHistory? {
        withErrorReporting {
            try database.read { database in
                try PasteboardHistory.find(id).fetchOne(database)
            }
        }
    }

    func fetchContent(id: PasteboardHistory.ID) -> PasteboardContent? {
        withErrorReporting {
            try database.read { database in
                let assets = try PasteboardHistoryAsset
                    .where { $0.pasteboardHistoryID.eq(id) }
                    .order(by: \.index)
                    .fetchAll(database)
                return PasteboardContent(
                    assets: assets.map {
                        PasteboardContent.Asset(type: $0.pasteboardType, data: $0.data)
                    }
                )
            }
        }
    }

    func save(id: PasteboardHistory.ID, content: PasteboardContent, updateAt: Int) {
        withErrorReporting {
            try database.write { database in
                let existingHistory = try PasteboardHistory
                    .find(id)
                    .fetchOne(database)
                let history = PasteboardHistory(
                    id: id,
                    title: content.stringValue.map { String($0.prefix(10000)) } ?? "",
                    ocrText: existingHistory?.ocrText,
                    pasteboardTypes: content.types,
                    createdAt: existingHistory?.createdAt ?? updateAt,
                    updateAt: updateAt,
                    deviceID: deviceIdentifier
                )
                try PasteboardHistory
                    .upsert { history }
                    .execute(database)
                // When a history already exists, its ID is derived from the content hash,
                // so the assets are guaranteed to be identical and do not need to be inserted again.
                if existingHistory == nil {
                    let assets = content.assets.enumerated().map { index, asset in
                        PasteboardHistoryAsset.Draft(
                            pasteboardHistoryID: id,
                            index: index,
                            pasteboardType: asset.type,
                            data: asset.data
                        )
                    }
                    try PasteboardHistoryAsset.insert { assets }.execute(database)
                    if let thumbnailAsset = thumbnailAsset(from: content, id: id) {
                        try PasteboardHistoryThumbnailAsset.insert { thumbnailAsset }.execute(database)
                    }
                }
            }
        }
    }

    func updateOCRText(id: PasteboardHistory.ID, ocrText: String) {
        withErrorReporting {
            try database.write { database in
                try PasteboardHistory
                    .find(id)
                    .update { $0.ocrText = #bind(ocrText) }
                    .execute(database)
            }
        }
    }

    func deleteHistory(id: PasteboardHistory.ID) {
        withErrorReporting {
            try database.write { database in
                try PasteboardHistory
                    .delete()
                    .where { $0.id.eq(id) }
                    .execute(database)
            }
        }
    }

    func deleteAll() {
        withErrorReporting {
            try database.write { database in
                try PasteboardHistory.delete().execute(database)
            }
        }
    }

    func deleteOverflowingHistories(sortsByCreatedAt: Bool, maxHistorySize: Int) {
        guard maxHistorySize > 0 else {
            deleteAll()
            return
        }
        withErrorReporting {
            try database.write { database in
                let deletingIDs = try PasteboardHistory
                    .order { columns in
                        if sortsByCreatedAt {
                            columns.createdAt.desc()
                        } else {
                            columns.updateAt.desc()
                        }
                    }
                    .limit(-1)
                    .offset(maxHistorySize)
                    .select { $0.id }
                    .fetchAll(database)
                guard !deletingIDs.isEmpty else { return }
                try PasteboardHistory
                    .delete()
                    .where { $0.id.in(deletingIDs) }
                    .execute(database)
            }
        }
    }
}

private extension PasteboardHistoryRepository {
    func attachThumbnailAssets(
        to histories: [PasteboardHistory],
        includesThumbnailAsset: Bool,
        database: Database
    ) throws -> [PasteboardHistoryDetail] {
        guard includesThumbnailAsset, !histories.isEmpty else {
            return histories.map { PasteboardHistoryDetail(history: $0, thumbnailAsset: nil) }
        }
        let assets = try PasteboardHistoryThumbnailAsset
            .where { $0.pasteboardHistoryID.in(histories.map(\.id)) }
            .fetchAll(database)
        let assetsByHistoryID = Dictionary(
            assets.map { ($0.pasteboardHistoryID, $0) },
            uniquingKeysWith: { first, _ in first }
        )
        return histories.map {
            PasteboardHistoryDetail(history: $0, thumbnailAsset: assetsByHistoryID[$0.id])
        }
    }

    func thumbnailAsset(from content: PasteboardContent, id: PasteboardHistory.ID) -> PasteboardHistoryThumbnailAsset? {
        var asset: PasteboardHistoryThumbnailAsset?
        if let thumbnailImage = content.thumbnailImage, let thumbnailData = thumbnailImage.tiffRepresentation {
            asset = PasteboardHistoryThumbnailAsset(
                pasteboardHistoryID: id,
                kind: .image,
                data: thumbnailData
            )
        }
        if let colorCodeImage = content.colorCodeImage, let colorCodeData = colorCodeImage.tiffRepresentation {
            asset = PasteboardHistoryThumbnailAsset(
                pasteboardHistoryID: id,
                kind: .colorCode,
                data: colorCodeData
            )
        }
        return asset
    }
}

extension DependencyValues {
    var pasteboardHistoryRepository: PasteboardHistoryRepositoryProtocol {
        get { self[PasteboardHistoryRepositoryKey.self] }
        set { self[PasteboardHistoryRepositoryKey.self] = newValue }
    }

    private enum PasteboardHistoryRepositoryKey: DependencyKey {
        static let liveValue: any PasteboardHistoryRepositoryProtocol = PasteboardHistoryRepository()
    }
}

//
//  DatabaseMigration.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/05/22.
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import Dependencies
import RealmSwift
import SQLiteData

struct DatabaseMigration {
    @Dependency(\.defaultDatabase)
    private var database
    @Dependency(\.realmConfiguration)
    private var realmConfiguration

    func migrateFromRealmToSQLiteData() {
        guard let realm = realm() else { return }

        let histories = migratePasteboardHistories(from: realm)
        let snippets = migrateSnippets(from: realm)

        withErrorReporting {
            try database.write { database in
                try PasteboardHistory.upsert { histories.0 }.execute(database)
                try PasteboardHistoryAsset.upsert { histories.1 }.execute(database)
                try PasteboardHistoryThumbnailAsset.upsert { histories.2 }.execute(database)

                try SnippetFolder.upsert { snippets.0 }.execute(database)
                try Snippet.upsert { snippets.1 }.execute(database)
            }
        }
    }

    // swiftlint:disable:next large_tuple
    private func migratePasteboardHistories(from realm: Realm) -> ([PasteboardHistory], [PasteboardHistoryAsset], [PasteboardHistoryThumbnailAsset]) {
        var historiesByID = [PasteboardHistory.ID: PasteboardHistory]()
        var assetsByHistoryID = [PasteboardHistory.ID: [PasteboardHistoryAsset]]()
        var thumbnailsByHistoryID = [PasteboardHistory.ID: PasteboardHistoryThumbnailAsset]()

        realm.objects(CPYClip.self)
            .forEach { clip in
                guard let data = NSKeyedUnarchiver.unarchiveObject(withFile: clip.dataPath) as? CPYClipData else { return }
                guard let content = data.toPasteboardContent() else { return }

                // If multiple histories contain the same data, keep only the latest history.
                let id = PasteboardHistory.ID(rawValue: content.hash)
                if historiesByID[id]?.updateAt ?? Int.min < clip.updateTime {
                    historiesByID[id] = PasteboardHistory(
                        id: id,
                        title: clip.title,
                        pasteboardTypes: content.types,
                        updateAt: clip.updateTime,
                        deviceID: CPYUtilities.deviceID
                    )
                }
                if assetsByHistoryID[id] == nil {
                    assetsByHistoryID[id] = content.assets.enumerated().map { index, asset in
                        PasteboardHistoryAsset(
                            id: .init(UUID()),
                            pasteboardHistoryID: id,
                            index: index,
                            pasteboardType: asset.type,
                            data: asset.data
                        )
                    }
                }
                if thumbnailsByHistoryID[id] == nil, let thumbnail = thumbnailAsset(from: content, id: id) {
                    thumbnailsByHistoryID[id] = thumbnail
                }
            }
        return (
            Array(historiesByID.values),
            assetsByHistoryID.values.flatMap { $0 },
            Array(thumbnailsByHistoryID.values)
        )
    }

    private func migrateSnippets(from realm: Realm) -> ([SnippetFolder], [Snippet]) {
        var folders = [SnippetFolder]()
        var snippets = [Snippet]()

        for folder in realm.objects(CPYFolder.self) {
            let id = SnippetFolder.ID(rawValue: UUID(uuidString: folder.identifier) ?? UUID())
            folders.append(
                SnippetFolder(
                    id: id,
                    title: folder.title,
                    index: folder.index,
                    isEnabled: folder.enable
                )
            )
            for snippet in folder.snippets {
                snippets.append(
                    Snippet(
                        id: Snippet.ID(rawValue: UUID(uuidString: snippet.identifier) ?? UUID()),
                        folderID: id,
                        title: snippet.title,
                        content: snippet.content,
                        index: snippet.index,
                        isEnabled: snippet.enable
                    )
                )
            }
        }

        return (folders, snippets)
    }
}

private extension DatabaseMigration {
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

private extension DatabaseMigration {
    func realm() -> Realm? {
        do {
            return try Realm(configuration: realmConfiguration)
        } catch {
            guard let url = realmConfiguration.fileURL else { return nil }

            // If Realm cannot be opened, remove the .lock file and management folder to make it open whenever possible.
            let lockURL = url.appendingPathExtension("lock")
            let managementURL = url.appendingPathExtension("management")
            try? FileManager.default.removeItem(at: lockURL)
            try? FileManager.default.removeItem(at: managementURL)

            return try? Realm(configuration: realmConfiguration)
        }
    }
}

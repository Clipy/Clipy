//
//  SQLiteDataSchema.swift
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
import SQLiteData
import Tagged

@Table
struct PasteboardHistory: Identifiable, Equatable {
    typealias ID = Tagged<Self, String>

    @Column(primaryKey: true)
    let id: ID
    let title: String
    @Column(as: [NSPasteboard.PasteboardType].JSONRepresentation.self)
    let pasteboardTypes: [NSPasteboard.PasteboardType]
    let updateAt: Int
    let deviceID: String?

    var primaryType: NSPasteboard.PasteboardType? {
        pasteboardTypes.first
    }
}

@Table
struct PasteboardHistoryAsset: Identifiable, Equatable {
    typealias ID = Tagged<Self, UUID>

    @Column(primaryKey: true)
    let id: ID
    let pasteboardHistoryID: PasteboardHistory.ID
    let index: Int
    let pasteboardType: NSPasteboard.PasteboardType
    let data: Data
}

@Table
struct PasteboardHistoryThumbnailAsset: Identifiable, Equatable {
    @Column(primaryKey: true)
    let pasteboardHistoryID: PasteboardHistory.ID
    let kind: Kind
    let data: Data
    var id: PasteboardHistory.ID { pasteboardHistoryID }

    enum Kind: String, QueryBindable {
        case image
        case colorCode
    }
}

@Selection
struct PasteboardHistoryDetail: Equatable {
    let history: PasteboardHistory
    let thumbnailAsset: PasteboardHistoryThumbnailAsset?
}

@Table
struct PasteboardHistorySearch: FTS5, Equatable {
    let id: PasteboardHistory.ID
    let title: String
}

@Selection
struct PasteboardHistorySearchResult: Equatable {
    let history: PasteboardHistory?
    let thumbnailAsset: PasteboardHistoryThumbnailAsset?
}

@Table
struct SnippetFolder: Identifiable, Equatable {
    typealias ID = Tagged<Self, UUID>

    @Column(primaryKey: true)
    let id: ID
    let title: String
    let index: Int
    let isEnabled: Bool
}

@Table
struct Snippet: Identifiable, Equatable {
    typealias ID = Tagged<Self, UUID>

    @Column(primaryKey: true)
    let id: ID
    let folderID: SnippetFolder.ID
    let title: String
    let content: String
    let index: Int
    let isEnabled: Bool
}

@Table
struct SnippetSearch: FTS5, Equatable {
    let id: Snippet.ID
    let title: String
    let content: String
}

@Selection
struct SnippetSearchResult: Equatable {
    let snippet: Snippet?
}

extension NSPasteboard.PasteboardType: @retroactive SQLiteType {}
extension NSPasteboard.PasteboardType: @retroactive QueryBindable {}
extension NSPasteboard.PasteboardType: @retroactive Codable {}

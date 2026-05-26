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
struct PasteboardHistory: Identifiable {
    typealias ID = Tagged<Self, String>

    @Column(primaryKey: true)
    let id: ID
    let title: String
    @Column(as: [NSPasteboard.PasteboardType].JSONRepresentation.self)
    let pasteboardTypes: [NSPasteboard.PasteboardType]
    let updateAt: Int
}

@Table
struct PasteboardHistoryAsset: Identifiable {
    typealias ID = Tagged<Self, String>

    @Column(primaryKey: true)
    let id: ID
    let pasteboardHistoryID: PasteboardHistory.ID
    let pasteboardType: NSPasteboard.PasteboardType
    let data: Data

    init(pasteboardHistoryID: PasteboardHistory.ID, pasteboardType: NSPasteboard.PasteboardType, data: Data) {
        self.id = ID(rawValue: "\(pasteboardHistoryID.rawValue)_\(pasteboardType.rawValue)")
        self.pasteboardHistoryID = pasteboardHistoryID
        self.pasteboardType = pasteboardType
        self.data = data
    }
}

@Table
struct PasteboardHistoryThumbnailAsset: Identifiable {
    @Column(primaryKey: true)
    let pasteboardHistoryID: PasteboardHistory.ID
    let data: Data
    var id: PasteboardHistory.ID { pasteboardHistoryID }
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

extension NSPasteboard.PasteboardType: @retroactive SQLiteType {}
extension NSPasteboard.PasteboardType: @retroactive QueryBindable {}
extension NSPasteboard.PasteboardType: @retroactive Codable {}

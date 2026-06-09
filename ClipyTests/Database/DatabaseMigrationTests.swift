//
//  DatabaseMigrationTests.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/06/04.
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import Dependencies
import DependenciesTestSupport
import RealmSwift
import SQLiteData
import Testing
@testable import Clipy

@MainActor
@Suite(
    .serialized,
    .dependencies {
        $0.realmConfiguration = Realm.Configuration(inMemoryIdentifier: UUID().uuidString)
        try $0.bootstrapDatabase()
    }
)
final class DatabaseMigrationTests {
    @Dependency(\.defaultDatabase)
    var database
    @Dependency(\.realmConfiguration)
    var realmConfiguration

    let migration: DatabaseMigration
    let temporaryDirectoryURL: URL

    init() throws {
        self.migration = DatabaseMigration()
        self.temporaryDirectoryURL = FileManager.default.temporaryDirectory.appending(path: UUID().uuidString)
        try FileManager.default.createDirectory(at: temporaryDirectoryURL, withIntermediateDirectories: true)
    }

    deinit {
        try? FileManager.default.removeItem(at: temporaryDirectoryURL)
    }

    @Test
    func migrateHistoriesKeepsLatestHistoryForSameData() throws {
        let realm = try Realm(configuration: realmConfiguration)

        let data = CPYClipData()
        data.types = [.deprecatedString, .deprecatedFilenames, .deprecatedURL, .deprecatedRTF, .deprecatedPDF, .deprecatedTIFF]
        data.fileNames = ["/tmp/file.txt", "/tmp/file2.txt"]
        data.URLs = ["https://clipy-app.com"]
        data.stringValue = "String"
        data.RTFData = Data("rtf".utf8)
        data.PDF = Data("pdf".utf8)
        data.image = NSImage.create(with: .red, size: NSSize(width: 10, height: 10))

        let firstURL = temporaryDirectoryURL.appending(path: UUID().uuidString)
        let secondURL = temporaryDirectoryURL.appending(path: UUID().uuidString)
        NSKeyedArchiver.archiveRootObject(data, toFile: firstURL.path())
        NSKeyedArchiver.archiveRootObject(data, toFile: secondURL.path())

        let firstClip = CPYClip()
        firstClip.dataPath = firstURL.path()
        firstClip.title = "String"
        firstClip.dataHash = "first"
        firstClip.updateTime = 1

        let secondClip = CPYClip()
        secondClip.dataPath = secondURL.path()
        secondClip.title = "String"
        secondClip.dataHash = "second"
        secondClip.updateTime = 2

        try realm.write {
            realm.add(firstClip)
            realm.add(secondClip)
        }

        migration.migrateFromRealmToSQLiteData()

        try database.read { database in
            let histories = try PasteboardHistory.all.fetchAll(database)
            let assets = try PasteboardHistoryAsset.all
                .order(by: \.index)
                .fetchAll(database)

            let content = try #require(data.toPasteboardContent())
            let id = PasteboardHistory.ID(rawValue: content.hash)
            #expect(histories.count == 1)
            #expect(histories.first?.id == id)
            #expect(histories.first?.title == "String")
            #expect(histories.first?.pasteboardTypes == [.string, .deprecatedFilenames, .URL, .rtf, .pdf, .tiff])
            #expect(histories.first?.updateAt == 2)
            #expect(histories.first?.deviceID == CPYUtilities.deviceID)

            #expect(assets.count == 6)
            #expect(assets.map(\.pasteboardHistoryID).allSatisfy { $0 == id })
            #expect(assets.map(\.pasteboardType) == [.string, .deprecatedFilenames, .URL, .rtf, .pdf, .tiff])
        }
    }

    @Test
    func migrateSnippetsPreservesFolderAndSnippetData() throws {
        let realm = try Realm(configuration: realmConfiguration)

        let folderID = UUID()
        let snippetID = UUID()

        let folder = CPYFolder()
        folder.identifier = folderID.uuidString
        folder.title = "Folder"
        folder.index = 2
        folder.enable = false

        let snippet = CPYSnippet()
        snippet.identifier = snippetID.uuidString
        snippet.title = "Snippet"
        snippet.content = "Content"
        snippet.index = 1
        snippet.enable = false
        folder.snippets.append(snippet)

        try realm.write {
            realm.add(folder)
        }

        migration.migrateFromRealmToSQLiteData()

        try database.read { database in
            let folders = try SnippetFolder.all.fetchAll(database)
            let snippets = try Snippet.all.fetchAll(database)
            #expect(
                folders == [
                    SnippetFolder(
                        id: SnippetFolder.ID(rawValue: folderID),
                        title: "Folder",
                        index: 2,
                        isEnabled: false
                    )
                ]
            )
            #expect(
                snippets == [
                    Snippet(
                        id: Snippet.ID(rawValue: snippetID),
                        folderID: SnippetFolder.ID(rawValue: folderID),
                        title: "Snippet",
                        content: "Content",
                        index: 1,
                        isEnabled: false
                    )
                ]
            )
        }
    }
}

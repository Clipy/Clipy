//
//  PasteboardHistoryRepositoryTests.swift
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
struct PasteboardHistoryRepositoryTests {
    let repository: PasteboardHistoryRepository

    init() {
        self.repository = PasteboardHistoryRepository()
    }

    @Test(.timeLimit(.minutes(1)))
    func observeHistories() async throws {
        var histories = [[PasteboardHistory]]()
        let cancellable = repository.observeHistories().sink { value in
            histories.append(value)
        }
        defer { _ = cancellable }

        try await waitUntil { histories.count >= 1 }

        let content = try #require(PasteboardContent("First"))
        let id = PasteboardHistory.ID(rawValue: content.hash)
        repository.save(id: id, content: content, updateAt: 1)
        try await waitUntil { histories.count >= 2 }

        let content2 = try #require(PasteboardContent("Second"))
        let id2 = PasteboardHistory.ID(rawValue: content2.hash)
        repository.save(id: id2, content: content2, updateAt: 2)
        try await waitUntil { histories.count >= 3 }

        repository.deleteHistory(id: id)
        try await waitUntil { histories.count >= 4 }

        #expect(
            histories == [
                [],
                [PasteboardHistory(id: id, title: "First", updateAt: 1)],
                [PasteboardHistory(id: id2, title: "Second", updateAt: 2), PasteboardHistory(id: id, title: "First", updateAt: 1)],
                [PasteboardHistory(id: id2, title: "Second", updateAt: 2)]
            ]
        )
    }

    @Test
    func saveAndFetchHistory() throws {
        #expect(!repository.hasHistories())

        let content = try #require(PasteboardContent("Hello"))
        let id = PasteboardHistory.ID(rawValue: content.hash)
        let history = PasteboardHistory(id: id, title: "Hello", updateAt: 1)

        repository.save(id: id, content: content, updateAt: 1)

        #expect(repository.hasHistories())
        #expect(repository.fetchHistory(id: id) == history)
        #expect(repository.fetchContent(id: id) == content)
        #expect(
            repository.fetchHistoryDetails(sortsByCreatedAt: false, includesThumbnailAsset: false, limit: 10) == [
                PasteboardHistoryDetail(history: history, thumbnailAsset: nil)
            ]
        )
    }

    @Test
    func fetchContentReturnsNilForMissingHistory() {
        #expect(repository.fetchContent(id: PasteboardHistory.ID(rawValue: "missing")) == nil)
    }

    @Test
    func fetchContentPreservesAssetOrder() throws {
        let content = try #require(
            PasteboardContent(
                assets: [
                    PasteboardContent.Asset(type: .fileURL, data: Data("file1".utf8)),
                    PasteboardContent.Asset(type: .string, data: Data("Hello".utf8)),
                    PasteboardContent.Asset(type: .fileURL, data: Data("file2".utf8))
                ]
            )
        )
        let id = PasteboardHistory.ID(rawValue: content.hash)

        repository.save(id: id, content: content, updateAt: 1)

        #expect(repository.fetchContent(id: id) == content)
    }

    @Test
    func fetchHistoryDetailsOrdersAndLimitsHistories() throws {
        let content = try #require(PasteboardContent("First"))
        let content2 = try #require(PasteboardContent("Second"))
        let content3 = try #require(PasteboardContent("Third"))
        let id = PasteboardHistory.ID(rawValue: content.hash)
        let id2 = PasteboardHistory.ID(rawValue: content2.hash)
        let id3 = PasteboardHistory.ID(rawValue: content3.hash)

        repository.save(id: id, content: content, updateAt: 1)
        repository.save(id: id2, content: content2, updateAt: 2)
        repository.save(id: id3, content: content3, updateAt: 3)

        #expect(
            repository
                .fetchHistoryDetails(sortsByCreatedAt: false, includesThumbnailAsset: false, limit: 2)
                .map(\.history.id) == [id3, id2]
        )
        #expect(
            repository
                .fetchHistoryDetails(sortsByCreatedAt: true, includesThumbnailAsset: false, limit: 2)
                .map(\.history.id) == [id3, id2]
        )
        repository.save(id: id, content: content, updateAt: 4)

        #expect(
            repository
                .fetchHistoryDetails(sortsByCreatedAt: false, includesThumbnailAsset: false, limit: 2)
                .map(\.history.id) == [id, id3]
        )
        #expect(
            repository
                .fetchHistoryDetails(sortsByCreatedAt: true, includesThumbnailAsset: false, limit: 2)
                .map(\.history.id) == [id3, id2]
        )
    }

    @Test
    func fetchHistoryDetailsIncludesThumbnailAssetsOnlyWhenRequested() throws {
        let textContent = try #require(PasteboardContent("Hello"))
        let colorContent = try #require(PasteboardContent("#ff0000"))
        let imageContent = try #require(
            PasteboardContent(image: NSImage.create(with: .blue, size: NSSize(width: 20, height: 20)))
        )
        let textID = PasteboardHistory.ID(rawValue: textContent.hash)
        let colorID = PasteboardHistory.ID(rawValue: colorContent.hash)
        let imageID = PasteboardHistory.ID(rawValue: imageContent.hash)

        repository.save(id: textID, content: textContent, updateAt: 1)
        repository.save(id: colorID, content: colorContent, updateAt: 2)
        repository.save(id: imageID, content: imageContent, updateAt: 3)

        let details = repository.fetchHistoryDetails(
            sortsByCreatedAt: false,
            includesThumbnailAsset: true,
            limit: 10
        )
        #expect(details.map(\.history.id) == [imageID, colorID, textID])
        #expect(details[0].thumbnailAsset?.pasteboardHistoryID == imageID)
        #expect(details[0].thumbnailAsset?.kind == .image)
        #expect(details[0].thumbnailAsset?.data.isEmpty == false)
        #expect(details[1].thumbnailAsset?.pasteboardHistoryID == colorID)
        #expect(details[1].thumbnailAsset?.kind == .colorCode)
        #expect(details[1].thumbnailAsset?.data.isEmpty == false)
        #expect(details[2].thumbnailAsset == nil)

        let detailsWithoutThumbnailAssets = repository.fetchHistoryDetails(
            sortsByCreatedAt: false,
            includesThumbnailAsset: false,
            limit: 10
        )
        #expect(detailsWithoutThumbnailAssets.map(\.history.id) == [imageID, colorID, textID])
        #expect(detailsWithoutThumbnailAssets.allSatisfy { $0.thumbnailAsset == nil })
    }

    @Test
    func searchHistoryDetailsMatchesContinuousFragments() throws {
        let continuousContent = try #require(PasteboardContent("Alpha beta gamma"))
        let separatedContent = try #require(PasteboardContent("Alpha beta then gamma"))
        let newestContent = try #require(PasteboardContent("Prefix BETA GAMMA suffix"))
        let continuousID = PasteboardHistory.ID(rawValue: continuousContent.hash)
        let separatedID = PasteboardHistory.ID(rawValue: separatedContent.hash)
        let newestID = PasteboardHistory.ID(rawValue: newestContent.hash)

        repository.save(id: continuousID, content: continuousContent, updateAt: 1)
        repository.save(id: separatedID, content: separatedContent, updateAt: 2)
        repository.save(id: newestID, content: newestContent, updateAt: 3)

        #expect(
            repository
                .searchHistoryDetails(containing: "beta gamma", includesThumbnailAsset: false, limit: 10)
                .map(\.history.id) == [newestID, continuousID]
        )
    }

    @Test
    func searchHistoryDetailsHandlesShortAndQuotedFragments() throws {
        let shortContent = try #require(PasteboardContent("Find XY here"))
        let quotedContent = try #require(PasteboardContent("A \"quoted fragment\" here"))
        let chineseContent = try #require(PasteboardContent("这里有一段连续片段内容"))
        let shortID = PasteboardHistory.ID(rawValue: shortContent.hash)
        let quotedID = PasteboardHistory.ID(rawValue: quotedContent.hash)
        let chineseID = PasteboardHistory.ID(rawValue: chineseContent.hash)

        repository.save(id: shortID, content: shortContent, updateAt: 1)
        repository.save(id: quotedID, content: quotedContent, updateAt: 2)
        repository.save(id: chineseID, content: chineseContent, updateAt: 3)

        #expect(
            repository
                .searchHistoryDetails(containing: "xy", includesThumbnailAsset: false, limit: 10)
                .map(\.history.id) == [shortID]
        )
        #expect(
            repository
                .searchHistoryDetails(containing: "\"quoted", includesThumbnailAsset: false, limit: 10)
                .map(\.history.id) == [quotedID]
        )
        #expect(
            repository
                .searchHistoryDetails(containing: "连续片段", includesThumbnailAsset: false, limit: 10)
                .map(\.history.id) == [chineseID]
        )
        #expect(repository.searchHistoryDetails(containing: "   ", includesThumbnailAsset: false, limit: 10).isEmpty)
    }

    @Test
    func searchHistoryDetailsIncludesOCRTextAndHonorsLimit() throws {
        let oldestContent = try #require(PasteboardContent("needle oldest"))
        let newestContent = try #require(PasteboardContent("needle newest"))
        let imageContent = try #require(
            PasteboardContent(image: NSImage.create(with: .blue, size: NSSize(width: 20, height: 20)))
        )
        let oldestID = PasteboardHistory.ID(rawValue: oldestContent.hash)
        let newestID = PasteboardHistory.ID(rawValue: newestContent.hash)
        let imageID = PasteboardHistory.ID(rawValue: imageContent.hash)

        repository.save(id: oldestID, content: oldestContent, updateAt: 1)
        repository.save(id: imageID, content: imageContent, updateAt: 2)
        repository.updateOCRText(id: imageID, ocrText: "needle recognized in image")
        repository.save(id: newestID, content: newestContent, updateAt: 3)

        let results = repository.searchHistoryDetails(
            containing: "needle",
            includesThumbnailAsset: true,
            limit: 2
        )
        #expect(results.map(\.history.id) == [newestID, imageID])
        #expect(results[0].thumbnailAsset == nil)
        #expect(results[1].thumbnailAsset?.pasteboardHistoryID == imageID)
    }

    @Test
    func updateOCRTextStoresRecognizedText() throws {
        let imageContent = try #require(
            PasteboardContent(image: NSImage.create(with: .blue, size: NSSize(width: 20, height: 20)))
        )
        let id = PasteboardHistory.ID(rawValue: imageContent.hash)
        repository.save(id: id, content: imageContent, updateAt: 1)

        repository.updateOCRText(id: id, ocrText: "recognized text")

        let history = try #require(repository.fetchHistory(id: id))
        #expect(history.ocrText == "recognized text")
    }

    @Test
    func saveExistingHistoryPreservesOCRText() throws {
        let imageContent = try #require(
            PasteboardContent(image: NSImage.create(with: .blue, size: NSSize(width: 20, height: 20)))
        )
        let id = PasteboardHistory.ID(rawValue: imageContent.hash)
        repository.save(id: id, content: imageContent, updateAt: 1)
        repository.updateOCRText(id: id, ocrText: "recognized text")

        repository.save(id: id, content: imageContent, updateAt: 2)

        let history = try #require(repository.fetchHistory(id: id))
        #expect(history.updateAt == 2)
        #expect(history.ocrText == "recognized text")
    }

    @Test
    func saveExistingHistoryUpdatesStoredHistory() throws {
        let content = try #require(PasteboardContent("Same"))
        let id = PasteboardHistory.ID(rawValue: content.hash)

        repository.save(id: id, content: content, updateAt: 1)
        repository.save(id: id, content: content, updateAt: 2)

        #expect(
            repository.fetchHistory(id: id) == PasteboardHistory(
                id: id,
                title: "Same",
                createdAt: 1,
                updateAt: 2
            )
        )
        #expect(
            repository.fetchHistoryDetails(sortsByCreatedAt: false, includesThumbnailAsset: false, limit: 10).map(\.history.id) == [id]
        )
    }

    @Test
    func deleteHistory() throws {
        let content = try #require(PasteboardContent("Hello"))
        let id = PasteboardHistory.ID(rawValue: content.hash)

        repository.save(id: id, content: content, updateAt: 1)
        #expect(repository.fetchHistory(id: id) != nil)

        repository.deleteHistory(id: id)
        #expect(repository.fetchHistory(id: id) == nil)
    }

    @Test
    func deleteAll() throws {
        let content = try #require(PasteboardContent("First"))
        let content2 = try #require(PasteboardContent("Second"))
        let id = PasteboardHistory.ID(rawValue: content.hash)
        let id2 = PasteboardHistory.ID(rawValue: content2.hash)

        repository.save(id: id, content: content, updateAt: 1)
        repository.save(id: id2, content: content2, updateAt: 2)
        #expect(repository.hasHistories())

        repository.deleteAll()

        #expect(!repository.hasHistories())
    }

    @Test
    func deleteOverflowingHistoriesUsesSelectedSortOrder() throws {
        let content = try #require(PasteboardContent("First"))
        let content2 = try #require(PasteboardContent("Second"))
        let content3 = try #require(PasteboardContent("Third"))
        let id = PasteboardHistory.ID(rawValue: content.hash)
        let id2 = PasteboardHistory.ID(rawValue: content2.hash)
        let id3 = PasteboardHistory.ID(rawValue: content3.hash)

        repository.save(id: id, content: content, updateAt: 1)
        repository.save(id: id2, content: content2, updateAt: 2)
        repository.save(id: id3, content: content3, updateAt: 3)
        repository.save(id: id, content: content, updateAt: 4)

        repository.deleteOverflowingHistories(sortsByCreatedAt: false, maxHistorySize: 2)
        #expect(
            repository
                .fetchHistoryDetails(sortsByCreatedAt: false, includesThumbnailAsset: false, limit: 10)
                .map(\.history.id) == [id, id3]
        )
        #expect(repository.fetchHistory(id: id2) == nil)

        repository.deleteAll()
        repository.save(id: id, content: content, updateAt: 1)
        repository.save(id: id2, content: content2, updateAt: 2)
        repository.save(id: id3, content: content3, updateAt: 3)
        repository.save(id: id, content: content, updateAt: 4)

        repository.deleteOverflowingHistories(sortsByCreatedAt: true, maxHistorySize: 2)
        #expect(
            repository
                .fetchHistoryDetails(sortsByCreatedAt: true, includesThumbnailAsset: false, limit: 10)
                .map(\.history.id) == [id3, id2]
        )
        #expect(repository.fetchHistory(id: id) == nil)

        repository.deleteOverflowingHistories(sortsByCreatedAt: true, maxHistorySize: 0)
        #expect(!repository.hasHistories())
    }
}

private extension PasteboardContent {
    init?(_ string: String) {
        guard let data = string.data(using: .utf8) else {
            return nil
        }
        guard let content = PasteboardContent(assets: [PasteboardContent.Asset(type: .string, data: data)]) else {
            return nil
        }
        self = content
    }
}

private extension PasteboardHistory {
    init(id: PasteboardHistory.ID, title: String, createdAt: Int? = nil, updateAt: Int, ocrText: String? = nil) {
        @Dependency(\.deviceIdentifier) var deviceIdentifier

        self.init(
            id: id,
            title: title,
            ocrText: ocrText,
            pasteboardTypes: [.string],
            createdAt: createdAt ?? updateAt,
            updateAt: updateAt,
            deviceID: deviceIdentifier
        )
    }
}

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

    @Test
    func searchHistoriesWithFTS5Query() throws {
        let textContent = try #require(PasteboardContent("xqa apple item"))
        let imageContent = try #require(
            PasteboardContent(image: NSImage.create(with: .blue, size: NSSize(width: 20, height: 20)))
        )
        let textID = PasteboardHistory.ID(rawValue: textContent.hash)
        let imageID = PasteboardHistory.ID(rawValue: imageContent.hash)

        repository.save(id: textID, content: textContent, updateAt: 1)
        repository.save(id: imageID, content: imageContent, updateAt: 2)
        repository.updateOCRText(id: imageID, ocrText: "xqa ocr recognized text")

        // Match by title
        let titleMatches = repository.searchHistories(matching: "xqa apple", includesThumbnailAsset: false, limit: 10)
        #expect(titleMatches.map(\.history.id).contains(textID))

        // Match by OCR text
        let ocrMatches = repository.searchHistories(matching: "ocr recognized", includesThumbnailAsset: true, limit: 10)
        #expect(ocrMatches.map(\.history.id) == [imageID])
        #expect(ocrMatches.first?.thumbnailAsset?.pasteboardHistoryID == imageID)
        #expect(ocrMatches.first?.thumbnailAsset?.kind == .image)

        // Non-matching query
        let noMatches = repository.searchHistories(matching: "zztop", includesThumbnailAsset: false, limit: 10)
        #expect(noMatches.isEmpty)
    }

    @Test
    func searchHistoriesWithShortQueryFallsBackToLIKE() throws {
        let content1 = try #require(PasteboardContent("ab first item"))
        let content2 = try #require(PasteboardContent("ac second item"))
        let content3 = try #require(PasteboardContent("xy third item"))
        let id1 = PasteboardHistory.ID(rawValue: content1.hash)
        let id2 = PasteboardHistory.ID(rawValue: content2.hash)
        let id3 = PasteboardHistory.ID(rawValue: content3.hash)

        repository.save(id: id1, content: content1, updateAt: 1)
        repository.save(id: id2, content: content2, updateAt: 2)
        repository.save(id: id3, content: content3, updateAt: 3)

        // 2 characters (below trigram length 3)
        let matches2Chars = repository.searchHistories(matching: "ab", includesThumbnailAsset: false, limit: 10)
        #expect(matches2Chars.map(\.history.id) == [id1])

        // 1 character
        let matches1Char = repository.searchHistories(matching: "a", includesThumbnailAsset: false, limit: 10)
        #expect(matches1Char.map(\.history.id) == [id2, id1])
    }

    @Test
    func searchHistoriesWithSpecialCharacters() throws {
        let content = try #require(PasteboardContent("test \"quoted\" (special) -hyphen *star:colon"))
        let id = PasteboardHistory.ID(rawValue: content.hash)
        repository.save(id: id, content: content, updateAt: 1)

        // Queries with FTS5 special operators/characters must not throw syntax errors
        #expect(!repository.searchHistories(matching: "\"quoted\"", includesThumbnailAsset: false, limit: 10).isEmpty)
        #expect(!repository.searchHistories(matching: "(special)", includesThumbnailAsset: false, limit: 10).isEmpty)
        #expect(!repository.searchHistories(matching: "-hyphen", includesThumbnailAsset: false, limit: 10).isEmpty)
        #expect(!repository.searchHistories(matching: "*star", includesThumbnailAsset: false, limit: 10).isEmpty)
        #expect(repository.searchHistories(matching: "\"\"\"\"\"", includesThumbnailAsset: false, limit: 10).isEmpty)
        #expect(repository.searchHistories(matching: "AND OR NOT", includesThumbnailAsset: false, limit: 10).isEmpty)
    }

    @Test
    func searchHistoriesWithFTS5QueryOrdersByRecencyBeforeLimit() throws {
        let content1 = try #require(PasteboardContent("shared token item 1"))
        let content2 = try #require(PasteboardContent("shared token item 2"))
        let content3 = try #require(PasteboardContent("shared token item 3"))
        let content4 = try #require(PasteboardContent("shared token item 4"))
        let content5 = try #require(PasteboardContent("shared token item 5"))

        let id1 = PasteboardHistory.ID(rawValue: content1.hash)
        let id2 = PasteboardHistory.ID(rawValue: content2.hash)
        let id3 = PasteboardHistory.ID(rawValue: content3.hash)
        let id4 = PasteboardHistory.ID(rawValue: content4.hash)
        let id5 = PasteboardHistory.ID(rawValue: content5.hash)

        repository.save(id: id1, content: content1, updateAt: 1)
        repository.save(id: id2, content: content2, updateAt: 2)
        repository.save(id: id3, content: content3, updateAt: 3)
        repository.save(id: id4, content: content4, updateAt: 4)
        repository.save(id: id5, content: content5, updateAt: 5)

        let matches = repository.searchHistories(matching: "shared", includesThumbnailAsset: false, limit: 3)
        #expect(matches.map(\.history.id) == [id5, id4, id3])
    }

    @Test
    func searchHistoriesWithLimit() throws {
        for i in 1...5 {
            let content = try #require(PasteboardContent("search_target item \(i)"))
            let id = PasteboardHistory.ID(rawValue: content.hash)
            repository.save(id: id, content: content, updateAt: i)
        }

        let limitedMatches = repository.searchHistories(matching: "search_target", includesThumbnailAsset: false, limit: 2)
        #expect(limitedMatches.count == 2)
    }

    @Test
    func searchHistoriesWithEmptyOrWhitespaceQuery() throws {
        let content = try #require(PasteboardContent("any item"))
        let id = PasteboardHistory.ID(rawValue: content.hash)
        repository.save(id: id, content: content, updateAt: 1)

        #expect(repository.searchHistories(matching: "", includesThumbnailAsset: false, limit: 10).isEmpty)
        #expect(repository.searchHistories(matching: "   \t\n  ", includesThumbnailAsset: false, limit: 10).isEmpty)
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

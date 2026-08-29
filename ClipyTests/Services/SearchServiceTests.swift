//
//  SearchServiceTests.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/08/25.
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import Dependencies
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
struct SearchServiceTests {
    @Dependency(\.defaultAppStorage)
    var appStorage
    let service: SearchService
    let historyRepository: PasteboardHistoryRepository
    let snippetRepository: SnippetRepository

    init() {
        let historyRepository = PasteboardHistoryRepository()
        let snippetRepository = SnippetRepository()
        self.historyRepository = historyRepository
        self.snippetRepository = snippetRepository
        self.service = withDependencies {
            $0.pasteboardHistoryRepository = historyRepository
            $0.snippetRepository = snippetRepository
        } operation: {
            SearchService()
        }
    }

    @Test
    func searchReturnsSnippetsBeforeHistories() throws {
        let content = try #require(PasteboardContent("search_keyword history item"))
        let historyID = PasteboardHistory.ID(rawValue: content.hash)
        historyRepository.save(id: historyID, content: content, updateAt: 1)

        let folder = try #require(snippetRepository.insertFolder())
        let snippet = try #require(snippetRepository.insertSnippet(to: folder.id))
        snippetRepository.updateSnippetTitle(snippet.id, title: "search_keyword snippet item")

        let results = service.search(query: "search_keyword", limit: 10)

        #expect(results.count == 2)
        guard results.count == 2 else { return }

        switch results[0] {
        case .snippet(let match):
            #expect(match.snippet.id == snippet.id)
        case .history:
            Issue.record("Expected first item to be a snippet")
        }

        switch results[1] {
        case .history(let detail):
            #expect(detail.history.id == historyID)
        case .snippet:
            Issue.record("Expected second item to be a history")
        }
    }

    @Test
    func searchWithEmptyQueryReturnsRecentHistories() throws {
        let content1 = try #require(PasteboardContent("First History"))
        let content2 = try #require(PasteboardContent("Second History"))
        let content3 = try #require(PasteboardContent("Third History"))
        let id1 = PasteboardHistory.ID(rawValue: content1.hash)
        let id2 = PasteboardHistory.ID(rawValue: content2.hash)
        let id3 = PasteboardHistory.ID(rawValue: content3.hash)

        historyRepository.save(id: id1, content: content1, updateAt: 1)
        historyRepository.save(id: id2, content: content2, updateAt: 2)
        historyRepository.save(id: id3, content: content3, updateAt: 3)

        let folder = try #require(snippetRepository.insertFolder())
        _ = snippetRepository.insertSnippet(to: folder.id)

        // Empty query should return recent histories only (no snippets)
        let emptyResults = service.search(query: "", limit: 10)
        #expect(emptyResults.count == 3)
        #expect(emptyResults.map(\.id) == ["history:\(id3.rawValue)", "history:\(id2.rawValue)", "history:\(id1.rawValue)"])

        // Whitespace-only query should also return recent histories
        let whitespaceResults = service.search(query: "   \t\n  ", limit: 10)
        #expect(whitespaceResults.count == 3)
    }

    @Test
    func searchResultItemProperties() throws {
        let content = try #require(PasteboardContent("History Title"))
        let historyID = PasteboardHistory.ID(rawValue: content.hash)
        historyRepository.save(id: historyID, content: content, updateAt: 1)

        let folder = try #require(snippetRepository.insertFolder())
        snippetRepository.updateFolderTitle(folder.id, title: "Snippets Group")
        let snippet = try #require(snippetRepository.insertSnippet(to: folder.id))
        snippetRepository.updateSnippetTitle(snippet.id, title: "Snippet Title")

        let historyDetail = try #require(historyRepository.fetchHistoryDetails(sortsByCreatedAt: false, includesThumbnailAsset: false, limit: 1).first)
        let updatedFolder = try #require(snippetRepository.fetchFolderDetail(id: folder.id)?.folder)
        let updatedSnippet = try #require(snippetRepository.fetchSnippet(id: snippet.id))
        let snippetMatch = SnippetSearchMatch(snippet: updatedSnippet, folder: updatedFolder)

        let historyItem = SearchResultItem.history(historyDetail)
        #expect(historyItem.id == "history:\(historyID.rawValue)")
        #expect(historyItem.title == "History Title")
        #expect(!historyItem.subtitle.isEmpty)
        #expect(historyItem.thumbnailImage == nil)

        let snippetItem = SearchResultItem.snippet(snippetMatch)
        #expect(snippetItem.id == "snippet:\(snippet.id.rawValue.uuidString)")
        #expect(snippetItem.title == "Snippet Title")
        #expect(snippetItem.subtitle == "Snippets Group")
        #expect(snippetItem.thumbnailImage == nil)
    }

    @Test
    func searchRespectsLimitAcrossCombinedResults() throws {
        for i in 1...5 {
            let content = try #require(PasteboardContent("combined_query history \(i)"))
            let id = PasteboardHistory.ID(rawValue: content.hash)
            historyRepository.save(id: id, content: content, updateAt: i)
        }

        let folder = try #require(snippetRepository.insertFolder())
        for i in 1...5 {
            let snippet = try #require(snippetRepository.insertSnippet(to: folder.id))
            snippetRepository.updateSnippetTitle(snippet.id, title: "combined_query snippet \(i)")
        }

        let results = service.search(query: "combined_query", limit: 4)
        #expect(results.count == 4)
        // All 4 should be snippets since 5 snippets exist and snippets are returned first
        #expect(results.allSatisfy { item in
            if case .snippet = item { return true }
            return false
        })
    }

    @Test
    func searchIncludesThumbnailAssetsWhenEnabled() throws {
        let imageContent = try #require(
            PasteboardContent(image: NSImage.create(with: .blue, size: NSSize(width: 20, height: 20)))
        )
        let imageID = PasteboardHistory.ID(rawValue: imageContent.hash)
        historyRepository.save(id: imageID, content: imageContent, updateAt: 1)
        historyRepository.updateOCRText(id: imageID, ocrText: "image_search_tag")

        // Without showImageInTheMenu
        appStorage.set(false, forKey: Constants.UserDefaults.showImageInTheMenu)
        appStorage.set(false, forKey: Constants.UserDefaults.showColorPreviewInTheMenu)

        let resultsWithoutThumbnail = service.search(query: "image_search_tag", limit: 10)
        #expect(resultsWithoutThumbnail.count == 1)
        #expect(resultsWithoutThumbnail.first?.thumbnailImage == nil)

        // With showImageInTheMenu enabled
        appStorage.set(true, forKey: Constants.UserDefaults.showImageInTheMenu)

        let resultsWithThumbnail = service.search(query: "image_search_tag", limit: 10)
        #expect(resultsWithThumbnail.count == 1)
        #expect(resultsWithThumbnail.first?.thumbnailImage != nil)

        let colorContent = try #require(
            PasteboardContent(assets: [
                .init(type: .string, data: Data("#ff0000".utf8))
            ])
        )
        let colorID = PasteboardHistory.ID(rawValue: colorContent.hash)
        historyRepository.save(id: colorID, content: colorContent, updateAt: 2)
        historyRepository.updateOCRText(id: colorID, ocrText: "color_search_tag")

        let imageOnlyColorResults = service.search(query: "color_search_tag", limit: 10)
        #expect(imageOnlyColorResults.count == 1)
        #expect(imageOnlyColorResults.first?.thumbnailImage == nil)

        appStorage.set(false, forKey: Constants.UserDefaults.showImageInTheMenu)
        appStorage.set(true, forKey: Constants.UserDefaults.showColorPreviewInTheMenu)

        let colorResults = service.search(query: "color_search_tag", limit: 10)
        #expect(colorResults.count == 1)
        #expect(colorResults.first?.thumbnailImage != nil)
    }

    @Test
    func searchWithEmptyQueryRespectsReorderClipsAfterPasting() throws {
        let content1 = try #require(PasteboardContent("First Item"))
        let content2 = try #require(PasteboardContent("Second Item"))
        let id1 = PasteboardHistory.ID(rawValue: content1.hash)
        let id2 = PasteboardHistory.ID(rawValue: content2.hash)

        // id1 created at 1, updated at 4
        // id2 created at 2, updated at 2
        historyRepository.save(id: id1, content: content1, updateAt: 1)
        historyRepository.save(id: id2, content: content2, updateAt: 2)
        historyRepository.save(id: id1, content: content1, updateAt: 4)

        // reorderClipsAfterPasting = true -> sorts by updateAt desc -> [id1, id2]
        appStorage.set(true, forKey: Constants.UserDefaults.reorderClipsAfterPasting)
        let resultsByUpdate = service.search(query: "", limit: 10)
        #expect(resultsByUpdate.map(\.id) == ["history:\(id1.rawValue)", "history:\(id2.rawValue)"])

        // reorderClipsAfterPasting = false -> sorts by createdAt desc -> [id2, id1]
        appStorage.set(false, forKey: Constants.UserDefaults.reorderClipsAfterPasting)
        let resultsByCreate = service.search(query: "", limit: 10)
        #expect(resultsByCreate.map(\.id) == ["history:\(id2.rawValue)", "history:\(id1.rawValue)"])
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

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
import Sharing
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

@MainActor
@Suite(.serialized, .dependencies { try $0.bootstrapDatabase() })
struct HistoryMenuSearchTests {
    @Test func searchesLiteralTextAndOCRWithBoundedResults() throws {
        let repository = PasteboardHistoryRepository()
        let store = HistoryMenuSearchStore()
        let first = try #require(PasteboardContent("Alpha 100%_value"))
        let second = try #require(PasteboardContent("Alpha other"))
        let firstID = PasteboardHistory.ID(rawValue: first.hash)
        let secondID = PasteboardHistory.ID(rawValue: second.hash)
        repository.save(id: firstID, content: first, updateAt: 1)
        repository.save(id: secondID, content: second, updateAt: 2)
        repository.updateOCRText(id: firstID, ocrText: "Receipt 12345")
        #expect(try store.search(query: "alpha", sortsByCreatedAt: false, limit: 1, sort: .original).map(\.id) == [secondID])
        #expect(try store.search(query: "%_", sortsByCreatedAt: false, limit: 10).map(\.id) == [firstID])
        #expect(try store.search(query: "receipt 123", sortsByCreatedAt: false, limit: 10).map(\.id) == [firstID])
        #expect(try store.search(query: "other receipt", sortsByCreatedAt: false, limit: 10).isEmpty)
        #expect(try store.search(query: "  ", sortsByCreatedAt: false, limit: 10).isEmpty)
    }

    @Test func clearingQueryPreservesOriginalMenuObjects() {
        let menu = NSMenu()
        let search = HistoryMenuSearchView()
        search.owningMenu = menu
        let header = NSMenuItem()
        header.view = search
        let history = NSMenuItem(title: "History", action: nil, keyEquivalent: "")
        let folder = NSMenuItem(title: "1–30", action: nil, keyEquivalent: "")
        folder.submenu = NSMenu()
        let snippets = NSMenuItem(title: "Snippets", action: nil, keyEquivalent: "")
        menu.addItem(header)
        search.installSortControl(in: menu)
        let sort = search.sortItem!
        [history, folder, snippets].forEach(menu.addItem)
        #expect(sort.isHidden)
        search.historyItems = [history, folder]
        for _ in 0..<20 {
            search.searchField.stringValue = "alpha"
            search.controlTextDidChange(Notification(name: NSControl.textDidChangeNotification))
            #expect(!sort.isHidden)
            search.searchField.stringValue = ""
            search.controlTextDidChange(Notification(name: NSControl.textDidChangeNotification))
            #expect(menu.items.map(ObjectIdentifier.init) == [header, sort, history, folder, snippets].map(ObjectIdentifier.init))
            #expect(folder.isEnabled)
            #expect(sort.isHidden)
        }
    }
    @Test func menuFieldEditorShowsInsertionPointInNonKeyWindow() async throws {
        let field = HistoryMenuSearchView().searchField
        let window = NSWindow(contentRect: NSRect(x: 0, y: 0, width: 320, height: 40),
                              styleMask: [.borderless], backing: .buffered, defer: false)
        window.contentView?.addSubview(field)
        #expect(window.makeFirstResponder(field))
        let editor = try #require(field.currentEditor() as? HistoryMenuFieldEditor)
        #expect(!window.isKeyWindow)
        #expect(editor.isFieldEditor)
        try await Task.sleep(for: .milliseconds(100))
        let caret = try #require(editor.subviews.first { $0 is HistoryMenuInsertionPoint })
        #expect(!caret.isHidden)
        #expect(caret.frame.width > 0 && caret.frame.height > 0)
        #expect(editor.isMenuInsertionPointActive)
        editor.string = "Search query"
        editor.setSelectedRange(NSRange(location: 0, length: 6))
        #expect(!editor.isMenuInsertionPointActive)
        try await Task.sleep(for: .milliseconds(50))
        #expect(caret.isHidden)
        editor.setSelectedRange(NSRange(location: 6, length: 0))
        #expect(editor.isMenuInsertionPointActive)
        (field.cell as? HistoryMenuSearchCell)?.endMenuTracking()
        try await Task.sleep(for: .milliseconds(650))
        #expect(caret.isHidden)
        #expect(!editor.isMenuInsertionPointActive)
        window.makeFirstResponder(nil)
        field.stringValue = ""
        #expect(window.makeFirstResponder(field))
        try await Task.sleep(for: .milliseconds(100))
        #expect(editor.isMenuInsertionPointActive)
        #expect(!caret.isHidden)
        window.makeFirstResponder(nil)
        #expect(!editor.isMenuInsertionPointActive)
    }

    @Test func searchRankingAndSortDoNotAlterNormalHistoryPreferences() throws {
        let repository = PasteboardHistoryRepository()
        let store = HistoryMenuSearchStore()
        var ids = [PasteboardHistory.ID]()
        for (index, text) in ["alpha beta", "alpha beta suffix", "before alpha beta", "alpha between beta", "OCR only"].enumerated() {
            let content = try #require(PasteboardContent(text))
            let id = PasteboardHistory.ID(rawValue: content.hash)
            repository.save(id: id, content: content, updateAt: index + 1)
            ids.append(id)
        }
        repository.updateOCRText(id: ids[4], ocrText: "alpha beta")
        #expect(try store.search(query: "alpha beta", sortsByCreatedAt: false, limit: 31).map(\.id) == ids)
        #expect(try store.search(query: "alpha absent", sortsByCreatedAt: false, limit: 31).isEmpty)
        for sort in HistoryMenuSearchStore.Sort.allCases {
            let complete = try store.search(query: "alpha beta", sortsByCreatedAt: false, limit: 100, sort: sort)
            #expect(try store.search(query: "alpha beta", sortsByCreatedAt: false, limit: 2, sort: sort).map(\.id) == complete.prefix(2).map(\.id))
        }
        let oldContent = try #require(PasteboardContent("alpha beta"))
        repository.save(id: ids[0], content: oldContent, updateAt: 99)
        #expect(try store.search(query: "alpha beta", sortsByCreatedAt: false, limit: 1, sort: .original).first?.id == ids[0])
        #expect(try store.search(query: "alpha beta", sortsByCreatedAt: false, limit: 1, sort: .newest).first?.id == ids[4])
        #expect(try store.search(query: "alpha beta", sortsByCreatedAt: false, limit: 1, sort: .oldest).first?.id == ids[0])

        @Shared(.historySearchSort) var searchSort
        @Shared(.reordersClipsAfterPasting) var reorder
        let previousSort = searchSort
        let previousReorder = reorder
        defer { $searchSort.withLock { $0 = previousSort } }
        let historyBefore = repository.fetchHistoryDetails(sortsByCreatedAt: !reorder, includesThumbnailAsset: false, limit: 100).map(\.history.id)
        let menu = NSMenu()
        let view = HistoryMenuSearchView()
        view.installSortControl(in: menu)
        for item in view.sortItem?.submenu?.items ?? [] {
            let action = try #require(item.action)
            #expect(NSApp.sendAction(action, to: item.target, from: item))
            #expect(searchSort.rawValue == item.representedObject as? String)
            #expect(reorder == previousReorder)
            #expect(repository.fetchHistoryDetails(sortsByCreatedAt: !reorder, includesThumbnailAsset: false, limit: 100).map(\.history.id) == historyBefore)
        }
    }

}

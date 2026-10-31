import AppKit
import Dependencies
import DependenciesTestSupport
import Sharing
import Testing
@testable import Clipy

@MainActor
@Suite(.dependencies { try $0.bootstrapDatabase() })
struct ColorCodePreviewTests {
    private let repository = PasteboardHistoryRepository()

    @Test(arguments: [false, true], [false, true])
    func previewsKeepCopyTimeSettings(bareHex: Bool, cssNames: Bool) throws {
        @Shared(.inlineMenuItemLimit) var inlineMenuItemLimit = 100
        @Shared(.parsesColorCodesWithoutHash) var parsesColorCodesWithoutHash = bareHex
        @Shared(.parsesCSSColorNames) var parsesCSSColorNames = cssNames

        let standard = ["#f00", "rgb(255 0 0)", "hsl(0 100% 50%)"]
        let bareCodes = ["f00", "FF000080", "123456"]
        let namedColors = ["red", "tomato", "transparent"]
        let codes = standard + bareCodes + namedColors + ["not a color"]
        let contents = try codes.map { try content($0) }
        for content in contents {
            repository.save(id: .init(rawValue: content.hash), content: content, updateAt: 1)
        }
        let savedThumbnails = thumbnailAssets()

        $parsesColorCodesWithoutHash.withLock { $0 = !bareHex }
        $parsesCSSColorNames.withLock { $0 = !cssNames }
        // Copying an existing entry again must also preserve its saved preview.
        for content in contents {
            repository.save(id: .init(rawValue: content.hash), content: content, updateAt: 2)
        }
        #expect(thumbnailAssets() == savedThumbnails)

        let manager = makeMenuManager()
        let menu = manager.makeMainMenu(details: [])
        for (code, content) in zip(codes, contents) {
            let expected = standard.contains(code) || (bareHex && bareCodes.contains(code)) || (cssNames && namedColors.contains(code))
            let image = try menuImage(in: menu, id: .init(rawValue: content.hash))
            #expect(!image.isTemplate == expected, "Saved preview changed for \(code)")
        }

        // New clipboard entries use the updated options when their previews are saved.
        let newContents = try codes.map { try content(" \($0) ") }
        for content in newContents {
            repository.save(id: .init(rawValue: content.hash), content: content, updateAt: 3)
        }
        let updatedMenu = manager.makeMainMenu(details: [])
        for (code, content) in zip(codes, newContents) {
            let expected = standard.contains(code) || (!bareHex && bareCodes.contains(code)) || (!cssNames && namedColors.contains(code))
            let image = try menuImage(in: updatedMenu, id: .init(rawValue: content.hash))
            #expect(!image.isTemplate == expected, "Unexpected new preview for \(code)")
        }
    }

    @Test
    func previewVisibilityDoesNotChangeSavedImages() throws {
        @Shared(.inlineMenuItemLimit) var inlineMenuItemLimit = 100
        @Shared(.showsColorPreviewInMenu) var showsColorPreviewInMenu = true
        @Shared(.parsesColorCodesWithoutHash) var parsesColorCodesWithoutHash = true
        @Shared(.parsesCSSColorNames) var parsesCSSColorNames = true
        let contents = try ["#f00", "ff0000", "tomato"].map { try content($0) }
        for content in contents {
            repository.save(id: .init(rawValue: content.hash), content: content, updateAt: 1)
        }
        let savedThumbnails = thumbnailAssets()
        let manager = makeMenuManager()
        for showsPreview in [true, false, true] {
            $showsColorPreviewInMenu.withLock { $0 = showsPreview }
            let menu = manager.makeMainMenu(details: [])
            for content in contents {
                let image = try menuImage(in: menu, id: .init(rawValue: content.hash))
                #expect(!image.isTemplate == showsPreview)
            }
        }
        #expect(thumbnailAssets() == savedThumbnails)
    }

    @Test
    func savedPreviewUsesOriginalTextInsteadOfTruncatedHistoryTitle() throws {
        @Shared(.inlineMenuItemLimit) var inlineMenuItemLimit = 100
        @Shared(.parsesColorCodesWithoutHash) var parsesColorCodesWithoutHash = true
        let padding = String(repeating: " ", count: 10000)
        let invalid = try content("ff0000" + padding + "not a color")
        let valid = try content(padding + "ff0000")
        for content in [invalid, valid] {
            repository.save(id: .init(rawValue: content.hash), content: content, updateAt: 1)
        }
        let menu = makeMenuManager().makeMainMenu(details: [])
        #expect(try menuImage(in: menu, id: .init(rawValue: invalid.hash)).isTemplate)
        #expect(try !menuImage(in: menu, id: .init(rawValue: valid.hash)).isTemplate)
    }

    private func thumbnailAssets() -> [PasteboardHistory.ID: PasteboardHistoryThumbnailAsset?] {
        Dictionary(uniqueKeysWithValues: repository.fetchHistoryDetails(
            sortsByCreatedAt: false,
            includesThumbnailAsset: true,
            limit: 100
        ).map { ($0.history.id, $0.thumbnailAsset) })
    }

    private func content(_ string: String) throws -> PasteboardContent {
        try #require(PasteboardContent(assets: [.init(type: .string, data: Data(string.utf8))]))
    }

    private func makeMenuManager() -> MenuManager {
        withDependencies {
            $0.pasteboardHistoryRepository = repository
        } operation: {
            MenuManager()
        }
    }

    private func menuImage(in menu: NSMenu, id: PasteboardHistory.ID) throws -> NSImage {
        let item = try #require(menu.items.first { ($0.representedObject as? PasteboardHistory.ID) == id })
        return try #require(item.image)
    }
}

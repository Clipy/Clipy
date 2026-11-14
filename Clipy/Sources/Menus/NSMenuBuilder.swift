//
//  NSMenuBuilder.swift
//  Clipy
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import Sharing

/// Builds native menus and their items.
@MainActor
final class NSMenuBuilder {
    private var symbolImages: [String: NSImage] = [:]
    private let maximumKeyEquivalents = 10

    @Shared(.showsClearHistoryMenuItem)
    private var showsClearHistoryMenuItem
    @Shared(.showsSnippetsAboveHistory)
    private var showsSnippetsAboveHistory
    @Shared(.inlineMenuItemLimit)
    private var inlineMenuItemLimit
    @Shared(.folderMenuItemLimit)
    private var folderMenuItemLimit
    @Shared(.startsMenuItemTitlesAtZero)
    private var startsMenuItemTitlesAtZero
    @Shared(.marksMenuItemsWithNumbers)
    private var marksMenuItemsWithNumbers
    @Shared(.showsImagesInMenu)
    private var showsImagesInMenu
    @Shared(.addsNumericKeyEquivalents)
    private var addsNumericKeyEquivalents
    @Shared(.showsColorPreviewInMenu)
    private var showsColorPreviewInMenu
    @Shared(.thumbnailWidth)
    private var thumbnailWidth
    @Shared(.thumbnailHeight)
    private var thumbnailHeight

    var firstItemNumber: Int { startsMenuItemTitlesAtZero ? 0 : 1 }

    nonisolated init() {}
}

extension NSMenuBuilder {
    func makeMainMenu(histories: [PasteboardHistoryDetail], folders: [SnippetFolderDetail]) -> NSMenu {
        let menu = NSMenu(title: Bundle.main.appName ?? "")
        menu.autoenablesItems = false

        if showsSnippetsAboveHistory {
            addSnippetItems(menu, details: folders)
            menu.addItem(.separator())
        }
        addHistoryItems(menu, details: histories)
        if !showsSnippetsAboveHistory {
            menu.addItem(.separator())
            addSnippetItems(menu, details: folders)
        }

        menu.addItem(.separator())
        if showsClearHistoryMenuItem {
            let item = NSMenuItem(title: String(localized: "Clear History"), action: #selector(AppDelegate.clearAllHistory))
            item.isEnabled = !histories.isEmpty
            menu.addItem(item)
        }
        menu.addItem(NSMenuItem(title: String(localized: "Edit Snippets"), action: #selector(AppDelegate.showSnippetEditorWindow)))
        menu.addItem(NSMenuItem(title: String(localized: "Settings…"), action: #selector(AppDelegate.showSettingsWindow), keyEquivalent: ","))
        menu.addItem(.separator())
        menu.addItem(NSMenuItem(title: String(localized: "Quit Clipy"), action: #selector(AppDelegate.terminate)))
        return menu
    }

    func makeHistoryMenu(details: [PasteboardHistoryDetail]) -> NSMenu {
        let menu = NSMenu(title: MenuType.history.rawValue)
        addHistoryItems(menu, details: details)
        return menu
    }

    func makeSnippetMenu(details: [SnippetFolderDetail]) -> NSMenu {
        let menu = NSMenu(title: MenuType.snippet.rawValue)
        addSnippetItems(menu, details: details)
        return menu
    }

    func makeSnippetFolderMenu(detail: SnippetFolderDetail?) -> NSMenu? {
        guard let detail, detail.folder.isEnabled else { return nil }

        let menu = NSMenu(title: detail.folder.title)
        menu.addItem(.sectionHeader(title: detail.folder.title))
        for (index, snippet) in detail.snippets.lazy.filter(\.isEnabled).enumerated() {
            menu.addItem(makeSnippetMenuItem(snippet, listNumber: firstItemNumber + index, font: menu.font))
        }
        return menu
    }
}

extension NSMenuBuilder {
    func makeClipMenuItem(_ historyDetail: PasteboardHistoryDetail, index: Int, listNumber: Int, font: NSFont) -> NSMenuItem {
        let history = historyDetail.history
        return NSMenuItem(
            attributedTitle: numberedMenuTitle(history.menuTitle, listNumber: listNumber),
            action: #selector(AppDelegate.selectClipMenuItem(_:)),
            representedObject: history.id,
            keyEquivalent: historyKeyEquivalent(at: index),
            image: thumbnailImage(historyDetail.thumbnailAsset) ?? symbolImage(history.primaryAvailableType?.symbolName, font: font),
            toolTip: history.toolTip
        )
    }

    func makeSnippetMenuItem(_ snippet: Snippet, listNumber: Int, font: NSFont) -> NSMenuItem {
        NSMenuItem(
            attributedTitle: numberedMenuTitle(snippet.title.trimmedMenuTitle, listNumber: listNumber),
            action: #selector(AppDelegate.selectSnippetMenuItem(_:)),
            representedObject: snippet.id,
            image: symbolImage("text.quote", font: font),
            toolTip: snippet.toolTip
        )
    }
}

private extension NSMenuBuilder {
    func makeSubmenuItem(_ title: String, font: NSFont) -> NSMenuItem {
        let subMenu = NSMenu(title: "")
        let subMenuItem = NSMenuItem(title: title, action: nil)
        subMenuItem.submenu = subMenu
        subMenuItem.image = symbolImage("folder", font: font)
        subMenuItem.preferVisibleImage()
        return subMenuItem
    }

    func addHistoryItems(_ menu: NSMenu, details historyDetails: [PasteboardHistoryDetail]) {
        menu.addItem(.sectionHeader(title: String(localized: "History")))

        let firstIndex = firstItemNumber
        let inlineCount = min(max(inlineMenuItemLimit, 0), historyDetails.count)
        for (index, detail) in historyDetails.prefix(inlineCount).enumerated() {
            menu.addItem(makeClipMenuItem(detail, index: index, listNumber: firstIndex + index, font: menu.font))
        }
        let folderSize = max(folderMenuItemLimit, 1)
        for offset in stride(from: inlineCount, to: historyDetails.count, by: folderSize) {
            let end = offset + min(folderSize, historyDetails.count - offset)
            let title = MonospacedDigitFormatter.rangeTitle(
                firstNumber: firstIndex + offset, lastNumber: firstIndex + end - 1
            )
            let folder = makeSubmenuItem(title.string, font: menu.font)
            folder.attributedTitle = title
            if let submenu = folder.submenu {
                for index in offset..<end {
                    submenu.addItem(
                        makeClipMenuItem(
                            historyDetails[index], index: index, listNumber: firstIndex + index - offset, font: submenu.font
                        )
                    )
                }
            }
            menu.addItem(folder)
        }
    }

    func addSnippetItems(_ menu: NSMenu, details: [SnippetFolderDetail]) {
        guard !details.isEmpty else { return }
        menu.addItem(.sectionHeader(title: String(localized: "Snippet")))

        let firstIndex = firstItemNumber
        for detail in details where detail.folder.isEnabled {
            let folder = makeSubmenuItem(detail.folder.title, font: menu.font)
            if let submenu = folder.submenu {
                for (index, snippet) in detail.snippets.lazy.filter(\.isEnabled).enumerated() {
                    submenu.addItem(makeSnippetMenuItem(snippet, listNumber: firstIndex + index, font: submenu.font))
                }
            }
            menu.addItem(folder)
        }
    }

    func historyKeyEquivalent(at index: Int) -> String {
        guard addsNumericKeyEquivalents, index < maximumKeyEquivalents else { return "" }
        return String((index + firstItemNumber) % maximumKeyEquivalents)
    }

    func thumbnailImage(_ asset: PasteboardHistoryThumbnailAsset?) -> NSImage? {
        guard let asset else { return nil }
        switch asset.kind {
        case .image where showsImagesInMenu:
            return NSImage(data: asset.data)?.aspectFitImage(CGFloat(thumbnailWidth), CGFloat(thumbnailHeight))
        case .colorCode where showsColorPreviewInMenu:
            let previewSize: CGFloat = 18
            return NSImage(data: asset.data)?.aspectFitImage(previewSize, previewSize)
        default:
            return nil
        }
    }

    func symbolImage(_ name: String?, font: NSFont) -> NSImage? {
        guard let name else { return nil }
        if let image = symbolImages[name] {
            return image
        }
        let configuration = NSImage.SymbolConfiguration(pointSize: font.pointSize, weight: .regular)
        guard let image = NSImage(systemSymbolName: name, accessibilityDescription: nil)?
            .withSymbolConfiguration(configuration) else {
            return nil
        }
        image.isTemplate = true
        symbolImages[name] = image
        return image
    }

    func numberedMenuTitle(_ title: String, listNumber: Int) -> NSAttributedString {
        MonospacedDigitFormatter.numberedTitle(title, listNumber: listNumber, showsNumber: marksMenuItemsWithNumbers)
    }
}

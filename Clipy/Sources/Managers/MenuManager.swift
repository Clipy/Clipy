//
//  MenuManager.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2016/03/08.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Cocoa
import Combine
import Dependencies
import Sharing

final class MenuManager: NSObject {

    // MARK: - Properties
    // Menus
    private var clipMenu: NSMenu?
    private var historyMenu: NSMenu?
    private var snippetMenu: NSMenu?
    // StatusMenu
    private lazy var statusBarItem: NSStatusItem = {
        let item = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
        item.button?.toolTip = "\(Constants.Application.name)\(Bundle.main.appVersion ?? "")"
        item.menu = clipMenu
        return item
    }()
    // Icon Cache
    private let folderIcon = NSImage(resource: .iconFolder)
    private let snippetIcon = NSImage(resource: .iconText)
    private let kMaxKeyEquivalents = 10

    @Dependency(\.pasteboardHistoryRepository)
    private var pasteboardHistoryRepository
    @Dependency(\.snippetRepository)
    private var snippetRepository
    @Dependency(\.mainQueue)
    private var mainQueue
    private var cancellables: Set<AnyCancellable> = []
    private var snippetFolderDetails = [SnippetFolderDetail]()

    @Shared(.statusItemDisplayMode)
    private var statusItemDisplayMode
    @Shared(.reordersClipsAfterPasting)
    private var reordersClipsAfterPasting
    @Shared(.showsClearHistoryMenuItem)
    private var showsClearHistoryMenuItem
    @Shared(.maximumHistoryCount)
    private var maximumHistoryCount
    @Shared(.showsIconsInMenu)
    private var showsIconsInMenu
    @Shared(.inlineMenuItemLimit)
    private var inlineMenuItemLimit
    @Shared(.folderMenuItemLimit)
    private var folderMenuItemLimit
    @Shared(.maximumMenuItemTitleLength)
    private var maximumMenuItemTitleLength
    @Shared(.startsMenuItemTitlesAtZero)
    private var startsMenuItemTitlesAtZero
    @Shared(.marksMenuItemsWithNumbers)
    private var marksMenuItemsWithNumbers
    @Shared(.showsToolTipsOnMenuItems)
    private var showsToolTipsOnMenuItems
    @Shared(.showsImagesInMenu)
    private var showsImagesInMenu
    @Shared(.addsNumericKeyEquivalents)
    private var addsNumericKeyEquivalents
    @Shared(.maximumToolTipLength)
    private var maximumToolTipLength
    @Shared(.showsColorPreviewInMenu)
    private var showsColorPreviewInMenu
    @Shared(.thumbnailWidth)
    private var thumbnailWidth
    @Shared(.thumbnailHeight)
    private var thumbnailHeight

    // MARK: - Enum Values
    enum StatusType: Int {
        case none, black, white
    }

    // MARK: - Initialize
    override init() {
        super.init()
        folderIcon.isTemplate = true
        folderIcon.size = NSSize(width: 15, height: 13)
        snippetIcon.isTemplate = true
        snippetIcon.size = NSSize(width: 12, height: 13)
    }

    func setup() {
        bind()
    }

}

// MARK: - Popup Menu
extension MenuManager {
    func popUpMenu(_ type: MenuType) {
        let menu: NSMenu?
        switch type {
        case .main:
            menu = clipMenu
        case .history:
            menu = historyMenu
        case .snippet:
            menu = snippetMenu
        }
        menu?.highlightingFirstItemIfPossible()
        menu?.popUp(positioning: nil, at: NSEvent.mouseLocation, in: nil)
    }

    func popUpSnippetFolder(_ folderDetail: SnippetFolderDetail) {
        let folderMenu = NSMenu(title: folderDetail.folder.title)
        // Folder title
        let labelItem = NSMenuItem(title: folderDetail.folder.title, action: nil)
        labelItem.isEnabled = false
        folderMenu.addItem(labelItem)
        // Snippets
        var index = firstIndexOfMenuItems()
        folderDetail.snippets
            .filter { $0.isEnabled }
            .forEach { snippet in
                let subMenuItem = makeSnippetMenuItem(snippet, listNumber: index)
                folderMenu.addItem(subMenuItem)
                index += 1
            }
        folderMenu.highlightingFirstItemIfPossible()
        folderMenu.popUp(positioning: nil, at: NSEvent.mouseLocation, in: nil)
    }
}

// MARK: - Binding
private extension MenuManager {
    func bind() {
        pasteboardHistoryRepository.observeHistories()
            .receive(on: mainQueue)
            .sink { [weak self] _ in self?.createClipMenu() }
            .store(in: &cancellables)
        snippetRepository.observeFolderDetails()
            .receive(on: mainQueue)
            .sink { [weak self] folderDetails in
                self?.snippetFolderDetails = folderDetails
                self?.createClipMenu()
            }
            .store(in: &cancellables)
        // Menu icon
        $statusItemDisplayMode.changes(includingInitialValue: true)
            .receive(on: mainQueue)
            .sink { [weak self] key in
                self?.changeStatusItem(StatusType(rawValue: key) ?? .black)
            }
            .store(in: &cancellables)
        // Sort clips
        $reordersClipsAfterPasting.changes()
            .receive(on: mainQueue)
            .sink { [weak self] _ in
                self?.createClipMenu()
            }
            .store(in: &cancellables)
        // Observe change preference settings
        Publishers.MergeMany(
            [
                $showsClearHistoryMenuItem.changeEvents(),
                $maximumHistoryCount.changeEvents(),
                $showsIconsInMenu.changeEvents(),
                $inlineMenuItemLimit.changeEvents(),
                $folderMenuItemLimit.changeEvents(),
                $maximumMenuItemTitleLength.changeEvents(),
                $startsMenuItemTitlesAtZero.changeEvents(),
                $marksMenuItemsWithNumbers.changeEvents(),
                $showsToolTipsOnMenuItems.changeEvents(),
                $showsImagesInMenu.changeEvents(),
                $addsNumericKeyEquivalents.changeEvents(),
                $maximumToolTipLength.changeEvents(),
                $showsColorPreviewInMenu.changeEvents()
            ]
        )
        .throttle(for: .seconds(1), scheduler: mainQueue, latest: true)
        .sink { [weak self] in
            self?.createClipMenu()
        }
        .store(in: &cancellables)
    }
}

// MARK: - Menus
private extension MenuManager {
     func createClipMenu() {
        clipMenu = NSMenu(title: Constants.Application.name)
        historyMenu = NSMenu(title: Constants.Menu.history)
        snippetMenu = NSMenu(title: Constants.Menu.snippet)

        addHistoryItems(clipMenu!)
        addHistoryItems(historyMenu!)

        addSnippetItems(clipMenu!, separateMenu: true, details: snippetFolderDetails)
        addSnippetItems(snippetMenu!, separateMenu: false, details: snippetFolderDetails)

        clipMenu?.addItem(NSMenuItem.separator())

        if showsClearHistoryMenuItem {
            clipMenu?.addItem(NSMenuItem(title: String(localized: "Clear History"), action: #selector(AppDelegate.clearAllHistory)))
        }

        clipMenu?.addItem(NSMenuItem(title: String(localized: "Edit Snippets"), action: #selector(AppDelegate.showSnippetEditorWindow)))
        clipMenu?.addItem(NSMenuItem(title: String(localized: "Preferences"), action: #selector(AppDelegate.showPreferenceWindow)))
        clipMenu?.addItem(NSMenuItem.separator())
        clipMenu?.addItem(NSMenuItem(title: String(localized: "Quit Clipy"), action: #selector(AppDelegate.terminate)))

        statusBarItem.menu = clipMenu
    }

    func makeSubmenuItem(_ count: Int, start: Int, end: Int, numberOfItems: Int) -> NSMenuItem {
        var count = count
        if start == 0 {
            count -= 1
        }
        var lastNumber = count + numberOfItems
        if end < lastNumber {
            lastNumber = end
        }
        let menuItemTitle = MonospacedDigitFormatter.rangeTitle(
            firstNumber: count + 1,
            lastNumber: lastNumber
        )
        let subMenuItem = makeSubmenuItem(menuItemTitle.string)
        subMenuItem.attributedTitle = menuItemTitle
        return subMenuItem
    }

    func makeSubmenuItem(_ title: String) -> NSMenuItem {
        let subMenu = NSMenu(title: "")
        let subMenuItem = NSMenuItem(title: title, action: nil)
        subMenuItem.submenu = subMenu
        subMenuItem.image = showsIconsInMenu ? folderIcon : nil
        return subMenuItem
    }
}

// MARK: - Clips
private extension MenuManager {
    func addHistoryItems(_ menu: NSMenu) {
        let placeInLine = inlineMenuItemLimit
        let placeInsideFolder = folderMenuItemLimit
        let maxHistory = maximumHistoryCount

        // History title
        let labelItem = NSMenuItem(title: String(localized: "History"), action: nil)
        labelItem.isEnabled = false
        menu.addItem(labelItem)

        // History
        let firstIndex = firstIndexOfMenuItems()
        var listNumber = firstIndex
        var subMenuCount = placeInLine
        var subMenuIndex = 1 + placeInLine

        let historyDetails = pasteboardHistoryRepository.fetchHistoryDetails(
            sortsByCreatedAt: !reordersClipsAfterPasting,
            includesThumbnailAsset: showsImagesInMenu || showsColorPreviewInMenu,
            limit: maxHistory
        )
        let currentSize = historyDetails.count
        var i = 0
        historyDetails.forEach { historyDetail in
            if placeInLine < 1 || placeInLine - 1 < i {
                // Folder
                if i == subMenuCount {
                    let subMenuItem = makeSubmenuItem(subMenuCount, start: firstIndex, end: currentSize, numberOfItems: placeInsideFolder)
                    menu.addItem(subMenuItem)
                    listNumber = firstIndex
                }

                // Clip
                if let subMenu = menu.item(at: subMenuIndex)?.submenu {
                    let menuItem = makeClipMenuItem(historyDetail, index: i, listNumber: listNumber)
                    subMenu.addItem(menuItem)
                    listNumber += 1
                }
            } else {
                // Clip
                let menuItem = makeClipMenuItem(historyDetail, index: i, listNumber: listNumber)
                menu.addItem(menuItem)
                listNumber += 1
            }

            i += 1
            if i == subMenuCount + placeInsideFolder {
                subMenuCount += placeInsideFolder
                subMenuIndex += 1
            }
        }
    }

    func makeClipMenuItem(_ historyDetail: PasteboardHistoryDetail, index: Int, listNumber: Int) -> NSMenuItem {
        let history = historyDetail.history
        var keyEquivalent = ""
        if addsNumericKeyEquivalents && (index < kMaxKeyEquivalents) {
            var shortCutNumber = startsMenuItemTitlesAtZero ? index : index + 1
            if shortCutNumber == kMaxKeyEquivalents {
                shortCutNumber = 0
            }
            keyEquivalent = "\(shortCutNumber)"
        }

        let titleWithMark = MonospacedDigitFormatter.numberedTitle(history.typedTitle, listNumber: listNumber, showsNumber: marksMenuItemsWithNumbers)
        let menuItem = NSMenuItem(title: titleWithMark.string, action: #selector(AppDelegate.selectClipMenuItem(_:)), keyEquivalent: keyEquivalent)
        menuItem.attributedTitle = titleWithMark
        menuItem.representedObject = history.id
        menuItem.toolTip = history.toolTip

        if showsImagesInMenu || showsColorPreviewInMenu,
           let thumbnailAsset = historyDetail.thumbnailAsset,
           let image = NSImage(data: thumbnailAsset.data),
           (thumbnailAsset.kind == .image && showsImagesInMenu) || (thumbnailAsset.kind == .colorCode && showsColorPreviewInMenu) {
            menuItem.image = image.aspectFitImage(CGFloat(thumbnailWidth), CGFloat(thumbnailHeight))
        }

        return menuItem
    }
}

// MARK: - Snippets
private extension MenuManager {
    func addSnippetItems(_ menu: NSMenu, separateMenu: Bool, details: [SnippetFolderDetail]) {
        guard !details.isEmpty else { return }

        if separateMenu {
            menu.addItem(NSMenuItem.separator())
        }

        // Snippet title
        let labelItem = NSMenuItem(title: String(localized: "Snippet"), action: nil)
        labelItem.isEnabled = false
        menu.addItem(labelItem)

        var subMenuIndex = menu.numberOfItems - 1
        let firstIndex = firstIndexOfMenuItems()
        details
            .filter { $0.folder.isEnabled }
            .forEach { detail in
                let folderTitle = detail.folder.title
                let subMenuItem = makeSubmenuItem(folderTitle)
                menu.addItem(subMenuItem)
                subMenuIndex += 1

                var i = firstIndex
                detail.snippets
                    .filter { $0.isEnabled }
                    .forEach { snippet in
                        let subMenuItem = makeSnippetMenuItem(snippet, listNumber: i)
                        if let subMenu = menu.item(at: subMenuIndex)?.submenu {
                            subMenu.addItem(subMenuItem)
                            i += 1
                        }
                    }
            }
    }

    func makeSnippetMenuItem(_ snippet: Snippet, listNumber: Int) -> NSMenuItem {
        let titleWithMark = MonospacedDigitFormatter.numberedTitle(snippet.title.trimmedMenuTitle, listNumber: listNumber, showsNumber: marksMenuItemsWithNumbers)
        let menuItem = NSMenuItem(title: titleWithMark.string, action: #selector(AppDelegate.selectSnippetMenuItem(_:)), keyEquivalent: "")
        menuItem.attributedTitle = titleWithMark
        menuItem.representedObject = snippet.id
        menuItem.toolTip = snippet.toolTip
        menuItem.image = showsIconsInMenu ? snippetIcon : nil

        return menuItem
    }
}

// MARK: - Status Item
private extension MenuManager {
    func changeStatusItem(_ type: StatusType) {
        switch type {
        case .black:
            let image = NSImage(resource: .statusbarMenuBlack)
            image.isTemplate = true
            statusBarItem.button?.image = image
            statusBarItem.isVisible = true
        case .white:
            let image = NSImage(resource: .statusbarMenuWhite)
            image.isTemplate = true
            statusBarItem.button?.image = image
            statusBarItem.isVisible = true
        case .none:
            statusBarItem.isVisible = false
        }
    }
}

// MARK: - Settings
private extension MenuManager {
    func firstIndexOfMenuItems() -> NSInteger {
        return startsMenuItemTitlesAtZero ? 0 : 1
    }
}

extension DependencyValues {
    var menuManager: MenuManager {
        get { self[MenuManagerKey.self] }
        set { self[MenuManagerKey.self] = newValue }
    }

    private enum MenuManagerKey: DependencyKey {
        static let liveValue = MenuManager()
    }
}

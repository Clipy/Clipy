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

import AlternativeMenu
import Cocoa
import Combine
import Dependencies
import Sharing

final class MenuManager: NSObject {

    // MARK: - Properties
    @MainActor private var preparedMenus: [MenuType: NSMenu] = [:]
    @MainActor private var searchMenu: AlternaiveSearchMenu?
    @MainActor private lazy var statusBarItem: NSStatusItem = {
        let item = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
        item.button?.toolTip = Bundle.main.appName
        return item
    }()
    private let nativeBuilder: NSMenuBuilder
    private let alternativeBuilder: AlternativeMenuBuilder

    @Dependency(\.pasteboardHistoryRepository)
    private var pasteboardHistoryRepository
    @Dependency(\.snippetRepository)
    private var snippetRepository
    @Dependency(\.mainQueue)
    private var mainQueue
    private var cancellables: Set<AnyCancellable> = []
    private var snippetFolderDetails = [SnippetFolderDetail]()

    @Shared(.reordersClipsAfterPasting)
    private var reordersClipsAfterPasting
    @Shared(.showsClearHistoryMenuItem)
    private var showsClearHistoryMenuItem
    @Shared(.maximumHistoryCount)
    private var maximumHistoryCount
    @Shared(.highlightsFirstItemOnPopUp)
    private var highlightsFirstItemOnPopUp
    @Shared(.showsSearchBar)
    private var showsSearchBar
    @Shared(.showsImagesInMenu)
    private var showsImagesInMenu
    @Shared(.showsColorPreviewInMenu)
    private var showsColorPreviewInMenu

    // MARK: - Enum Values
    enum StatusType: Int {
        case none, black, white
    }

    override init() {
        let nativeBuilder = NSMenuBuilder()
        self.nativeBuilder = nativeBuilder
        self.alternativeBuilder = AlternativeMenuBuilder(nativeBuilder: nativeBuilder)
        super.init()
    }

    @MainActor
    func setup() {
        bind()
    }
}

// MARK: - Popup Menu
extension MenuManager {
    @MainActor
    func popUpMenu(_ type: MenuType) {
        popUpMenu(preparedMenus[type], scope: .menu(type))
    }

    @MainActor
    func popUpSnippetFolder(_ folderDetail: SnippetFolderDetail) {
        let menu = nativeBuilder.makeSnippetFolderMenu(detail: folderDetail)
        popUpMenu(menu, scope: .snippetFolder(folderDetail.folder.id))
    }

    @MainActor
    private func popUpMenu(_ menu: NSMenu?, scope: MenuSearchScope) {
        guard let menu else { return }
        guard showsSearchBar else {
            popUp(menu)
            return
        }
        if searchMenu?.scope != scope {
            searchMenu?.dismiss()
            searchMenu = AlternaiveSearchMenu(scope: scope, builder: alternativeBuilder)
        }
        searchMenu?.present(initialItems: alternativeBuilder.makeItems(from: menu.items))
    }

    @MainActor
    private func popUp(_ menu: NSMenu) {
        if highlightsFirstItemOnPopUp {
            menu.popUpHighlightingFirstItem(positioning: nil, at: NSEvent.mouseLocation, in: nil)
        } else {
            menu.popUp(positioning: nil, at: NSEvent.mouseLocation, in: nil)
        }
    }
}

// MARK: - Binding
private extension MenuManager {
    @MainActor
    func bind() {
        pasteboardHistoryRepository.observeHistories()
            .receive(on: mainQueue)
            .sink { [weak self] _ in self?.updateMenus() }
            .store(in: &cancellables)
        snippetRepository.observeFolderDetails()
            .receive(on: mainQueue)
            .sink { [weak self] folderDetails in
                self?.snippetFolderDetails = folderDetails
                self?.updateMenus()
            }
            .store(in: &cancellables)
        // Menu icon
        Shared(.statusItemDisplayMode).changes(includingInitialValue: true)
            .receive(on: mainQueue)
            .sink { [weak self] key in
                self?.changeStatusItem(StatusType(rawValue: key) ?? .black)
            }
            .store(in: &cancellables)
        // Observe change preference settings
        Publishers.MergeMany(
            [
                $reordersClipsAfterPasting.changeEvents(),
                $showsClearHistoryMenuItem.changeEvents(),
                $maximumHistoryCount.changeEvents(),
                Shared(.showsSnippetsAboveHistory).changeEvents(),
                Shared(.inlineMenuItemLimit).changeEvents(),
                Shared(.folderMenuItemLimit).changeEvents(),
                Shared(.maximumMenuItemTitleLength).changeEvents(),
                Shared(.startsMenuItemTitlesAtZero).changeEvents(),
                Shared(.marksMenuItemsWithNumbers).changeEvents(),
                Shared(.showsToolTipsOnMenuItems).changeEvents(),
                $showsImagesInMenu.changeEvents(),
                Shared(.addsNumericKeyEquivalents).changeEvents(),
                Shared(.maximumToolTipLength).changeEvents(),
                $showsColorPreviewInMenu.changeEvents(),
                Shared(.thumbnailWidth).changeEvents(),
                Shared(.thumbnailHeight).changeEvents()
            ]
        )
        .throttle(for: .seconds(1), scheduler: mainQueue, latest: true)
        .sink { [weak self] in
            self?.updateMenus()
        }
        .store(in: &cancellables)
    }
}

// MARK: - Menus
extension MenuManager {
    @MainActor
    func updateMenus() {
        let histories = fetchHistoryDetails()
        let main = nativeBuilder.makeMainMenu(
            histories: histories,
            folders: snippetFolderDetails
        )
        preparedMenus = [
            .main: main,
            .history: nativeBuilder.makeHistoryMenu(details: histories),
            .snippet: nativeBuilder.makeSnippetMenu(details: snippetFolderDetails)
        ]
        statusBarItem.menu = main
    }
}

// MARK: - Clips
private extension MenuManager {
    func fetchHistoryDetails() -> [PasteboardHistoryDetail] {
        pasteboardHistoryRepository.fetchHistoryDetails(
            sortsByCreatedAt: !reordersClipsAfterPasting,
            includesThumbnailAsset: showsImagesInMenu || showsColorPreviewInMenu,
            limit: maximumHistoryCount
        )
    }
}

// MARK: - Status Item
private extension MenuManager {
    @MainActor
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

extension DependencyValues {
    var menuManager: MenuManager {
        get { self[MenuManagerKey.self] }
        set { self[MenuManagerKey.self] = newValue }
    }

    private enum MenuManagerKey: DependencyKey {
        static let liveValue = MenuManager()
    }
}

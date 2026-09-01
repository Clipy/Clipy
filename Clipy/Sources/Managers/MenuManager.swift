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
import RxCocoa
import RxSwift

final class MenuManager: NSObject, NSSearchFieldDelegate, NSMenuDelegate {
    // MARK: - Properties
    private var clipMenu: NSMenu?
    private var historyMenu: NSMenu?
    private var snippetMenu: NSMenu?
    private weak var openMenu: NSMenu?
    private var needsMenuRebuild = false
    private var clipSearchField: NSSearchField?
    private var historySearchField: NSSearchField?
    private lazy var statusBarItem: NSStatusItem = {
        let item = NSStatusBar.system.statusItem(withLength: NSStatusItem.variableLength)
        item.button?.toolTip = "\(Constants.Application.name)\(Bundle.main.appVersion ?? "")"
        item.menu = clipMenu
        return item
    }()
    private let folderIcon = NSImage(resource: .iconFolder)
    private let snippetIcon = NSImage(resource: .iconText)
    private let disposeBag = DisposeBag()
    private let kMaxKeyEquivalents = 10

    @Dependency(\.pasteboardHistoryRepository)
    private var pasteboardHistoryRepository
    @Dependency(\.snippetRepository)
    private var snippetRepository
    @Dependency(\.mainQueue)
    private var mainQueue
    @Dependency(\.defaultAppStorage)
    private var appStorage
    private var cancellables: Set<AnyCancellable> = []
    private var historyDetails = [PasteboardHistoryDetail]()
    private var snippetFolderDetails = [SnippetFolderDetail]()

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

    func controlTextDidChange(_ notification: Notification) {
        guard let searchField = notification.object as? NSSearchField else { return }
        if searchField === clipSearchField, let clipMenu {
            rebuildClipMenu(clipMenu, query: searchField.stringValue)
        } else if searchField === historySearchField, let historyMenu {
            rebuildHistoryMenu(historyMenu, query: searchField.stringValue)
        }
    }

    func menuWillOpen(_ menu: NSMenu) {
        guard let searchField = searchField(for: menu) else { return }
        openMenu = menu
        searchField.stringValue = ""
        rebuildSearchableMenu(menu, query: "")
        RunLoop.current.perform(inModes: [.eventTracking]) { [weak searchField] in
            searchField?.window?.makeFirstResponder(searchField)
        }
    }

    func menuDidClose(_ menu: NSMenu) {
        searchField(for: menu)?.stringValue = ""
        openMenu = nil
        guard needsMenuRebuild else { return }
        needsMenuRebuild = false
        createClipMenu()
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
        appStorage.rx.observe(Int.self, Constants.UserDefaults.showStatusItem, retainSelf: false)
            .compactMap { $0 }
            .asDriver(onErrorDriveWith: .empty())
            .drive(onNext: { [weak self] key in
                self?.changeStatusItem(StatusType(rawValue: key) ?? .black)
            })
            .disposed(by: disposeBag)
        // Sort clips
        appStorage.rx.observe(Bool.self, Constants.UserDefaults.reorderClipsAfterPasting, options: [.new], retainSelf: false)
            .compactMap { $0 }
            .asDriver(onErrorDriveWith: .empty())
            .drive(onNext: { [weak self] _ in
                self?.createClipMenu()
            })
            .disposed(by: disposeBag)
        // Observe change preference settings
        var menuChangedObservables = [Observable<Void>]()
        menuChangedObservables.append(appStorage.rx.observe(Bool.self, Constants.UserDefaults.addClearHistoryMenuItem, options: [.new], retainSelf: false)
                                        .compactMap { $0 }.distinctUntilChanged().map { _ in })
        menuChangedObservables.append(appStorage.rx.observe(Int.self, Constants.UserDefaults.maxHistorySize, options: [.new], retainSelf: false)
                                        .compactMap { $0 }.distinctUntilChanged().map { _ in })
        menuChangedObservables.append(appStorage.rx.observe(Bool.self, Constants.UserDefaults.showIconInTheMenu, options: [.new], retainSelf: false)
                                        .compactMap { $0 }.distinctUntilChanged().map { _ in })
        menuChangedObservables.append(appStorage.rx.observe(Int.self, Constants.UserDefaults.numberOfItemsPlaceInline, options: [.new], retainSelf: false)
                                        .compactMap { $0 }.distinctUntilChanged().map { _ in })
        menuChangedObservables.append(appStorage.rx.observe(Int.self, Constants.UserDefaults.numberOfItemsPlaceInsideFolder, options: [.new], retainSelf: false)
                                        .compactMap { $0 }.distinctUntilChanged().map { _ in })
        menuChangedObservables.append(appStorage.rx.observe(Int.self, Constants.UserDefaults.maxMenuItemTitleLength, options: [.new], retainSelf: false)
                                        .compactMap { $0 }.distinctUntilChanged().map { _ in })
        menuChangedObservables.append(appStorage.rx.observe(Bool.self, Constants.UserDefaults.menuItemsTitleStartWithZero, options: [.new], retainSelf: false)
                                        .compactMap { $0 }.distinctUntilChanged().map { _ in })
        menuChangedObservables.append(appStorage.rx.observe(Bool.self, Constants.UserDefaults.menuItemsAreMarkedWithNumbers, options: [.new], retainSelf: false)
                                        .compactMap { $0 }.distinctUntilChanged().map { _ in })
        menuChangedObservables.append(appStorage.rx.observe(Bool.self, Constants.UserDefaults.showToolTipOnMenuItem, options: [.new], retainSelf: false)
                                        .compactMap { $0 }.distinctUntilChanged().map { _ in })
        menuChangedObservables.append(appStorage.rx.observe(Bool.self, Constants.UserDefaults.showImageInTheMenu, options: [.new], retainSelf: false)
                                        .compactMap { $0 }.distinctUntilChanged().map { _ in })
        menuChangedObservables.append(appStorage.rx.observe(Bool.self, Constants.UserDefaults.addNumericKeyEquivalents, options: [.new], retainSelf: false)
                                        .compactMap { $0 }.distinctUntilChanged().map { _ in })
        menuChangedObservables.append(appStorage.rx.observe(Int.self, Constants.UserDefaults.maxLengthOfToolTip, options: [.new], retainSelf: false)
                                        .compactMap { $0 }.distinctUntilChanged().map { _ in })
        menuChangedObservables.append(appStorage.rx.observe(Bool.self, Constants.UserDefaults.showColorPreviewInTheMenu, options: [.new], retainSelf: false)
                                        .compactMap { $0 }.distinctUntilChanged().map { _ in })
        Observable.merge(menuChangedObservables)
            .throttle(.seconds(1), scheduler: MainScheduler.instance)
            .asDriver(onErrorDriveWith: .empty())
            .drive(onNext: { [weak self] in
                self?.createClipMenu()
            })
            .disposed(by: disposeBag)
    }
}

// MARK: - Menus
private extension MenuManager {
    func createClipMenu() {
        guard openMenu == nil else {
            needsMenuRebuild = true
            return
        }
        clipMenu = NSMenu(title: Constants.Application.name)
        historyMenu = NSMenu(title: Constants.Menu.history)
        snippetMenu = NSMenu(title: Constants.Menu.snippet)

        historyDetails = fetchHistoryDetails()
        clipSearchField = clipMenu?.addSearchField()
        clipSearchField?.delegate = self
        clipMenu?.delegate = self
        historySearchField = historyMenu?.addSearchField()
        historySearchField?.delegate = self
        historyMenu?.delegate = self
        rebuildClipMenu(clipMenu!, query: "")
        rebuildHistoryMenu(historyMenu!, query: "")
        addSnippetItems(snippetMenu!, separateMenu: false, details: snippetFolderDetails)

        statusBarItem.menu = clipMenu
    }

    func rebuildSearchableMenu(_ menu: NSMenu, query: String) {
        if menu === clipMenu {
            rebuildClipMenu(menu, query: query)
        } else if menu === historyMenu {
            rebuildHistoryMenu(menu, query: query)
        }
    }

    func rebuildClipMenu(_ menu: NSMenu, query: String) {
        removeItemsAfterSearchField(from: menu)
        addHistoryItems(menu, query: query)
        guard query.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty else {
            menu.highlightingFirstItemIfPossible()
            return
        }

        addSnippetItems(menu, separateMenu: true, details: snippetFolderDetails)
        menu.addItem(NSMenuItem.separator())
        if appStorage.bool(forKey: Constants.UserDefaults.addClearHistoryMenuItem) {
            menu.addItem(NSMenuItem(title: String(localized: "Clear History"), action: #selector(AppDelegate.clearAllHistory)))
        }
        menu.addItem(NSMenuItem(title: String(localized: "Edit Snippets"), action: #selector(AppDelegate.showSnippetEditorWindow)))
        menu.addItem(NSMenuItem(title: String(localized: "Preferences"), action: #selector(AppDelegate.showPreferenceWindow)))
        menu.addItem(NSMenuItem.separator())
        menu.addItem(NSMenuItem(title: String(localized: "Quit Clipy"), action: #selector(AppDelegate.terminate)))
    }

    func rebuildHistoryMenu(_ menu: NSMenu, query: String) {
        removeItemsAfterSearchField(from: menu)
        addHistoryItems(menu, query: query)
        if !query.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty {
            menu.highlightingFirstItemIfPossible()
        }
    }

    func removeItemsAfterSearchField(from menu: NSMenu) {
        while menu.numberOfItems > 1 {
            menu.removeItem(at: 1)
        }
    }

    func searchField(for menu: NSMenu) -> NSSearchField? {
        if menu === clipMenu {
            return clipSearchField
        }
        if menu === historyMenu {
            return historySearchField
        }
        return nil
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
        subMenuItem.image = (appStorage.bool(forKey: Constants.UserDefaults.showIconInTheMenu)) ? folderIcon : nil
        return subMenuItem
    }
}

// MARK: - Clips
private extension MenuManager {
    func addHistoryItems(_ menu: NSMenu, query: String) {
        let placeInLine = appStorage.integer(forKey: Constants.UserDefaults.numberOfItemsPlaceInline)
        let placeInsideFolder = appStorage.integer(forKey: Constants.UserDefaults.numberOfItemsPlaceInsideFolder)
        let normalizedQuery = query.trimmingCharacters(in: .whitespacesAndNewlines)

        // History title
        let labelItem = NSMenuItem(title: String(localized: "History"), action: nil)
        labelItem.isEnabled = false
        menu.addItem(labelItem)

        // History
        let firstIndex = firstIndexOfMenuItems()
        var listNumber = firstIndex
        var subMenuCount = placeInLine
        var currentSubMenu: NSMenu?

        let filteredHistoryDetails = normalizedQuery.isEmpty
            ? historyDetails
            : historyDetails.filter { $0.history.matchesSearch(normalizedQuery) }
        if filteredHistoryDetails.isEmpty, !normalizedQuery.isEmpty {
            let noMatchesItem = NSMenuItem(title: String(localized: "No Matching Clipboard Items"), action: nil)
            noMatchesItem.isEnabled = false
            menu.addItem(noMatchesItem)
            return
        }

        let currentSize = filteredHistoryDetails.count
        var i = 0
        filteredHistoryDetails.forEach { historyDetail in
            if normalizedQuery.isEmpty && (placeInLine < 1 || placeInLine - 1 < i) {
                // Folder
                if i == subMenuCount {
                    let subMenuItem = makeSubmenuItem(subMenuCount, start: firstIndex, end: currentSize, numberOfItems: placeInsideFolder)
                    menu.addItem(subMenuItem)
                    currentSubMenu = subMenuItem.submenu
                    listNumber = firstIndex
                }

                // Clip
                if let subMenu = currentSubMenu {
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
            }
        }
    }

    func fetchHistoryDetails() -> [PasteboardHistoryDetail] {
        let reorderClipsAfterPasting = appStorage.bool(forKey: Constants.UserDefaults.reorderClipsAfterPasting)
        let isShowImage = appStorage.bool(forKey: Constants.UserDefaults.showImageInTheMenu)
        let isShowColorCode = appStorage.bool(forKey: Constants.UserDefaults.showColorPreviewInTheMenu)
        return pasteboardHistoryRepository.fetchHistoryDetails(
            sortsByCreatedAt: !reorderClipsAfterPasting,
            includesThumbnailAsset: isShowImage || isShowColorCode,
            limit: appStorage.integer(forKey: Constants.UserDefaults.maxHistorySize)
        )
    }

    func makeClipMenuItem(_ historyDetail: PasteboardHistoryDetail, index: Int, listNumber: Int) -> NSMenuItem {
        let history = historyDetail.history
        let isMarkWithNumber = appStorage.bool(forKey: Constants.UserDefaults.menuItemsAreMarkedWithNumbers)
        let isShowImage = appStorage.bool(forKey: Constants.UserDefaults.showImageInTheMenu)
        let isShowColorCode = appStorage.bool(forKey: Constants.UserDefaults.showColorPreviewInTheMenu)
        let addNumbericKeyEquivalents = appStorage.bool(forKey: Constants.UserDefaults.addNumericKeyEquivalents)

        var keyEquivalent = ""
        if addNumbericKeyEquivalents && (index < kMaxKeyEquivalents) {
            let isStartFromZero = appStorage.bool(forKey: Constants.UserDefaults.menuItemsTitleStartWithZero)

            var shortCutNumber = (isStartFromZero) ? index : index + 1
            if shortCutNumber == kMaxKeyEquivalents {
                shortCutNumber = 0
            }
            keyEquivalent = "\(shortCutNumber)"
        }

        let titleWithMark = MonospacedDigitFormatter.numberedTitle(history.typedTitle, listNumber: listNumber, showsNumber: isMarkWithNumber)
        let menuItem = NSMenuItem(title: titleWithMark.string, action: #selector(AppDelegate.selectClipMenuItem(_:)), keyEquivalent: keyEquivalent)
        menuItem.attributedTitle = titleWithMark
        menuItem.representedObject = history.id
        menuItem.toolTip = history.toolTip

        if isShowImage || isShowColorCode,
           let thumbnailAsset = historyDetail.thumbnailAsset,
           let image = NSImage(data: thumbnailAsset.data),
           (thumbnailAsset.kind == .image && isShowImage) || (thumbnailAsset.kind == .colorCode && isShowColorCode) {
            let width = appStorage.integer(forKey: Constants.UserDefaults.thumbnailWidth)
            let height = appStorage.integer(forKey: Constants.UserDefaults.thumbnailHeight)
            menuItem.image = image.aspectFitImage(CGFloat(width), CGFloat(height))
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
        let isMarkWithNumber = appStorage.bool(forKey: Constants.UserDefaults.menuItemsAreMarkedWithNumbers)
        let isShowIcon = appStorage.bool(forKey: Constants.UserDefaults.showIconInTheMenu)

        let titleWithMark = MonospacedDigitFormatter.numberedTitle(snippet.title.trimmedMenuTitle, listNumber: listNumber, showsNumber: isMarkWithNumber)
        let menuItem = NSMenuItem(title: titleWithMark.string, action: #selector(AppDelegate.selectSnippetMenuItem(_:)), keyEquivalent: "")
        menuItem.attributedTitle = titleWithMark
        menuItem.representedObject = snippet.id
        menuItem.toolTip = snippet.toolTip
        menuItem.image = (isShowIcon) ? snippetIcon : nil

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
        return appStorage.bool(forKey: Constants.UserDefaults.menuItemsTitleStartWithZero) ? 0 : 1
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

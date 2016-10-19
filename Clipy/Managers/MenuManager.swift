//
//  MenuManager.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/03/08.
//  Copyright (c) 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import PINCache
import RealmSwift
import RxCocoa
import RxSwift
import NSObject_Rx
import RxOptional

final class MenuManager: NSObject {
    // MARK: - Properties
    static let sharedManager = MenuManager()
    // Menus
    private var clipMenu: NSMenu?
    private var historyMenu: NSMenu?
    private var snippetMenu: NSMenu?
    // StatusMenu
    private var statusItem: NSStatusItem?
    // Icon Cache
    private let folderIcon = NSImage(assetIdentifier: .IconFolder)
    private let snippetIcon = NSImage(assetIdentifier: .IconText)
    // Other
    private let defaults = NSUserDefaults.standardUserDefaults()
    private let notificationCenter = NSNotificationCenter.defaultCenter()
    private let kMaxKeyEquivalents = 10
    private let shortenSymbol = "..."
    // Realm Results
    private let realm = try! Realm()
    private var clipResults: Results<CPYClip>
    private let folderResults: Results<CPYFolder>
    // Realm Token
    private var clipToken: NotificationToken?
    private var snippetToken: NotificationToken?

    // MARK: - Enum Values
    enum StatusType: Int {
        case None, Black, White
    }

    // MARK: - Initialize
    override init() {
        clipResults = realm.objects(CPYClip.self).sorted("updateTime", ascending: !defaults.boolForKey(Constants.UserDefaults.reorderClipsAfterPasting))
        folderResults = realm.objects(CPYFolder.self).sorted("index", ascending: true)
        super.init()
        folderIcon.template = true
        folderIcon.size = NSSize(width: 15, height: 13)
        snippetIcon.template = true
        snippetIcon.size = NSSize(width: 12, height: 13)
    }

    func setup() {
        bind()
    }
}

// MARK: - Popup Menu
extension MenuManager {
    func popUpMenu(type: MenuType) {
        let menu: NSMenu?
        switch type {
        case .Main:
            menu = clipMenu
        case .History:
            menu = historyMenu
        case .Snippet:
            menu = snippetMenu
        }
        menu?.popUpMenuPositioningItem(nil, atLocation: NSEvent.mouseLocation(), inView: nil)
    }

    func popUpSnippetFolder(folder: CPYFolder) {
        let folderMenu = NSMenu(title: folder.title)
        // Folder title
        let labelItem = NSMenuItem(title: folder.title, action: nil)
        labelItem.enabled = false
        folderMenu.addItem(labelItem)
        // Snippets
        var index = firstIndexOfMenuItems()
        folder.snippets
            .sorted("index", ascending: true)
            .filter { $0.enable }
            .forEach { snippet in
                let subMenuItem = makeSnippetMenuItem(snippet, listNumber: index)
                folderMenu.addItem(subMenuItem)
                index += 1
            }
        folderMenu.popUpMenuPositioningItem(nil, atLocation: NSEvent.mouseLocation(), inView: nil)
    }
}

// MARK: - Binding
private extension MenuManager {
    private func bind() {
        // Realm Notification
        clipToken = realm.objects(CPYClip.self)
                        .addNotificationBlock { [unowned self] _ in
                            self.createClipMenu()
                        }
        snippetToken = realm.objects(CPYFolder.self)
                        .addNotificationBlock { [unowned self] _ in
                            self.createClipMenu()
                        }
        // Menu icon
        defaults.rx_observe(Int.self, Constants.UserDefaults.showStatusItem)
            .filterNil()
            .subscribeNext { [unowned self] key in
                self.changeStatusItem(StatusType(rawValue: key) ?? .Black)
            }.addDisposableTo(rx_disposeBag)
        // Clear history menu
        defaults.rx_observe(Bool.self, Constants.UserDefaults.addClearHistoryMenuItem, options: [.New])
            .filterNil()
            .subscribeNext { [unowned self] enabled in
                self.createClipMenu()
            }.addDisposableTo(rx_disposeBag)
        // Sort clips
        defaults.rx_observe(Bool.self, Constants.UserDefaults.reorderClipsAfterPasting, options: [.New])
            .filterNil()
            .subscribeNext { [unowned self] enabled in
                self.clipResults = self.realm.objects(CPYClip.self).sorted("updateTime", ascending: !enabled)
                self.createClipMenu()
            }.addDisposableTo(rx_disposeBag)
        // Edit snippets
        notificationCenter.rx_notification(Constants.Notification.closeSnippetEditor)
            .subscribeNext { [unowned self] notification in
                self.createClipMenu()
            }.addDisposableTo(rx_disposeBag)
    }
}

// MARK: - Menus
private extension MenuManager {
    private func createClipMenu() {
        clipMenu = NSMenu(title: Constants.Application.name)
        historyMenu = NSMenu(title: Constants.Menu.history)
        snippetMenu = NSMenu(title: Constants.Menu.snippet)

        addHistoryItems(clipMenu!)
        addHistoryItems(historyMenu!)

        addSnippetItems(clipMenu!, separateMenu: true)
        addSnippetItems(snippetMenu!, separateMenu: false)

        clipMenu?.addItem(NSMenuItem.separatorItem())

        if defaults.boolForKey(Constants.UserDefaults.addClearHistoryMenuItem) {
            clipMenu?.addItem(NSMenuItem(title: LocalizedString.ClearHistory.value, action: #selector(AppDelegate.clearAllHistory)))
        }

        clipMenu?.addItem(NSMenuItem(title: LocalizedString.EditSnippets.value, action: #selector(AppDelegate.showSnippetEditorWindow)))
        clipMenu?.addItem(NSMenuItem(title: LocalizedString.Preference.value, action: #selector(AppDelegate.showPreferenceWindow)))
        clipMenu?.addItem(NSMenuItem.separatorItem())
        clipMenu?.addItem(NSMenuItem(title: LocalizedString.QuitClipy.value, action: #selector(AppDelegate.terminateApplication)))

        statusItem?.menu = clipMenu
    }

    private func menuItemTitle(title: String, listNumber: NSInteger, isMarkWithNumber: Bool) -> String {
        return (isMarkWithNumber) ? "\(listNumber) \(title)" : title
    }

    private func makeSubmenuItem(count: Int, start: Int, end: Int, numberOfItems: Int) -> NSMenuItem {
        var count = count
        if start == 0 {
            count = count - 1
        }
        var lastNumber = count + numberOfItems
        if end < lastNumber {
            lastNumber = end
        }
        let menuItemTitle = "\(count + 1) - \(lastNumber)"
        return makeSubmenuItem(menuItemTitle)
    }

    private func makeSubmenuItem(title: String) -> NSMenuItem {
        let subMenu = NSMenu(title: "")
        let subMenuItem = NSMenuItem(title: title, action: nil)
        subMenuItem.submenu = subMenu
        subMenuItem.image = (defaults.boolForKey(Constants.UserDefaults.showIconInTheMenu)) ? folderIcon : nil
        return subMenuItem
    }

    private func incrementListNumber(listNumber: NSInteger, max: NSInteger, start: NSInteger) -> NSInteger {
        var listNumber = listNumber + 1
        if listNumber == max && max == 10 && start == 1 {
            listNumber = 0
        }
        return listNumber
    }

    private func trimTitle(title: String?) -> String {
        if title == nil { return "" }
        let theString = title!.stringByTrimmingCharactersInSet(NSCharacterSet.whitespaceAndNewlineCharacterSet()) as NSString

        let aRange = NSRange(location: 0, length: 0)
        var lineStart = 0, lineEnd = 0, contentsEnd = 0
        theString.getLineStart(&lineStart, end: &lineEnd, contentsEnd: &contentsEnd, forRange: aRange)

        var titleString = (lineEnd == theString.length) ? theString : theString.substringToIndex(contentsEnd)

        var maxMenuItemTitleLength = defaults.integerForKey(Constants.UserDefaults.maxMenuItemTitleLength)
        if maxMenuItemTitleLength < shortenSymbol.characters.count {
            maxMenuItemTitleLength = shortenSymbol.characters.count
        }
        if titleString.length > maxMenuItemTitleLength {
            titleString = titleString.substringToIndex(maxMenuItemTitleLength - shortenSymbol.characters.count) + shortenSymbol
        }

        return titleString as String
    }
}

// MARK: - Clips
private extension MenuManager {
    private func addHistoryItems(menu: NSMenu) {
        let placeInLine = defaults.integerForKey(Constants.UserDefaults.numberOfItemsPlaceInline)
        let placeInsideFolder = defaults.integerForKey(Constants.UserDefaults.numberOfItemsPlaceInsideFolder)
        let maxHistory = defaults.integerForKey(Constants.UserDefaults.maxHistorySize)

        // History title
        let labelItem = NSMenuItem(title: LocalizedString.History.value, action: nil)
        labelItem.enabled = false
        menu.addItem(labelItem)

        // History
        let firstIndex = firstIndexOfMenuItems()
        var listNumber = firstIndex
        var subMenuCount = placeInLine
        var subMenuIndex = 1 + placeInLine

        let currentSize = Int(clipResults.count)
        var i = 0
        for clip in clipResults {
            if placeInLine < 1 || placeInLine - 1 < i {
                // Folder
                if i == subMenuCount {
                    let subMenuItem = makeSubmenuItem(subMenuCount, start: firstIndex, end: currentSize, numberOfItems: placeInsideFolder)
                    menu.addItem(subMenuItem)
                    listNumber = firstIndex
                }

                // Clip
                if let subMenu = menu.itemAtIndex(subMenuIndex)?.submenu {
                    let menuItem = makeClipMenuItem(clip, index: i, listNumber: listNumber)
                    subMenu.addItem(menuItem)
                    listNumber = incrementListNumber(listNumber, max: placeInsideFolder, start: firstIndex)
                }
            } else {
                // Clip
                let menuItem = makeClipMenuItem(clip, index: i, listNumber: listNumber)
                menu.addItem(menuItem)
                listNumber = incrementListNumber(listNumber, max: placeInLine, start: firstIndex)
            }

            i += 1
            if i == subMenuCount + placeInsideFolder {
                subMenuCount += placeInsideFolder
                subMenuIndex += 1
            }

            if maxHistory <= i { break }
        }
    }

    private func makeClipMenuItem(clip: CPYClip, index: Int, listNumber: Int) -> NSMenuItem {
        let isMarkWithNumber = defaults.boolForKey(Constants.UserDefaults.menuItemsAreMarkedWithNumbers)
        let isShowToolTip = defaults.boolForKey(Constants.UserDefaults.showToolTipOnMenuItem)
        let isShowImage = defaults.boolForKey(Constants.UserDefaults.showImageInTheMenu)
        let addNumbericKeyEquivalents = defaults.boolForKey(Constants.UserDefaults.addNumericKeyEquivalents)

        var keyEquivalent = ""

        if addNumbericKeyEquivalents && (index <= kMaxKeyEquivalents) {
            let isStartFromZero = defaults.boolForKey(Constants.UserDefaults.menuItemsTitleStartWithZero)

            var shortCutNumber = (isStartFromZero) ? index : index + 1
            if shortCutNumber == kMaxKeyEquivalents {
                shortCutNumber = 0
            }
            keyEquivalent = "\(shortCutNumber)"
        }

        let primaryPboardType = clip.primaryType
        let clipString = clip.title
        let title = trimTitle(clipString)
        let titleWithMark = menuItemTitle(title, listNumber: listNumber, isMarkWithNumber: isMarkWithNumber)

        let menuItem = NSMenuItem(title: titleWithMark, action: #selector(AppDelegate.selectClipMenuItem(_:)), keyEquivalent: keyEquivalent)
        menuItem.representedObject = clip.dataHash

        if isShowToolTip {
            let maxLengthOfToolTip = defaults.integerForKey(Constants.UserDefaults.maxLengthOfToolTip)
            let toIndex = (clipString.characters.count < maxLengthOfToolTip) ? clipString.characters.count : maxLengthOfToolTip
            menuItem.toolTip = (clipString as NSString).substringToIndex(toIndex)
        }

        if primaryPboardType == NSTIFFPboardType {
            menuItem.title = menuItemTitle("(Image)", listNumber: listNumber, isMarkWithNumber: isMarkWithNumber)
        } else if primaryPboardType == NSPDFPboardType {
            menuItem.title = menuItemTitle("(PDF)", listNumber: listNumber, isMarkWithNumber: isMarkWithNumber)
        } else if primaryPboardType == NSFilenamesPboardType && title.isEmpty {
            menuItem.title = menuItemTitle("(Filenames)", listNumber: listNumber, isMarkWithNumber: isMarkWithNumber)
        }

        if !clip.thumbnailPath.isEmpty && isShowImage {
            PINCache.sharedCache().objectForKey(clip.thumbnailPath, block: { [weak menuItem] (cache, key, object) in
                if let image = object as? NSImage {
                    menuItem?.image = image
                }
            })
        }

        return menuItem
    }
}

// MARK: - Snippets
private extension MenuManager {
    private func addSnippetItems(menu: NSMenu, separateMenu: Bool) {
        if folderResults.count == 0 { return }
        if separateMenu {
            menu.addItem(NSMenuItem.separatorItem())
        }

        // Snippet title
        let labelItem = NSMenuItem(title: LocalizedString.Snippet.value, action: nil)
        labelItem.enabled = false
        menu.addItem(labelItem)

        var subMenuIndex = menu.numberOfItems - 1
        let firstIndex = firstIndexOfMenuItems()

        folderResults
            .filter { $0.enable }
            .forEach { folder in
                let folderTitle = folder.title
                let subMenuItem = makeSubmenuItem(folderTitle)
                menu.addItem(subMenuItem)
                subMenuIndex += 1

                var i = firstIndex
                folder.snippets
                    .sorted("index", ascending: true)
                    .filter { $0.enable }
                    .forEach { snippet in
                        let subMenuItem = makeSnippetMenuItem(snippet, listNumber: i)
                        if let subMenu = menu.itemAtIndex(subMenuIndex)?.submenu {
                            subMenu.addItem(subMenuItem)
                            i += 1
                        }
                    }
            }
    }

    private func makeSnippetMenuItem(snippet: CPYSnippet, listNumber: Int) -> NSMenuItem {
        let isMarkWithNumber = defaults.boolForKey(Constants.UserDefaults.menuItemsAreMarkedWithNumbers)
        let isShowIcon = defaults.boolForKey(Constants.UserDefaults.showIconInTheMenu)

        let title = trimTitle(snippet.title)
        let titleWithMark = menuItemTitle(title, listNumber: listNumber, isMarkWithNumber: isMarkWithNumber)

        let menuItem = NSMenuItem(title: titleWithMark, action: #selector(AppDelegate.selectSnippetMenuItem(_:)), keyEquivalent: "")
        menuItem.representedObject = snippet.identifier
        menuItem.toolTip = snippet.content
        menuItem.image = (isShowIcon) ? snippetIcon : nil

        return menuItem
    }
}

// MARK: - Status Item
private extension MenuManager {
    private func changeStatusItem(type: StatusType) {
        removeStatusItem()
        if type == .None { return }

        let image: NSImage?
        switch type {
        case .Black:
            image = NSImage(assetIdentifier: .MenuBlack)
        case .White:
            image = NSImage(assetIdentifier: .MenuWhite)
        case .None: return
        }
        image?.template = true

        statusItem = NSStatusBar.systemStatusBar().statusItemWithLength(-1)
        statusItem?.image = image
        statusItem?.highlightMode = true
        statusItem?.toolTip = "\(Constants.Application.name)\(NSBundle.mainBundle().appVersion ?? "")"
        statusItem?.menu = clipMenu
    }

    private func removeStatusItem() {
        if let item = statusItem {
            NSStatusBar.systemStatusBar().removeStatusItem(item)
            statusItem = nil
        }
    }
}

// MARK: - Settings
private extension MenuManager {
    private func firstIndexOfMenuItems() -> NSInteger {
        return defaults.boolForKey(Constants.UserDefaults.menuItemsTitleStartWithZero) ? 0 : 1
    }
}

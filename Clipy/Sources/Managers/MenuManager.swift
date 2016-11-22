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
import RxOptional

final class MenuManager: NSObject {
    // MARK: - Properties
    static let sharedManager = MenuManager()
    // Menus
    fileprivate var clipMenu: NSMenu?
    fileprivate var historyMenu: NSMenu?
    fileprivate var snippetMenu: NSMenu?
    // StatusMenu
    fileprivate var statusItem: NSStatusItem?
    // Icon Cache
    fileprivate let folderIcon = NSImage(assetIdentifier: .IconFolder)
    fileprivate let snippetIcon = NSImage(assetIdentifier: .IconText)
    // Other
    fileprivate let disposeBag = DisposeBag()
    fileprivate let defaults = UserDefaults.standard
    fileprivate let notificationCenter = NotificationCenter.default
    fileprivate let kMaxKeyEquivalents = 10
    fileprivate let shortenSymbol = "..."
    // Realm Results
    fileprivate let realm = try! Realm()
    fileprivate var clipResults: Results<CPYClip>
    fileprivate let folderResults: Results<CPYFolder>
    // Realm Token
    fileprivate var clipToken: NotificationToken?
    fileprivate var snippetToken: NotificationToken?

    // MARK: - Enum Values
    enum StatusType: Int {
        case none, black, white
    }

    // MARK: - Initialize
    override init() {
        clipResults = realm.objects(CPYClip.self).sorted(byProperty: "updateTime", ascending: !defaults.bool(forKey: Constants.UserDefaults.reorderClipsAfterPasting))
        folderResults = realm.objects(CPYFolder.self).sorted(byProperty: "index", ascending: true)
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
        menu?.popUp(positioning: nil, at: NSEvent.mouseLocation(), in: nil)
    }

    func popUpSnippetFolder(_ folder: CPYFolder) {
        let folderMenu = NSMenu(title: folder.title)
        // Folder title
        let labelItem = NSMenuItem(title: folder.title, action: nil)
        labelItem.isEnabled = false
        folderMenu.addItem(labelItem)
        // Snippets
        var index = firstIndexOfMenuItems()
        folder.snippets
            .sorted(byProperty: "index", ascending: true)
            .filter { $0.enable }
            .forEach { snippet in
                let subMenuItem = makeSnippetMenuItem(snippet, listNumber: index)
                folderMenu.addItem(subMenuItem)
                index += 1
            }
        folderMenu.popUp(positioning: nil, at: NSEvent.mouseLocation(), in: nil)
    }
}

// MARK: - Binding
fileprivate extension MenuManager {
    fileprivate func bind() {
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
        defaults.rx.observe(Int.self, Constants.UserDefaults.showStatusItem)
            .filterNil()
            .subscribe(onNext: { [unowned self] key in
                self.changeStatusItem(StatusType(rawValue: key) ?? .black)
            }).addDisposableTo(disposeBag)
        // Clear history menu
        defaults.rx.observe(Bool.self, Constants.UserDefaults.addClearHistoryMenuItem, options: [.new])
            .filterNil()
            .subscribe(onNext: { [unowned self] enabled in
                self.createClipMenu()
            }).addDisposableTo(disposeBag)
        // Sort clips
        defaults.rx.observe(Bool.self, Constants.UserDefaults.reorderClipsAfterPasting, options: [.new])
            .filterNil()
            .subscribe(onNext: { [unowned self] enabled in
                self.clipResults = self.realm.objects(CPYClip.self).sorted(byProperty: "updateTime", ascending: !enabled)
                self.createClipMenu()
            }).addDisposableTo(disposeBag)
        // Edit snippets
        notificationCenter.rx.notification(Notification.Name(rawValue: Constants.Notification.closeSnippetEditor))
            .subscribe(onNext: { [unowned self] notification in
                self.createClipMenu()
            }).addDisposableTo(disposeBag)
    }
}

// MARK: - Menus
fileprivate extension MenuManager {
    fileprivate func createClipMenu() {
        clipMenu = NSMenu(title: Constants.Application.name)
        historyMenu = NSMenu(title: Constants.Menu.history)
        snippetMenu = NSMenu(title: Constants.Menu.snippet)

        addHistoryItems(clipMenu!)
        addHistoryItems(historyMenu!)

        addSnippetItems(clipMenu!, separateMenu: true)
        addSnippetItems(snippetMenu!, separateMenu: false)

        clipMenu?.addItem(NSMenuItem.separator())

        if defaults.bool(forKey: Constants.UserDefaults.addClearHistoryMenuItem) {
            clipMenu?.addItem(NSMenuItem(title: LocalizedString.ClearHistory.value, action: #selector(AppDelegate.clearAllHistory)))
        }

        clipMenu?.addItem(NSMenuItem(title: LocalizedString.EditSnippets.value, action: #selector(AppDelegate.showSnippetEditorWindow)))
        clipMenu?.addItem(NSMenuItem(title: LocalizedString.Preference.value, action: #selector(AppDelegate.showPreferenceWindow)))
        clipMenu?.addItem(NSMenuItem.separator())
        clipMenu?.addItem(NSMenuItem(title: LocalizedString.QuitClipy.value, action: #selector(AppDelegate.terminateApplication)))

        statusItem?.menu = clipMenu
    }

    fileprivate func menuItemTitle(_ title: String, listNumber: NSInteger, isMarkWithNumber: Bool) -> String {
        return (isMarkWithNumber) ? "\(listNumber) \(title)" : title
    }

    fileprivate func makeSubmenuItem(_ count: Int, start: Int, end: Int, numberOfItems: Int) -> NSMenuItem {
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

    fileprivate func makeSubmenuItem(_ title: String) -> NSMenuItem {
        let subMenu = NSMenu(title: "")
        let subMenuItem = NSMenuItem(title: title, action: nil)
        subMenuItem.submenu = subMenu
        subMenuItem.image = (defaults.bool(forKey: Constants.UserDefaults.showIconInTheMenu)) ? folderIcon : nil
        return subMenuItem
    }

    fileprivate func incrementListNumber(_ listNumber: NSInteger, max: NSInteger, start: NSInteger) -> NSInteger {
        var listNumber = listNumber + 1
        if listNumber == max && max == 10 && start == 1 {
            listNumber = 0
        }
        return listNumber
    }

    fileprivate func trimTitle(_ title: String?) -> String {
        if title == nil { return "" }
        let theString = title!.trimmingCharacters(in: .whitespacesAndNewlines) as NSString

        let aRange = NSRange(location: 0, length: 0)
        var lineStart = 0, lineEnd = 0, contentsEnd = 0
        theString.getLineStart(&lineStart, end: &lineEnd, contentsEnd: &contentsEnd, for: aRange)

        var titleString = (lineEnd == theString.length) ? theString as String : theString.substring(to: contentsEnd)

        var maxMenuItemTitleLength = defaults.integer(forKey: Constants.UserDefaults.maxMenuItemTitleLength)
        if maxMenuItemTitleLength < shortenSymbol.characters.count {
            maxMenuItemTitleLength = shortenSymbol.characters.count
        }

        if titleString.utf16.count > maxMenuItemTitleLength {
            titleString = (titleString as NSString).substring(to: maxMenuItemTitleLength - shortenSymbol.characters.count) + shortenSymbol
        }

        return titleString as String
    }
}

// MARK: - Clips
fileprivate extension MenuManager {
    fileprivate func addHistoryItems(_ menu: NSMenu) {
        let placeInLine = defaults.integer(forKey: Constants.UserDefaults.numberOfItemsPlaceInline)
        let placeInsideFolder = defaults.integer(forKey: Constants.UserDefaults.numberOfItemsPlaceInsideFolder)
        let maxHistory = defaults.integer(forKey: Constants.UserDefaults.maxHistorySize)

        // History title
        let labelItem = NSMenuItem(title: LocalizedString.History.value, action: nil)
        labelItem.isEnabled = false
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
                if let subMenu = menu.item(at: subMenuIndex)?.submenu {
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

    fileprivate func makeClipMenuItem(_ clip: CPYClip, index: Int, listNumber: Int) -> NSMenuItem {
        let isMarkWithNumber = defaults.bool(forKey: Constants.UserDefaults.menuItemsAreMarkedWithNumbers)
        let isShowToolTip = defaults.bool(forKey: Constants.UserDefaults.showToolTipOnMenuItem)
        let isShowImage = defaults.bool(forKey: Constants.UserDefaults.showImageInTheMenu)
        let isShowColorCode = defaults.bool(forKey: Constants.UserDefaults.showColorPreviewInTheMenu)
        let addNumbericKeyEquivalents = defaults.bool(forKey: Constants.UserDefaults.addNumericKeyEquivalents)

        var keyEquivalent = ""

        if addNumbericKeyEquivalents && (index <= kMaxKeyEquivalents) {
            let isStartFromZero = defaults.bool(forKey: Constants.UserDefaults.menuItemsTitleStartWithZero)

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
            let maxLengthOfToolTip = defaults.integer(forKey: Constants.UserDefaults.maxLengthOfToolTip)
            let toIndex = (clipString.characters.count < maxLengthOfToolTip) ? clipString.characters.count : maxLengthOfToolTip
            menuItem.toolTip = (clipString as NSString).substring(to: toIndex)
        }

        if primaryPboardType == NSTIFFPboardType {
            menuItem.title = menuItemTitle("(Image)", listNumber: listNumber, isMarkWithNumber: isMarkWithNumber)
        } else if primaryPboardType == NSPDFPboardType {
            menuItem.title = menuItemTitle("(PDF)", listNumber: listNumber, isMarkWithNumber: isMarkWithNumber)
        } else if primaryPboardType == NSFilenamesPboardType && title.isEmpty {
            menuItem.title = menuItemTitle("(Filenames)", listNumber: listNumber, isMarkWithNumber: isMarkWithNumber)
        }

        if !clip.thumbnailPath.isEmpty && !clip.isColorCode && isShowImage {
            PINCache.shared().object(forKey: clip.thumbnailPath, block: { [weak menuItem] (_, _, object) in
                DispatchQueue.main.async {
                    menuItem?.image = object as? NSImage
                }
            })
        }
        if !clip.thumbnailPath.isEmpty && clip.isColorCode && isShowColorCode {
            PINCache.shared().object(forKey: clip.thumbnailPath, block: { [weak menuItem] (_, _, object) in
                DispatchQueue.main.async {
                    menuItem?.image = object as? NSImage
                }
            })
        }


        return menuItem
    }
}

// MARK: - Snippets
private extension MenuManager {
    func addSnippetItems(_ menu: NSMenu, separateMenu: Bool) {
        if folderResults.count == 0 { return }
        if separateMenu {
            menu.addItem(NSMenuItem.separator())
        }

        // Snippet title
        let labelItem = NSMenuItem(title: LocalizedString.Snippet.value, action: nil)
        labelItem.isEnabled = false
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
                    .sorted(byProperty: "index", ascending: true)
                    .filter { $0.enable }
                    .forEach { snippet in
                        let subMenuItem = makeSnippetMenuItem(snippet, listNumber: i)
                        if let subMenu = menu.item(at: subMenuIndex)?.submenu {
                            subMenu.addItem(subMenuItem)
                            i += 1
                        }
                    }
            }
    }

    func makeSnippetMenuItem(_ snippet: CPYSnippet, listNumber: Int) -> NSMenuItem {
        let isMarkWithNumber = defaults.bool(forKey: Constants.UserDefaults.menuItemsAreMarkedWithNumbers)
        let isShowIcon = defaults.bool(forKey: Constants.UserDefaults.showIconInTheMenu)

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
    func changeStatusItem(_ type: StatusType) {
        removeStatusItem()
        if type == .none { return }

        let image: NSImage?
        switch type {
        case .black:
            image = NSImage(assetIdentifier: .MenuBlack)
        case .white:
            image = NSImage(assetIdentifier: .MenuWhite)
        case .none: return
        }
        image?.isTemplate = true

        statusItem = NSStatusBar.system().statusItem(withLength: -1)
        statusItem?.image = image
        statusItem?.highlightMode = true
        statusItem?.toolTip = "\(Constants.Application.name)\(Bundle.main.appVersion ?? "")"
        statusItem?.menu = clipMenu
    }

    func removeStatusItem() {
        if let item = statusItem {
            NSStatusBar.system().removeStatusItem(item)
            statusItem = nil
        }
    }
}

// MARK: - Settings
private extension MenuManager {
    func firstIndexOfMenuItems() -> NSInteger {
        return defaults.bool(forKey: Constants.UserDefaults.menuItemsTitleStartWithZero) ? 0 : 1
    }
}

//
//  MenuService.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/11/23.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation
import Cocoa
import RealmSwift
import RxRealm
import RxSwift
import RxOptional
import PINCache

final class MenuService {

    // MARK: - Properties
    static let shared = MenuService()

    // Main menu
    let mainMenu: Observable<NSMenu>
    fileprivate let _mainMenu = Variable<NSMenu?>(nil) // swiftlint:disable:this variable_name
    // History menu
    fileprivate let historyMenu = Variable<NSMenu?>(nil)
    // Snippet menu
    fileprivate let snippetMenu = Variable<NSMenu?>(nil)
    // Icon cache
    fileprivate let folderIcon: NSImage = {
        let image = NSImage(assetIdentifier: .IconFolder)
        image.isTemplate = true
        image.size = NSSize(width: 15, height: 13)
        return image
    }()
    fileprivate let snippetIcon: NSImage = {
        let image = NSImage(assetIdentifier: .IconText)
        image.isTemplate = true
        image.size = NSSize(width: 12, height: 13)
        return image
    }()
    // Disposes
    fileprivate let disposeBag = DisposeBag()
    fileprivate var clipDisposeBag = DisposeBag()
    // Objects
    fileprivate let defaults = UserDefaults.standard
    fileprivate let notificationCenter = NotificationCenter.default
    fileprivate let maxKeyEquivalents = 10
    fileprivate let shortenSymbol = "..."

    // MARK: - Initialize
    init() {
        // Main menu
        mainMenu = _mainMenu.asObservable().filterNil()
        // Observe clip histories
        defaults.rx.observe(Bool.self, Constants.UserDefaults.reorderClipsAfterPasting)
            .filterNil()
            .subscribe(onNext: { [weak self] ascending in
                self?.observeClips(ascending: !ascending)
            })
            .addDisposableTo(disposeBag)
        // Observe snippet menu
        let realm = try! Realm()
        let folders = realm.objects(CPYFolder.self).sorted(byProperty: "index", ascending: true)
        Observable.from(folders)
            .map { [weak self] in self?.buildMenu(with: $0) }
            .bindTo(snippetMenu)
            .addDisposableTo(disposeBag)
        // Create main menu
        Observable.of(
            historyMenu.asObservable().map { _ in },
            snippetMenu.asObservable().map { _ in },
            defaults.rx.observe(Bool.self, Constants.UserDefaults.addClearHistoryMenuItem, options: [.new]).map { _ in }
        )
        .merge()
        .skip(1)
        .map { _ in NSMenu(title: Constants.Application.name) }
        .withLatestFrom(historyMenu.asObservable()) { ($0, $1) }
        .map { [unowned self] in self.mergeHistoryMenu($0.0, historyMenu: $0.1) }
        .withLatestFrom(snippetMenu.asObservable()) { ($0, $1) }
        .map { [unowned self] in self.mergeSnippetMenu($0.0, snippetMenu: $0.1) }
        .map { [unowned self] in self.addOptionMenu(to: $0) }
        .bindTo(_mainMenu)
        .addDisposableTo(disposeBag)
    }

    private func observeClips(ascending: Bool) {
        clipDisposeBag = DisposeBag()
        let realm = try! Realm()
        let clips = realm.objects(CPYClip.self).sorted(byProperty: "updateTime", ascending: ascending)
        Observable.from(clips)
            .map { [weak self]  in self?.buildMenu(with: $0) }
            .bindTo(historyMenu)
            .addDisposableTo(clipDisposeBag)
    }

}

// MARK: - Popup Menu
extension MenuService {
    func popupMenu(with type: MenuType) {
        let menu: NSMenu?
        switch type {
        case .main:
            menu = _mainMenu.value
        case .history:
            menu = historyMenu.value
        case .snippet:
            menu = snippetMenu.value
        }
        menu?.popUp(positioning: nil, at: NSEvent.mouseLocation(), in: nil)
    }

    func popupSnippet(with folder: CPYFolder) {
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
                let subMenuItem = buildMenuItem(with: snippet, listNumber: index)
                folderMenu.addItem(subMenuItem)
                index += 1
        }
        folderMenu.popUp(positioning: nil, at: NSEvent.mouseLocation(), in: nil)
    }
}

// MARK: - Main Menus
fileprivate extension MenuService {
    fileprivate func mergeHistoryMenu(_ mainMenu: NSMenu, historyMenu: NSMenu?) -> NSMenu {
        historyMenu?.items
            .flatMap { $0.copy() as? NSMenuItem }
            .forEach { mainMenu.addItem($0) }
        return mainMenu
    }

    fileprivate func mergeSnippetMenu(_ mainMenu: NSMenu, snippetMenu: NSMenu?) -> NSMenu {
        if snippetMenu?.items.isEmpty == false {
            mainMenu.addItem(NSMenuItem.separator())
        }
        snippetMenu?.items
            .flatMap { $0.copy() as? NSMenuItem }
            .forEach { mainMenu.addItem($0) }
        return mainMenu
    }

    fileprivate func addOptionMenu(to mainMenu: NSMenu) -> NSMenu {
        mainMenu.addItem(NSMenuItem.separator())
        if defaults.bool(forKey: Constants.UserDefaults.addClearHistoryMenuItem) {
            mainMenu.addItem(NSMenuItem(title: LocalizedString.ClearHistory.value, action: #selector(AppDelegate.clearAllHistory)))
        }
        mainMenu.addItem(NSMenuItem(title: LocalizedString.EditSnippets.value, action: #selector(AppDelegate.showSnippetEditorWindow)))
        mainMenu.addItem(NSMenuItem(title: LocalizedString.Preference.value, action: #selector(AppDelegate.showPreferenceWindow)))
        mainMenu.addItem(NSMenuItem.separator())
        mainMenu.addItem(NSMenuItem(title: LocalizedString.QuitClipy.value, action: #selector(AppDelegate.terminateApplication)))
        return mainMenu
    }
}

// MARK: - History Menus
fileprivate extension MenuService {
    fileprivate func buildMenu(with clips: Results<CPYClip>) -> NSMenu {
        let menu = NSMenu(title: Constants.Menu.history)
        // History menu settings
        let placeInLine = defaults.integer(forKey: Constants.UserDefaults.numberOfItemsPlaceInline)
        let placeInsideFolder = defaults.integer(forKey: Constants.UserDefaults.numberOfItemsPlaceInsideFolder)
        let maxHistory = defaults.integer(forKey: Constants.UserDefaults.maxHistorySize)
        // History menu title
        let labelItem = NSMenuItem(title: LocalizedString.History.value, action: nil)
        labelItem.isEnabled = false
        menu.addItem(labelItem)

        // Create history menus
        let firstIndex = firstIndexOfMenuItems()
        var listNumber = firstIndex
        var subMenuCount = placeInLine
        var subMenuIndex = 1 + placeInLine

        for (index, clip) in clips.enumerated() {
            if maxHistory <= index { break }

            if placeInLine < 1 || placeInLine - 1 < index {
                // Folder menu
                if index == subMenuCount {
                    let subMenuItem = buildSubmenuItem(with: subMenuCount, start: firstIndex, end: clips.count, numberOfItems: placeInsideFolder)
                    menu.addItem(subMenuItem)
                    listNumber = firstIndex
                }
                // history menu inside the folder
                if let submenu = menu.item(at: subMenuIndex)?.submenu {
                    let menuItem = buildMenuItem(with: clip, index: index, listNumber: listNumber)
                    submenu.addItem(menuItem)
                    listNumber = incrementListNumber(listNumber, max: placeInsideFolder, start: firstIndex)
                }
            } else {
                // Inline history menu outside the folder
                let menuItem = buildMenuItem(with: clip, index: index, listNumber: listNumber)
                menu.addItem(menuItem)
                listNumber = incrementListNumber(listNumber, max: placeInLine, start: firstIndex)
            }
            if index + 1 == subMenuCount + placeInsideFolder {
                subMenuCount += placeInsideFolder
                subMenuIndex += 1
            }
        }

        return menu
    }

    fileprivate func buildMenuItem(with clip: CPYClip, index: Int, listNumber: Int) -> NSMenuItem {
        let isMarkWithNumber = defaults.bool(forKey: Constants.UserDefaults.menuItemsAreMarkedWithNumbers)
        let isShowToolTip = defaults.bool(forKey: Constants.UserDefaults.showToolTipOnMenuItem)
        let isShowImage = defaults.bool(forKey: Constants.UserDefaults.showImageInTheMenu)
        let isShowColorCode = defaults.bool(forKey: Constants.UserDefaults.showColorPreviewInTheMenu)
        let addNumbericKeyEquivalents = defaults.bool(forKey: Constants.UserDefaults.addNumericKeyEquivalents)

        // Calculation KeyEquivalent
        var keyEquivalent = ""
        if addNumbericKeyEquivalents && (index <= maxKeyEquivalents) {
            let isStartFromZero = defaults.bool(forKey: Constants.UserDefaults.menuItemsTitleStartWithZero)
            var shortCutNumber = (isStartFromZero) ? index : index + 1
            if shortCutNumber == maxKeyEquivalents {
                shortCutNumber = 0
            }
            keyEquivalent = "\(shortCutNumber)"
        }

        let primaryPboardType = clip.primaryType
        let clipString = clip.title
        let title = trim(with: clipString)
        let titleWithMark = menuItemTitle(with: title, listNumber: listNumber, isMarkWithNumber: isMarkWithNumber)

        let menuItem = NSMenuItem(title: titleWithMark, action: #selector(AppDelegate.selectClipMenuItem(_:)), keyEquivalent: keyEquivalent)
        menuItem.representedObject = clip.dataHash

        if isShowToolTip {
            let maxLengthOfToolTip = defaults.integer(forKey: Constants.UserDefaults.maxLengthOfToolTip)
            let toIndex = (clipString.characters.count < maxLengthOfToolTip) ? clipString.characters.count : maxLengthOfToolTip
            menuItem.toolTip = (clipString as NSString).substring(to: toIndex)
        }

        if primaryPboardType == NSTIFFPboardType {
            menuItem.title = menuItemTitle(with: "(Image)", listNumber: listNumber, isMarkWithNumber: isMarkWithNumber)
        } else if primaryPboardType == NSPDFPboardType {
            menuItem.title = menuItemTitle(with: "(PDF)", listNumber: listNumber, isMarkWithNumber: isMarkWithNumber)
        } else if primaryPboardType == NSFilenamesPboardType && title.isEmpty {
            menuItem.title = menuItemTitle(with: "(Filenames)", listNumber: listNumber, isMarkWithNumber: isMarkWithNumber)
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

// MARK: - Snippet Menus
fileprivate extension MenuService {
    fileprivate func buildMenu(with folders: Results<CPYFolder>) -> NSMenu {
        let menu = NSMenu(title: Constants.Menu.snippet)
        if folders.isEmpty { return menu }

        // Snippet menu title
        let labelItem = NSMenuItem(title: LocalizedString.Snippet.value, action: nil)
        labelItem.isEnabled = false
        menu.addItem(labelItem)

        // Create snippet menus
        var subMenuIndex = menu.numberOfItems - 1
        let firstIndex = firstIndexOfMenuItems()

        folders
            .filter { $0.enable }
            .forEach { folder in
                let folderTitle = folder.title
                let subMenuItem = buildSubmenuItem(with: folderTitle)
                menu.addItem(subMenuItem)
                subMenuIndex += 1

                var i = firstIndex
                folder.snippets
                    .sorted(byProperty: "index", ascending: true)
                    .filter { $0.enable }
                    .forEach { snippet in
                        let subMenuItem = buildMenuItem(with: snippet, listNumber: i)
                        if let subMenu = menu.item(at: subMenuIndex)?.submenu {
                            subMenu.addItem(subMenuItem)
                            i += 1
                        }
                }
            }

        return menu
    }

    fileprivate func buildMenuItem(with snippet: CPYSnippet, listNumber: Int) -> NSMenuItem {
        let isMarkWithNumber = defaults.bool(forKey: Constants.UserDefaults.menuItemsAreMarkedWithNumbers)
        let isShowIcon = defaults.bool(forKey: Constants.UserDefaults.showIconInTheMenu)

        let title = trim(with: snippet.title)
        let titleWithMark = menuItemTitle(with: title, listNumber: listNumber, isMarkWithNumber: isMarkWithNumber)

        let menuItem = NSMenuItem(title: titleWithMark, action: #selector(AppDelegate.selectSnippetMenuItem(_:)), keyEquivalent: "")
        menuItem.representedObject = snippet.identifier
        menuItem.toolTip = snippet.content
        menuItem.image = (isShowIcon) ? snippetIcon : nil

        return menuItem
    }
}

// MARK: - Menus
fileprivate extension MenuService {
    fileprivate func buildSubmenuItem(with count: Int, start: Int, end: Int, numberOfItems: Int) -> NSMenuItem {
        let count = (start == 0) ? count - 1 : count
        var lastNumber = count + numberOfItems
        if end < lastNumber {
            lastNumber = end
        }
        let menuItemTitle = "\(count + 1) - \(lastNumber)"
        return buildSubmenuItem(with: menuItemTitle)
    }

    fileprivate func buildSubmenuItem(with title: String) -> NSMenuItem {
        let subMenu = NSMenu(title: "")
        let subMenuItem = NSMenuItem(title: title, action: nil)
        subMenuItem.submenu = subMenu
        subMenuItem.image = (defaults.bool(forKey: Constants.UserDefaults.showIconInTheMenu)) ? folderIcon : nil
        return subMenuItem
    }

    fileprivate func trim(with title: String) -> String {
        if title.isEmpty { return title }

        let theString = title.trimmingCharacters(in: .whitespacesAndNewlines) as NSString
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

    fileprivate func incrementListNumber(_ listNumber: NSInteger, max: NSInteger, start: NSInteger) -> NSInteger {
        var listNumber = listNumber + 1
        if listNumber == max && max == 10 && start == 1 {
            listNumber = 0
        }
        return listNumber
    }

    fileprivate func menuItemTitle(with title: String, listNumber: NSInteger, isMarkWithNumber: Bool) -> String {
        return (isMarkWithNumber) ? "\(listNumber) \(title)" : title
    }
}

// MARK: - Settings
fileprivate extension MenuService {
    fileprivate func firstIndexOfMenuItems() -> Int {
        return defaults.bool(forKey: Constants.UserDefaults.menuItemsTitleStartWithZero) ? 0 : 1
    }
}

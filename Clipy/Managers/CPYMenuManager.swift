//
//  CPYMenuManager.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/21.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import PINCache
import Realm
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
    private let SHORTEN_SYMBOL = "..."
    private let shortVersion = NSBundle.mainBundle().infoDictionary?["CFBundleShortVersionString"] as! String
    // Realm Results
    private var clipResults = CPYClip.allObjects().sortedResultsUsingProperty("updateTime", ascending: !NSUserDefaults.standardUserDefaults().boolForKey(kCPYPrefReorderClipsAfterPasting))
    private let folderResults = CPYFolder.allObjects().sortedResultsUsingProperty("index", ascending: true)
    // Realm Token
    private var clipToken: RLMNotificationToken?
    
    // MARK: - Enum Values
    enum MenuType {
        case Main, History, Snippet
    }
    enum StatusType: Int {
        case None = 0
        case Black
        case White
    }
    
    // MARK: - Initialize
    override init() {
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
}

// MARK: - Binding
private extension MenuManager {
    private func bind() {
        // Realm Notification
        clipToken = CPYClip.allObjects()
                        .addNotificationBlock { [unowned self] (results, error)  in
                            print("reload clip")
                            self.createClipMenu()
                        }
        // Menu icon
        defaults.rx_observe(Int.self, kCPYPrefShowStatusItemKey, options: [.New])
            .filterNil()
            .subscribeNext { [unowned self] key in
                print("change status")
                self.changeStatusItem(StatusType(rawValue: key) ?? .Black)
            }.addDisposableTo(rx_disposeBag)
        // Clear history menu
        defaults.rx_observe(Bool.self, kCPYPrefAddClearHistoryMenuItemKey, options: [.New])
            .filterNil()
            .skip(1)
            .subscribeNext { [unowned self] enabled in
                print("crear history")
                self.createClipMenu()
            }.addDisposableTo(rx_disposeBag)
        // Sort clips
        defaults.rx_observe(Bool.self, kCPYPrefReorderClipsAfterPasting, options: [.New])
            .filterNil()
            .skip(1)
            .subscribeNext { [unowned self] enabled in
                print("reorder")
                self.clipResults = CPYClip.allObjects().sortedResultsUsingProperty("updateTime", ascending: !enabled)
                self.createClipMenu()
            }.addDisposableTo(rx_disposeBag)
        // Edit snippets
        notificationCenter.rx_notification(kCPYSnippetEditorWillCloseNotification)
            .subscribeNext { [unowned self] notification in
                print("edit snippet")
                self.createClipMenu()
            }.addDisposableTo(rx_disposeBag)
    }
}

// MARK: - Menus
private extension MenuManager {
    private func createClipMenu() {
        clipMenu = NSMenu(title: kClipyIdentifier)
        historyMenu = NSMenu(title: kHistoryMenuIdentifier)
        snippetMenu = NSMenu(title: kSnippetsMenuIdentifier)
        
        addHistoryItems(clipMenu!)
        addHistoryItems(historyMenu!)
        
        addSnippetItems(clipMenu!, separateMenu: true)
        addSnippetItems(snippetMenu!, separateMenu: false)
        
        clipMenu?.addItem(NSMenuItem.separatorItem())

        if defaults.boolForKey(kCPYPrefAddClearHistoryMenuItemKey) {
            clipMenu?.addItem(NSMenuItem(title: LocalizedString.ClearHistory.value, action: "clearAllHistory"))
        }
        
        clipMenu?.addItem(NSMenuItem(title: LocalizedString.EditSnippets.value, action: "showSnippetEditorWindow"))
        clipMenu?.addItem(NSMenuItem(title: LocalizedString.Preference.value, action: "showPreferenceWindow"))
        clipMenu?.addItem(NSMenuItem.separatorItem())
        clipMenu?.addItem(NSMenuItem(title: LocalizedString.QuitClipy.value, action: "terminate:"))
        
        statusItem?.menu = clipMenu
    }
    
    private func menuItemTitle(title: String, listNumber: NSInteger, isMarkWithNumber: Bool) -> String {
        return (isMarkWithNumber) ? "\(listNumber)\(kSingleSpace)\(title)" : title
    }
    
    private func makeSubmenuItem(var count: Int, start: Int, end: Int, numberOfItems: Int) -> NSMenuItem {
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
        subMenuItem.image = (defaults.boolForKey(kCPYPrefShowIconInTheMenuKey)) ? folderIcon : nil
        return subMenuItem
    }
    
    private func incrementListNumber(var listNumber: NSInteger, max: NSInteger, start: NSInteger) -> NSInteger {
        listNumber = listNumber + 1
        if listNumber == max && max == 10 && start == 1 {
            listNumber = 0
        }
        return listNumber
    }
    
    private func trimTitle(title: String?) -> String {
        if title == nil { return "" }
        let theString = title!.stringByTrimmingCharactersInSet(NSCharacterSet.whitespaceAndNewlineCharacterSet()) as NSString
        
        let aRange = NSMakeRange(0, 0)
        var lineStart = 0, lineEnd = 0, contentsEnd = 0
        theString.getLineStart(&lineStart, end: &lineEnd, contentsEnd: &contentsEnd, forRange: aRange)
        
        var titleString = (lineEnd == theString.length) ? theString : theString.substringToIndex(contentsEnd)
        
        var maxMenuItemTitleLength = defaults.integerForKey(kCPYPrefMaxMenuItemTitleLengthKey)
        if maxMenuItemTitleLength < SHORTEN_SYMBOL.characters.count {
            maxMenuItemTitleLength = SHORTEN_SYMBOL.characters.count
        }
        if titleString.length > maxMenuItemTitleLength {
            titleString = titleString.substringToIndex(maxMenuItemTitleLength - SHORTEN_SYMBOL.characters.count) + SHORTEN_SYMBOL
        }
        
        return titleString as String
    }
}

// MARK: - Clips
private extension MenuManager {
    private func addHistoryItems(menu: NSMenu) {
        let placeInLine = defaults.integerForKey(kCPYPrefNumberOfItemsPlaceInlineKey)
        let placeInsideFolder = defaults.integerForKey(kCPYPrefNumberOfItemsPlaceInsideFolderKey)
        let maxHistory = defaults.integerForKey(kCPYPrefMaxHistorySizeKey)
        
        // History title
        let labelItem = NSMenuItem(title: LocalizedString.History.value, action: "", keyEquivalent: kEmptyString)
        labelItem.enabled = false
        menu.addItem(labelItem)
        
        // History
        let firstIndex = firstIndexOfMenuItems()
        var listNumber = firstIndex
        var subMenuCount = placeInLine
        var subMenuIndex = 1 + placeInLine
        
        let currentSize = Int(clipResults.count)
        var i = 0
        for object in clipResults {
            let clip = object as! CPYClip
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
        let isMarkWithNumber = defaults.boolForKey(kCPYPrefMenuItemsAreMarkedWithNumbersKey)
        let isShowToolTip = defaults.boolForKey(kCPYPrefShowToolTipOnMenuItemKey)
        let isShowImage = defaults.boolForKey(kCPYPrefShowImageInTheMenuKey)
        let addNumbericKeyEquivalents = defaults.boolForKey(kCPYPrefAddNumericKeyEquivalentsKey)
        
        var keyEquivalent = kEmptyString
        
        if addNumbericKeyEquivalents && (index <= kMaxKeyEquivalents) {
            let isStartFromZero = defaults.boolForKey(kCPYPrefMenuItemsTitleStartWithZeroKey)
            
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
        
        let menuItem = NSMenuItem(title: titleWithMark, action: "selectClipMenuItem:", keyEquivalent: keyEquivalent)
        menuItem.tag = index
        
        if isShowToolTip {
            let maxLengthOfToolTip = defaults.integerForKey(kCPYPrefMaxLengthOfToolTipKey)
            let toIndex = (clipString.characters.count < maxLengthOfToolTip) ? clipString.characters.count : maxLengthOfToolTip
            menuItem.toolTip = (clipString as NSString).substringToIndex(toIndex)
        }
        
        if primaryPboardType == NSTIFFPboardType {
            menuItem.title = menuItemTitle("(Image)", listNumber: listNumber, isMarkWithNumber: isMarkWithNumber)
        } else if primaryPboardType == NSPDFPboardType {
            menuItem.title = menuItemTitle("(PDF)", listNumber: listNumber, isMarkWithNumber: isMarkWithNumber)
        } else if primaryPboardType == NSFilenamesPboardType && title == kEmptyString {
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
        let labelItem = NSMenuItem(title: LocalizedString.Snippet.value, action: "")
        labelItem.enabled = false
        menu.addItem(labelItem)
        
        var subMenuIndex = menu.numberOfItems - 1
        let firstIndex = firstIndexOfMenuItems()
        
        for object in folderResults {
            let folder = object as! CPYFolder
            if !folder.enable { continue }
            
            let folderTitle = folder.title
            let subMenuItem = makeSubmenuItem(folderTitle)
            menu.addItem(subMenuItem)
            subMenuIndex += 1
            
            var i = firstIndex
            for object in folder.snippets.sortedResultsUsingProperty("index", ascending: true) {
                let snippet = object as! CPYSnippet
                if !snippet.enable { continue }
                
                let subMenuItem = makeSnippetMenuItem(snippet, listNumber: i)
                if let subMenu = menu.itemAtIndex(subMenuIndex)?.submenu {
                    subMenu.addItem(subMenuItem)
                    i += 1
                }
            }
        }
    }
    
    private func makeSnippetMenuItem(snippet: CPYSnippet, listNumber: Int) -> NSMenuItem {
        let isMarkWithNumber = defaults.boolForKey(kCPYPrefMenuItemsAreMarkedWithNumbersKey)
        let isShowIcon = defaults.boolForKey(kCPYPrefShowIconInTheMenuKey)

        let title = trimTitle(snippet.title)
        let titleWithMark = menuItemTitle(title, listNumber: listNumber, isMarkWithNumber: isMarkWithNumber)
        
        let menuItem = NSMenuItem(title: titleWithMark, action: Selector("selectSnippetMenuItem:"), keyEquivalent: kEmptyString)
        menuItem.representedObject = snippet
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
        case .None:
            image = nil
        }
        image?.template = true
        
        statusItem = NSStatusBar.systemStatusBar().statusItemWithLength(-1)
        statusItem?.image = image
        statusItem?.highlightMode = true
        statusItem?.toolTip = "\(kClipyIdentifier)\(shortVersion)"
        
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
        return defaults.boolForKey(kCPYPrefMenuItemsTitleStartWithZeroKey) ? 0 : 1
    }
}

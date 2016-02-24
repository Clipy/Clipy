//
//  CPYMenuManager.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/21.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import PINCache

enum PopUpMenuType {
    case Main, History, Snippets
}

class CPYMenuManager: NSObject {

    // MARK: - Properties
    static let sharedManager = CPYMenuManager()
    
    private var clipMenu: NSMenu?
    private var statusItem: NSStatusItem?
    private var highlightedMenuItem: NSMenuItem?
    
    private var folderIcon = NSImage(assetIdentifier: .IconFolder)
    private var snippetIcon = NSImage(assetIdentifier: .IconText)
    
    private let defaults = NSUserDefaults.standardUserDefaults()
    private let kMaxKeyEquivalents = 10
    private let SHORTEN_SYMBOL = "..."
    private var shortVersion = ""
    
    // MARK: - Init
    override init() {
        super.init()
        initManager()
    }
    
    private func initManager() {
        shortVersion = NSBundle.mainBundle().infoDictionary?["CFBundleShortVersionString"] as! String
        
        folderIcon?.template = true
        folderIcon?.size = NSMakeSize(15, 13)
    
        snippetIcon?.template = true
        snippetIcon?.size = NSMakeSize(12, 13)
        
        createMenu()
        createStatusItem()

        defaults.addObserver(self, forKeyPath: kCPYPrefMenuIconSizeKey, options: .New, context: nil)
        defaults.addObserver(self, forKeyPath: kCPYPrefShowStatusItemKey, options: .New, context: nil)
        
        let notificationCenter = NSNotificationCenter.defaultCenter()
        notificationCenter.addObserver(self, selector: "handleSnippetEditorWillClose:", name: kCPYSnippetEditorWillCloseNotification, object: nil)
        notificationCenter.addObserver(self, selector: "updateStatusItem", name: kCPYChangeContentsNotification, object: nil)
    }

    deinit {
        NSNotificationCenter.defaultCenter().removeObserver(self)

        defaults.removeObserver(self, forKeyPath: kCPYPrefMenuIconSizeKey)
        defaults.removeObserver(self, forKeyPath: kCPYPrefShowStatusItemKey)
        
        if statusItem != nil {
            NSStatusBar.systemStatusBar().removeStatusItem(statusItem!)
            statusItem = nil
        }
    }
    
    // MARK: - KVO
    override func observeValueForKeyPath(keyPath: String?, ofObject object: AnyObject?, change: [String : AnyObject]?, context: UnsafeMutablePointer<Void>) {
        if let change = change where keyPath == kCPYPrefShowStatusItemKey {
            if let new = change["new"] as? Int {
                if new == 0 {
                    removeStatusItem()
                } else {
                    changeStatusItem()
                }
            }
        }
    }
    
    // MARK: - Public Methods
    func createStatusItem() {
        if statusItem == nil {
            changeStatusItem()
        }
    }
    
    func updateStatusItem() {
        if statusItem != nil {
            createMenu()
            statusItem?.menu = clipMenu
        }
    }
    
    func popUpMenuForType(type: PopUpMenuType) {
        var menu: NSMenu?
        switch type {
        case .Main:
            createMenu()
            menu = clipMenu
            break
        case .History:
            menu = makeHistoryMenu()
        case .Snippets:
            menu = makeSnippetsMenu()
        }
        menu?.popUpMenuPositioningItem(nil, atLocation: NSEvent.mouseLocation(), inView: nil)
    }
    
    // MARK: - Menu Methods
    private func createMenu() {
        let newMenu = NSMenu(title: kClipyIdentifier)
        newMenu.delegate = self
        
        addClipsToMenu(newMenu)
        addSnippetsToMenu(newMenu, isBelowClips: true)
        newMenu.addItem(NSMenuItem.separatorItem())
        
        if NSUserDefaults.standardUserDefaults().boolForKey(kCPYPrefAddClearHistoryMenuItemKey) {
            newMenu.addItem(makeMenuItemWithTitle(NSLocalizedString("Clear History", comment: ""), action: "clearAllHistory"))
        }
        newMenu.addItem(makeMenuItemWithTitle(NSLocalizedString("Edit Snippets...", comment: ""), action: "showSnippetEditorWindow"))
        newMenu.addItem(makeMenuItemWithTitle(NSLocalizedString("Preferences...", comment: ""), action: "showPreferenceWindow"))
        newMenu.addItem(NSMenuItem.separatorItem())
        
        newMenu.addItem(makeMenuItemWithTitle(NSLocalizedString("Quit ClipMenu", comment: ""), action: "terminate:"))
        
        clipMenu = newMenu
    }
    
    private func makeMenuItemWithTitle(title: String, action: Selector) -> NSMenuItem {
        return NSMenuItem(title: title, action: action, keyEquivalent: kEmptyString)
    }
    
    private func makeEventFromCurrentEvent(currentEvent: NSEvent, mousePoint: NSPoint, windowNumber: Int) -> NSEvent? {
        return NSEvent.otherEventWithType(currentEvent.type, location: mousePoint, modifierFlags: currentEvent.modifierFlags, timestamp: currentEvent.timestamp, windowNumber: windowNumber, context: currentEvent.context, subtype: currentEvent.subtype.rawValue, data1: currentEvent.data1, data2: currentEvent.data2)
    }
    
    private func makeHistoryMenu() -> NSMenu {
        let newMenu = NSMenu(title: "")
        newMenu.delegate = self
        addClipsToMenu(newMenu)
        return newMenu
    }
    
    private func makeSnippetsMenu() -> NSMenu {
        let newMenu = NSMenu(title: "")
        newMenu.delegate = self
        newMenu.title = kSnippetsMenuIdentifier
        addSnippetsToMenu(newMenu, isBelowClips: false)
        return newMenu
    }
    
    private func menuItemTitleWithString(titleString: String, listNumber: NSInteger, isMarkWithNumber: Bool) -> String {
        if isMarkWithNumber {
            return String(listNumber) + kSingleSpace + titleString
        }
        return titleString
    }
    
    private func makeSubmenuItemWithCount(var count: NSInteger, start: NSInteger, end: NSInteger, numberOfItems: NSInteger) -> NSMenuItem {
        if start == 0 {
            count = count - 1
        }
        var lastNumber = count + numberOfItems
        if end < lastNumber {
            lastNumber = end
        }
        let menuItemTitle = String(count + 1) + " - " + String(lastNumber)
        return makeSubmenuItemWithTitle(menuItemTitle)
    }
    
    private func makeSubmenuItemWithTitle(title: String) -> NSMenuItem {
        let subMenu = NSMenu(title: "")
        let subMenuItem = NSMenuItem(title: title, action: nil, keyEquivalent: kEmptyString)
        subMenuItem.submenu = subMenu
        if NSUserDefaults.standardUserDefaults().boolForKey(kCPYPrefShowIconInTheMenuKey) && folderIcon != nil {
            subMenuItem.image = folderIcon
        }
        
        return subMenuItem
    }
    
    private func addClipsToMenu(menu: NSMenu) {
        let defaluts = NSUserDefaults.standardUserDefaults()
        let numberOfItemsPlaceInLine = defaluts.integerForKey(kCPYPrefNumberOfItemsPlaceInlineKey)
        let numberOfItemsPlaceInsideFolder = defaluts.integerForKey(kCPYPrefNumberOfItemsPlaceInsideFolderKey)
        let maxHistory = defaluts.integerForKey(kCPYPrefMaxHistorySizeKey)
        
        let labelItem = NSMenuItem(title: NSLocalizedString("History", comment: ""), action: "", keyEquivalent: kEmptyString)
        labelItem.enabled = false
        menu.addItem(labelItem)
        
        let firstIndex = firstIndexOfMenuItems()
        var listNumber = firstIndex
        var subMenuCount = numberOfItemsPlaceInLine
        var subMenuIndex = (0 < menu.numberOfItems) ? menu.numberOfItems : 0
        subMenuIndex += numberOfItemsPlaceInLine

        let clips = CPYClipManager.sharedManager.loadSortedClips()
        let currentSize = Int(clips.count)
        var i = 0
        for clip in clips {
            if (numberOfItemsPlaceInLine < 1) || (numberOfItemsPlaceInLine - 1) < i {
                if i == subMenuCount {
                    let subMenuItem = makeSubmenuItemWithCount(subMenuCount, start: firstIndex, end: currentSize, numberOfItems: numberOfItemsPlaceInsideFolder)
                    menu.addItem(subMenuItem)
                    listNumber = firstIndex
                }
                
                let indexOfSubMenu = subMenuIndex
                
                if let subMenu = menu.itemAtIndex(indexOfSubMenu)?.submenu {
                    let menuItem = makeMenuItemForClip(clip as! CPYClip, clipIndex: i, listNumber: listNumber)
                    subMenu.addItem(menuItem)
                    listNumber = incrementListNumber(listNumber, max: numberOfItemsPlaceInsideFolder, start: firstIndex)
                }
                
            } else {
                let menuItem = makeMenuItemForClip(clip as! CPYClip, clipIndex: i, listNumber: listNumber)
                menu.addItem(menuItem)
                
                listNumber = incrementListNumber(listNumber, max: numberOfItemsPlaceInLine, start: firstIndex)
            }
            
            i = i + 1
            if i == (subMenuCount + numberOfItemsPlaceInsideFolder) {
                subMenuCount = subMenuCount + numberOfItemsPlaceInsideFolder
                subMenuIndex = subMenuIndex + 1
            }
            
            if maxHistory <= i {
                break
            }
        }
    }
    
    private func addSnippetsToMenu(menu: NSMenu, isBelowClips: Bool) {
        let folders = CPYSnippetManager.sharedManager.loadSortedFolders()
        if folders.count < 1 {
            return
        }
        
        if isBelowClips {
            menu.addItem(NSMenuItem.separatorItem())
        }
        
        let snippetsLabelItem = NSMenuItem()
        snippetsLabelItem.title = NSLocalizedString("Snippet", comment: "")
        snippetsLabelItem.enabled = false
        menu.addItem(snippetsLabelItem)
        
        var enabled = false
        var subMenuIndex = menu.numberOfItems - 1
        let firstIndex = firstIndexOfMenuItems()
        
        for object in folders {
            let folder = object as! CPYFolder
            enabled = folder.enable
            if !enabled {
                continue
            }
            
            let folderTitle = folder.title
            let subMenuItem = makeSubmenuItemWithTitle(folderTitle)
            menu.addItem(subMenuItem)
            subMenuIndex = subMenuIndex + 1
            
            var i = firstIndex
            for snippetObject in folder.snippets.sortedResultsUsingProperty("index", ascending: true) {
                let snippet = snippetObject as! CPYSnippet
                enabled = snippet.enable
                if !enabled {
                    continue
                }
                
                let menuItem = makeMenuItemForSnippet(snippet, listNumber: i)
                
                if let subMenu = menu.itemAtIndex(subMenuIndex)?.submenu {
                    subMenu.addItem(menuItem)
                    i = i + 1
                }
            }
        }
    }
    
    private func makeMenuItemForClip(clip: CPYClip, clipIndex: NSInteger, listNumber: NSInteger) -> NSMenuItem {
        let defaults = NSUserDefaults.standardUserDefaults()
        
        let isMarkWithNumber = defaults.boolForKey(kCPYPrefMenuItemsAreMarkedWithNumbersKey)
        let isShowToolTip = defaults.boolForKey(kCPYPrefShowToolTipOnMenuItemKey)
        let isShowImage = defaults.boolForKey(kCPYPrefShowImageInTheMenuKey)
        let addNumbericKeyEquivalents = defaults.boolForKey(kCPYPrefAddNumericKeyEquivalentsKey)
        
        var keyEquivalent = kEmptyString
        
        if addNumbericKeyEquivalents && (clipIndex <= kMaxKeyEquivalents) {
            let isStartFromZero = defaults.boolForKey(kCPYPrefMenuItemsTitleStartWithZeroKey)
            
            var shortCutNumber = (isStartFromZero) ? clipIndex : clipIndex + 1
            if shortCutNumber == kMaxKeyEquivalents {
                shortCutNumber = 0
            }
            keyEquivalent = String(shortCutNumber)
        }
        
        let primaryPboardType = clip.primaryType

        let clipString = clip.title
        let title = trimTitle(clipString)
        let titleWithMark = menuItemTitleWithString(title, listNumber: listNumber, isMarkWithNumber: isMarkWithNumber)
        
        let menuItem = NSMenuItem(title: titleWithMark, action: "selectClipMenuItem:", keyEquivalent: keyEquivalent)
        menuItem.tag = clipIndex
        
        if isShowToolTip {
            let maxLengthOfToolTip = defaults.integerForKey(kCPYPrefMaxLengthOfToolTipKey)
            let toIndex = (clipString.characters.count < maxLengthOfToolTip) ? clipString.characters.count : maxLengthOfToolTip
            menuItem.toolTip = (clipString as NSString).substringToIndex(toIndex)
        }
        
        if primaryPboardType == NSTIFFPboardType {
            menuItem.title = menuItemTitleWithString("(Image)", listNumber: listNumber, isMarkWithNumber: isMarkWithNumber)
        } else if primaryPboardType == NSPDFPboardType {
            menuItem.title = menuItemTitleWithString("(PDF)", listNumber: listNumber, isMarkWithNumber: isMarkWithNumber)
        } else if primaryPboardType == NSFilenamesPboardType && title == kEmptyString {
            menuItem.title = menuItemTitleWithString("(Filenames)", listNumber: listNumber, isMarkWithNumber: isMarkWithNumber)
        }
        
        if !clip.thumbnailPath.isEmpty && isShowImage {
            PINCache.sharedCache().objectForKey(clip.thumbnailPath, block: { (cache, key, object) -> Void in
                if let image = object as? NSImage {
                    menuItem.image = image
                }
            })
        }
        
        return menuItem
    }
    
    private func makeMenuItemForSnippet(snippet: CPYSnippet, listNumber: NSInteger) -> NSMenuItem {
        
        let defaults = NSUserDefaults.standardUserDefaults()
        let isMarkWithNumber = defaults.boolForKey(kCPYPrefMenuItemsAreMarkedWithNumbersKey)
        let isShowIcon = defaults.boolForKey(kCPYPrefShowIconInTheMenuKey)
        
        var icon: NSImage?
        
        if isShowIcon {
            icon = iconForSnippet()
        }
        
        let title = trimTitle(snippet.title)
        let titleWithMark = menuItemTitleWithString(title, listNumber: listNumber, isMarkWithNumber: isMarkWithNumber)
        
        let menuItem = NSMenuItem(title: titleWithMark, action: Selector("selectSnippetMenuItem:"), keyEquivalent: kEmptyString)
        menuItem.representedObject = snippet
        menuItem.toolTip = snippet.content
        
        if icon != nil {
            menuItem.image = icon
        }
        
        return menuItem
    }
    
    // MARK: - Icon Manage Methods
    private func iconForSnippet() -> NSImage? {
        return snippetIcon
    }
    
    // MARK: - StatusItem Manage Methods
    private func changeStatusItem() {
        removeStatusItem()
                
        let itemIndex = NSUserDefaults.standardUserDefaults().integerForKey(kCPYPrefShowStatusItemKey)
        if itemIndex == 0 {
            return
        }
        
        var statusIcon: NSImage?
        switch itemIndex {
        case 1:
            statusIcon = NSImage(assetIdentifier: .MenuBlack)
        case 2:
            statusIcon = NSImage(assetIdentifier: .MenuWhite)
        default:
            statusIcon = NSImage(assetIdentifier: .MenuBlack)
        }
        statusIcon?.template = true
        
        let statusBar = NSStatusBar.systemStatusBar()
        statusItem = statusBar.statusItemWithLength(-1)
        statusItem?.image = statusIcon
        statusItem?.highlightMode = true
        statusItem?.toolTip = kClipyIdentifier + shortVersion
        
        if clipMenu != nil {
            statusItem?.menu = clipMenu
        }
    }
    
    private func removeStatusItem() {
        if statusItem != nil {
            NSStatusBar.systemStatusBar().removeStatusItem(statusItem!)
            statusItem = nil
        }
    }
    
    private func unhighlightMenuItem() {
        highlightedMenuItem?.image = folderIcon
        highlightedMenuItem = nil
    }
    
    // MARK: - Private Methods
    private func firstIndexOfMenuItems() -> NSInteger {
        if NSUserDefaults.standardUserDefaults().boolForKey(kCPYPrefMenuItemsTitleStartWithZeroKey) {
            return 0
        }
        return 1
    }
    
    private func incrementListNumber(var listNumber: NSInteger, max: NSInteger, start: NSInteger) -> NSInteger {
        listNumber = listNumber + 1
        if listNumber == max && max == 10 && start == 1 {
            listNumber = 0
        }
        return listNumber
    }
    
    private func trimTitle(clipString: String?) -> String {
        if clipString == nil {
            return ""
        }
        let theString = clipString!.stringByTrimmingCharactersInSet(NSCharacterSet.whitespaceAndNewlineCharacterSet()) as NSString
        
        let aRange = NSMakeRange(0, 0)
        var lineStart = 0, lineEnd = 0, contentsEnd = 0
        theString.getLineStart(&lineStart, end: &lineEnd, contentsEnd: &contentsEnd, forRange: aRange)
        
        var titleString = (lineEnd == theString.length) ? theString : theString.substringToIndex(contentsEnd)
        
        var maxMenuItemTitleLength = NSUserDefaults.standardUserDefaults().integerForKey(kCPYPrefMaxMenuItemTitleLengthKey)
        if maxMenuItemTitleLength < SHORTEN_SYMBOL.characters.count {
            maxMenuItemTitleLength = SHORTEN_SYMBOL.characters.count
        }
        if titleString.length > maxMenuItemTitleLength {
            titleString = titleString.substringToIndex(maxMenuItemTitleLength - SHORTEN_SYMBOL.characters.count) + SHORTEN_SYMBOL
        }
        
        return titleString as String
    }
    
    // MARK: - NSNotificationCenter Methods
    func handleSnippetEditorWillClose(notification: NSNotification) {
        updateStatusItem()
    }

}

// MARK: - NSMenu Delegate
extension CPYMenuManager: NSMenuDelegate {
    func menu(menu: NSMenu, willHighlightItem item: NSMenuItem?) {
        if highlightedMenuItem != nil {
            if item == nil {
                unhighlightMenuItem()
            } else if !item!.isEqualTo(highlightedMenuItem) {
                unhighlightMenuItem()
            }
        }
        
        if item == nil || (item != nil && !item!.hasSubmenu) {
            return
        }
        
        if item?.image != nil {
            highlightedMenuItem = item
        }
    }
    
    func menuDidClose(menu: NSMenu) {
        if highlightedMenuItem != nil {
            unhighlightMenuItem()
        }
    }
}

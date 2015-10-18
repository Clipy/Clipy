//
//  CPYMenuManager.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/21.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa

enum PopUpMenuType {
    case Main, History, Snippets
}

class CPYMenuManager: NSObject {

    // MARK: - Properties
    static let sharedManager = CPYMenuManager()
    
    private var clipMenu: NSMenu?
    private var statusItem: NSStatusItem?
    private var highlightedMenuItem: NSMenuItem?
    
    var folderIcon = NSImage(named: "icon_folder")
    var snippetIcon = NSImage(named: "icon_text")

    private let kMaxKeyEquivalents = 10
    private let SHORTEN_SYMBOL = "..."
    private var shortVersion = ""
    
    // MARK: - Init
    override init() {
        super.init()
        self.initManager()
    }
    
    private func initManager() {
        self.shortVersion = NSBundle.mainBundle().infoDictionary?["CFBundleShortVersionString"] as! String
        
        self.folderIcon?.template = true
        self.folderIcon?.size = NSMakeSize(15, 13)
    
        self.snippetIcon?.template = true
        self.snippetIcon?.size = NSMakeSize(12, 13)
        
        self.createMenu()
        self.createStatusItem()
        
        let defaults = NSUserDefaults.standardUserDefaults()
        defaults.addObserver(self, forKeyPath: kCPYPrefMenuIconSizeKey, options: .New, context: nil)
        defaults.addObserver(self, forKeyPath: kCPYPrefShowStatusItemKey, options: .New, context: nil)
        
        let notificationCenter = NSNotificationCenter.defaultCenter()
        notificationCenter.addObserver(self, selector: "handleSnippetEditorWillClose:", name: kCPYSnippetEditorWillCloseNotification, object: nil)
        notificationCenter.addObserver(self, selector: "updateStatusItem", name: kCPYChangeContentsNotification, object: nil)
    }

    deinit {
        NSNotificationCenter.defaultCenter().removeObserver(self)
        
        let defaults = NSUserDefaults.standardUserDefaults()
        defaults.removeObserver(self, forKeyPath: kCPYPrefMenuIconSizeKey)
        defaults.removeObserver(self, forKeyPath: kCPYPrefShowStatusItemKey)
        
        if self.statusItem != nil {
            NSStatusBar.systemStatusBar().removeStatusItem(self.statusItem!)
            self.statusItem = nil
        }
    }
    
    // MARK: - KVO
    override func observeValueForKeyPath(keyPath: String?, ofObject object: AnyObject?, change: [String : AnyObject]?, context: UnsafeMutablePointer<Void>) {
        if let change = change where keyPath == kCPYPrefShowStatusItemKey {
            if let new = change["new"] as? Int {
                if new == 0 {
                    self.removeStatusItem()
                } else {
                    self.changeStatusItem()
                }
            }
        }
    }
    
    // MARK: - Public Methods
    func createStatusItem() {
        if self.statusItem == nil {
            self.changeStatusItem()
        }
    }
    
    func updateStatusItem() {
        if self.statusItem != nil {
            self.createMenu()
            self.statusItem?.menu = self.clipMenu
        }
    }
    
    func popUpMenuForType(type: PopUpMenuType) {
        var menu: NSMenu?
        switch type {
        case .Main:
            self.createMenu()
            menu = self.clipMenu
            break
        case .History:
            menu = self.makeHistoryMenu()
        case .Snippets:
            menu = self.makeSnippetsMenu()
        }
        menu?.popUpMenuPositioningItem(nil, atLocation: NSEvent.mouseLocation(), inView: nil)
    }
    
    // MARK: - Menu Methods
    private func createMenu() {
        let newMenu = NSMenu(title: kClipyIdentifier)
        newMenu.delegate = self
        
        self.addClipsToMenu(newMenu)
        self.addSnippetsToMenu(newMenu, isBelowClips: true)
        newMenu.addItem(NSMenuItem.separatorItem())
        
        if NSUserDefaults.standardUserDefaults().boolForKey(kCPYPrefAddClearHistoryMenuItemKey) {
            newMenu.addItem(self.makeMenuItemWithTitle(NSLocalizedString("Clear History", comment: ""), action: "clearAllHistory"))
        }
        newMenu.addItem(self.makeMenuItemWithTitle(NSLocalizedString("Edit Snippets...", comment: ""), action: "showSnippetEditorWindow"))
        newMenu.addItem(self.makeMenuItemWithTitle(NSLocalizedString("Preferences...", comment: ""), action: "showPreferenceWindow"))
        newMenu.addItem(NSMenuItem.separatorItem())
        
        newMenu.addItem(self.makeMenuItemWithTitle(NSLocalizedString("Quit ClipMenu", comment: ""), action: "terminate:"))
        
        self.clipMenu = newMenu
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
        self.addClipsToMenu(newMenu)
        return newMenu
    }
    
    private func makeSnippetsMenu() -> NSMenu {
        let newMenu = NSMenu(title: "")
        newMenu.delegate = self
        newMenu.title = kSnippetsMenuIdentifier
        self.addSnippetsToMenu(newMenu, isBelowClips: false)
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
        return self.makeSubmenuItemWithTitle(menuItemTitle)
    }
    
    private func makeSubmenuItemWithTitle(title: String) -> NSMenuItem {
        let subMenu = NSMenu(title: "")
        let subMenuItem = NSMenuItem(title: title, action: nil, keyEquivalent: kEmptyString)
        subMenuItem.submenu = subMenu
        if NSUserDefaults.standardUserDefaults().boolForKey(kCPYPrefShowIconInTheMenuKey) && self.folderIcon != nil {
            subMenuItem.image = self.folderIcon
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
        
        let firstIndex = self.firstIndexOfMenuItems()
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
                    let subMenuItem = self.makeSubmenuItemWithCount(subMenuCount, start: firstIndex, end: currentSize, numberOfItems: numberOfItemsPlaceInsideFolder)
                    menu.addItem(subMenuItem)
                    listNumber = firstIndex
                }
                
                let indexOfSubMenu = subMenuIndex
                
                if let subMenu = menu.itemAtIndex(indexOfSubMenu)?.submenu {
                    let menuItem = self.makeMenuItemForClip(clip as! CPYClip, clipIndex: i, listNumber: listNumber)
                    subMenu.addItem(menuItem)
                    listNumber = self.incrementListNumber(listNumber, max: numberOfItemsPlaceInsideFolder, start: firstIndex)
                }
                
            } else {
                let menuItem = self.makeMenuItemForClip(clip as! CPYClip, clipIndex: i, listNumber: listNumber)
                menu.addItem(menuItem)
                
                listNumber = self.incrementListNumber(listNumber, max: numberOfItemsPlaceInLine, start: firstIndex)
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
        let firstIndex = self.firstIndexOfMenuItems()
        
        for object in folders {
            let folder = object as! CPYFolder
            enabled = folder.enable
            if !enabled {
                continue
            }
            
            let folderTitle = folder.title
            let subMenuItem = self.makeSubmenuItemWithTitle(folderTitle)
            menu.addItem(subMenuItem)
            subMenuIndex = subMenuIndex + 1
            
            var i = firstIndex
            for snippetObject in folder.snippets.sortedResultsUsingProperty("index", ascending: true) {
                let snippet = snippetObject as! CPYSnippet
                enabled = snippet.enable
                if !enabled {
                    continue
                }
                
                let menuItem = self.makeMenuItemForSnippet(snippet, listNumber: i)
                
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
        let title = self.trimTitle(clipString)
        let titleWithMark = self.menuItemTitleWithString(title, listNumber: listNumber, isMarkWithNumber: isMarkWithNumber)
        
        let menuItem = NSMenuItem(title: titleWithMark, action: "selectClipMenuItem:", keyEquivalent: keyEquivalent)
        menuItem.tag = clipIndex
        
        if isShowToolTip {
            let maxLengthOfToolTip = defaults.integerForKey(kCPYPrefMaxLengthOfToolTipKey)
            let toIndex = (clipString.characters.count < maxLengthOfToolTip) ? clipString.characters.count : maxLengthOfToolTip
            menuItem.toolTip = (clipString as NSString).substringToIndex(toIndex)
        }
        
        if primaryPboardType == NSTIFFPboardType {
            menuItem.title = self.menuItemTitleWithString("(Image)", listNumber: listNumber, isMarkWithNumber: isMarkWithNumber)
        } else if primaryPboardType == NSPDFPboardType {
            menuItem.title = self.menuItemTitleWithString("(PDF)", listNumber: listNumber, isMarkWithNumber: isMarkWithNumber)
        } else if primaryPboardType == NSFilenamesPboardType && title == kEmptyString {
            menuItem.title = self.menuItemTitleWithString("(Filenames)", listNumber: listNumber, isMarkWithNumber: isMarkWithNumber)
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
            icon = self.iconForSnippet()
        }
        
        let title = self.trimTitle(snippet.title)
        let titleWithMark = self.menuItemTitleWithString(title, listNumber: listNumber, isMarkWithNumber: isMarkWithNumber)
        
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
        return self.snippetIcon
    }
    
    // MARK: - StatusItem Manage Methods
    private func changeStatusItem() {
        self.removeStatusItem()
                
        let itemIndex = NSUserDefaults.standardUserDefaults().integerForKey(kCPYPrefShowStatusItemKey)
        if itemIndex == 0 {
            return
        }
        
        var statusIcon: NSImage?
        switch itemIndex {
        case 1:
            statusIcon = NSImage(named: "statusbar_menu_black")
        case 2:
            statusIcon = NSImage(named: "statusbar_menu_white")
        default:
            statusIcon = NSImage(named: "statusbar_menu_black")
        }
        statusIcon?.template = true
        
        let statusBar = NSStatusBar.systemStatusBar()
        self.statusItem = statusBar.statusItemWithLength(-1)
        self.statusItem?.image = statusIcon
        self.statusItem?.highlightMode = true
        self.statusItem?.toolTip = kClipyIdentifier + self.shortVersion
        
        if self.clipMenu != nil {
            self.statusItem?.menu = self.clipMenu
        }
    }
    
    private func removeStatusItem() {
        if self.statusItem != nil {
            NSStatusBar.systemStatusBar().removeStatusItem(self.statusItem!)
            self.statusItem = nil
        }
    }
    
    private func unhighlightMenuItem() {
        self.highlightedMenuItem?.image = folderIcon
        self.highlightedMenuItem = nil
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
        self.updateStatusItem()
    }

}

// MARK: - NSMenu Delegate
extension CPYMenuManager: NSMenuDelegate {
    func menu(menu: NSMenu, willHighlightItem item: NSMenuItem?) {
        
        if self.highlightedMenuItem != nil {
            if item == nil {
                self.unhighlightMenuItem()
            } else if !item!.isEqualTo(self.highlightedMenuItem) {
                self.unhighlightMenuItem()
            }
        }
        
        if item == nil || (item != nil && !item!.hasSubmenu) {
            return
        }
        
        if item?.image != nil {
            self.highlightedMenuItem = item
        }
    }
    
    func menuDidClose(menu: NSMenu) {
        if self.highlightedMenuItem != nil {
            self.unhighlightMenuItem()
        }
    }
}

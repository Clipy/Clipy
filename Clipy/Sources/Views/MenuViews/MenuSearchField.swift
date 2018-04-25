//
//  MenuSearchField.swift
//  Clipy
//
//  Created by Vincenzo Favara on 20/08/2017.
//  Copyright © 2017 Shunsuke Furubayashi. All rights reserved.
//

protocol SearchProtocol: class {
    func controlTextDidChange()
    func checkResultsForFilter()
}

import Cocoa

class MenuSearchField: NSSearchField {
    
    lazy var searchesMenu: NSMenu = {
        
        let menu = NSMenu(title: "Search")
        
        let recentTitleItem = menu.addItem(withTitle: "Recent Searches", action: nil, keyEquivalent: "")
        recentTitleItem.tag = Int(NSSearchFieldRecentsTitleMenuItemTag)
        
        let placeholder = menu.addItem(withTitle: "Item", action: nil, keyEquivalent: "")
        placeholder.tag = Int(NSSearchFieldRecentsMenuItemTag)
        
        menu.addItem( NSMenuItem.separator() )
        
        let clearItem = menu.addItem(withTitle: "Clear Menu", action: nil, keyEquivalent: "")
        clearItem.tag = Int(NSSearchFieldClearRecentsMenuItemTag)
        
        let emptyItem = menu.addItem(withTitle: "No Recent Searches", action: nil, keyEquivalent: "")
        emptyItem.tag = Int(NSSearchFieldNoRecentsMenuItemTag)
        
        return menu
    }()
    
    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        initialize()
    }
    
    required init?(coder: NSCoder) {
        super.init(coder: coder)
        initialize()
    }
    
    //create menu
    private func initialize() {
        self.searchMenuTemplate = searchesMenu
    }
}

//
//  CPYPreferencesWindowController.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/02/25.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa

class CPYPreferencesWindowController: NSWindowController {

    override func windowDidLoad() {
        super.windowDidLoad()
        self.window?.collectionBehavior = NSWindowCollectionBehavior.CanJoinAllSpaces
        self.window?.backgroundColor = NSColor(white: 0.99, alpha: 1)
        if #available(OSX 10.10, *) {
            self.window?.titlebarAppearsTransparent = true
        }
        //self.switchView(self.generalButton)
    }
    
}

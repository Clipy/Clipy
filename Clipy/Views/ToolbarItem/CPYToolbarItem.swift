//
//  CPYToolbarItem.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/02/25.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa

@IBDesignable final class CPYToolbarItem: NSView {

    // MARK: - Properties
    @IBInspectable var image: NSImage?
    @IBInspectable var selectedImage: NSImage?
    @IBInspectable var titleColor: NSColor = NSColor.titleColor()
    @IBInspectable var selectedTitleColor: NSColor = NSColor.clipyColor()
    @IBInspectable var title: String = ""
    private let button = NSButton()
    
    // MARK: - Initializr
    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        initView()
    }
    
    required init?(coder: NSCoder) {
        super.init(coder: coder)
        initView()
    }
    
    private func initView() {
        
    }
    
    // MARK: - Draw Method
    override func drawRect(dirtyRect: NSRect) {
        super.drawRect(dirtyRect)
    }
    
}

// MARK; - Action
extension CPYToolbarItem {
    
}

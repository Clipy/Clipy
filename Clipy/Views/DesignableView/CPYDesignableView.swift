//
//  CPYDesignableView.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/02/25.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation
import Cocoa

@IBDesignable class CPYDesignableView: NSView {
    
    // MARK: - Properties
    @IBInspectable var backgroundColor: NSColor = NSColor.clearColor() {
        didSet {
            needsDisplay = true
        }
    }
    @IBInspectable var borderColor: NSColor = NSColor.clearColor() {
        didSet {
            needsDisplay = true
        }
    }
    @IBInspectable var borderWidth: CGFloat = 0 {
        didSet {
            needsDisplay = true
        }
    }
    @IBInspectable var cornerRadius: CGFloat = 0 {
        didSet {
            needsDisplay = true
        }
    }
    override var wantsUpdateLayer: Bool {
        return true
    }
    
    // MARK: - Initialize
    override init(frame frameRect: NSRect) {
        super.init(frame: frameRect)
        wantsLayer = true
    }
    
    required init?(coder: NSCoder) {
        super.init(coder: coder)
        wantsLayer = true
    }
    
    // MARK: - Update
    override func updateLayer() {
        layer?.backgroundColor = backgroundColor.CGColor
        layer?.borderColor = borderColor.CGColor
        layer?.borderWidth = borderWidth
        layer?.cornerRadius = cornerRadius
        if cornerRadius != 0 {
            layer?.masksToBounds = true
        }
    }

}

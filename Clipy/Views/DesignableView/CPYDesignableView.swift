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
    }

    required init?(coder: NSCoder) {
        super.init(coder: coder)
    }

    // MARK: - Update
    override func drawRect(dirtyRect: NSRect) {
        // Background
        backgroundColor.setFill()
        NSBezierPath(roundedRect: bounds, xRadius: cornerRadius, yRadius: cornerRadius).fill()
        // Border
        let rect = NSRect(x: borderWidth / 2, y: borderWidth / 2, width: bounds.width - borderWidth, height: bounds.height - borderWidth)
        let path = NSBezierPath(roundedRect: rect, xRadius: cornerRadius, yRadius: cornerRadius)
        path.lineWidth = borderWidth
        borderColor.set()
        path.stroke()
    }

}

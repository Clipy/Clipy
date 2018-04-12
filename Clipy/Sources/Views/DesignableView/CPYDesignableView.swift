//
//  CPYDesignableView.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2016/02/25.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Foundation
import Cocoa

@IBDesignable class CPYDesignableView: NSView {

    // MARK: - Properties
    @IBInspectable var backgroundColor: NSColor = .clear {
        didSet {
            needsDisplay = true
        }
    }
    @IBInspectable var borderColor: NSColor = .clear {
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
    override func draw(_ dirtyRect: NSRect) {
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

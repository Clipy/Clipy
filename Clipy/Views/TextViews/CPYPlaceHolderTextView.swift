//
//  CPYPlaceHolderTextView.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/06/29.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa

class CPYPlaceHolderTextView: NSTextView {

    // MARK: - Properties
    @IBInspectable var placeHolderColor: NSColor = .disabledControlTextColor() {
        didSet {
            needsDisplay = true
        }
    }
    @IBInspectable var placeHolderText: String = "" {
        didSet {
            needsDisplay = true
        }
    }
    override var textContainerOrigin: NSPoint {
        return NSPoint(x: 0, y: 7)
    }

    // MARK: - Draw
    override func drawRect(dirtyRect: NSRect) {
        super.drawRect(dirtyRect)
        if placeHolderText.isEmpty { return }
        if let string = string where !string.isEmpty { return }

        let text = placeHolderText as NSString
        let paragraphStyle = NSMutableParagraphStyle()
        paragraphStyle.lineBreakMode = .ByTruncatingTail
        paragraphStyle.baseWritingDirection = .LeftToRight
        let attributes = [NSFontAttributeName: NSFont.systemFontOfSize(14), NSForegroundColorAttributeName: placeHolderColor, NSParagraphStyleAttributeName: paragraphStyle]
        text.drawAtPoint(NSPoint(x: 5, y: 5), withAttributes: attributes)
    }

}

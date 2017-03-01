//
//  CPYSnippetsEditorCell.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/07/02.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation
import Cocoa

final class CPYSnippetsEditorCell: NSTextFieldCell {

    // MARK: - Properties
    var iconType = IconType.folder
    var isItemEnabled = false
    override var cellSize: NSSize {
        var size = super.cellSize
        size.width += 3.0 + 16.0
        return size
    }

    // MARK: - Enums
    enum IconType {
        case folder, none
    }

    // MARK: - Initialize
    required init(coder: NSCoder) {
        super.init(coder: coder)
        font = NSFont.systemFont(ofSize: 14)
    }

    override func copy(with zone: NSZone?) -> Any {
        guard let cell = super.copy(with: zone) as? CPYSnippetsEditorCell else { return super.copy(with: zone) }
        cell.iconType = iconType
        return cell
    }

    // MARK: - Draw
    override func draw(withFrame cellFrame: NSRect, in controlView: NSView) {
        var newFrame: NSRect
        switch iconType {
        case .folder:

            var imageFrame = NSRect.zero
            var cellFrame = cellFrame
            NSDivideRect(cellFrame, &imageFrame, &cellFrame, 15, .minX)
            imageFrame.origin.x += 5
            imageFrame.origin.y += 5
            imageFrame.size = NSSize(width: 16, height: 13)

            let drawImage = (isHighlighted) ? NSImage(assetIdentifier: .SnippetsIconFolderWhite) : NSImage(assetIdentifier: .SnippetsIconFolder)
            drawImage.size = NSSize(width: 16, height: 13)
            drawImage.draw(in: imageFrame, from: NSRect.zero, operation: .sourceOver, fraction: 1.0, respectFlipped: true, hints: nil)

            newFrame = cellFrame
            newFrame.origin.x += 8
            newFrame.origin.y += 2
            newFrame.size.height -= 2
        case .none:
            newFrame = cellFrame
            newFrame.origin.y += 2
            newFrame.size.height -= 2
        }

        textColor = (!isItemEnabled) ? .lightGray : (isHighlighted) ? .white : .titleColor()

        super.draw(withFrame: newFrame, in: controlView)
    }

    // MARK: - Frame
    override func select(withFrame aRect: NSRect, in controlView: NSView, editor textObj: NSText, delegate anObject: Any?, start selStart: Int, length selLength: Int) {
        let textFrame = titleRect(forBounds: aRect)
        textColor = .titleColor()
        super.select(withFrame: textFrame, in: controlView, editor: textObj, delegate: anObject, start: selStart, length: selLength)
    }

    override func edit(withFrame aRect: NSRect, in controlView: NSView, editor textObj: NSText, delegate anObject: Any?, event theEvent: NSEvent?) {
        let textFrame = titleRect(forBounds: aRect)
        super.edit(withFrame: textFrame, in: controlView, editor: textObj, delegate: anObject, event: theEvent)
    }

    override func titleRect(forBounds theRect: NSRect) -> NSRect {
        switch iconType {
        case .folder:

            var imageFrame = NSRect.zero
            var cellRect = NSRect.zero

            NSDivideRect(theRect, &imageFrame, &cellRect, 15, .minX)

            imageFrame.origin.x += 5
            imageFrame.origin.y += 4
            imageFrame.size = CGSize(width: 16, height: 15)

            imageFrame.origin.y += ceil((cellRect.size.height - imageFrame.size.height) / 2)

            var newFrame = cellRect
            newFrame.origin.x += 10
            newFrame.origin.y += 2
            newFrame.size.height -= 2

            return newFrame

        case .none:
            var newFrame = theRect
            newFrame.origin.y += 2
            newFrame.size.height -= 2
            return newFrame
        }
    }
}

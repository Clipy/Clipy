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
    var iconType = IconType.Folder
    var isItemEnabled = false
    override var cellSize: NSSize {
        var size = super.cellSize
        size.width = size.width + 3.0 + 16.0
        return size
    }

    // MARK: - Enums
    enum IconType {
        case Folder, None
    }

    // MARK: - Initialize
    required init?(coder aDecoder: NSCoder) {
        super.init(coder: aDecoder)
        font = NSFont.systemFontOfSize(14)
    }

    override func copyWithZone(zone: NSZone) -> AnyObject {
        guard let cell = super.copyWithZone(zone) as? CPYSnippetsEditorCell else { return super.copyWithZone(zone) }
        cell.iconType = iconType
        return cell
    }

    // MARK: - Draw
    override func drawWithFrame(cellFrame: NSRect, inView controlView: NSView) {
        var newFrame: NSRect
        switch iconType {
        case .Folder:

            var imageFrame = NSRect.zero
            var cellFrame = cellFrame
            NSDivideRect(cellFrame, &imageFrame, &cellFrame, 15, .MinX)
            imageFrame.origin.x += 5
            imageFrame.origin.y += 5
            imageFrame.size = NSSize(width: 16, height: 13)

            let drawImage = (highlighted) ? NSImage(assetIdentifier: .SnippetsIconFolderWhite) : NSImage(assetIdentifier: .SnippetsIconFolder)
            drawImage.size = NSSize(width: 16, height: 13)
            drawImage.drawInRect(imageFrame, fromRect: NSRect.zero, operation: .CompositeSourceOver, fraction: 1.0, respectFlipped: true, hints: nil)

            newFrame = cellFrame
            newFrame.origin.x += 8
            newFrame.origin.y += 2
            newFrame.size.height -= 2
        case .None:
            newFrame = cellFrame
            newFrame.origin.y += 2
            newFrame.size.height -= 2
        }

        textColor = (!isItemEnabled) ? .lightGrayColor() : (highlighted) ? .whiteColor() : .titleColor()

        super.drawWithFrame(newFrame, inView: controlView)
    }

    // MARK: - Frame
    override func selectWithFrame(aRect: NSRect, inView controlView: NSView, editor textObj: NSText, delegate anObject: AnyObject?, start selStart: Int, length selLength: Int) {
        let textFrame = titleRectForBounds(aRect)
        textColor = .titleColor()
        super.selectWithFrame(textFrame, inView: controlView, editor: textObj, delegate: anObject, start: selStart, length: selLength)
    }

    override func editWithFrame(aRect: NSRect, inView controlView: NSView, editor textObj: NSText, delegate anObject: AnyObject?, event theEvent: NSEvent) {
        let textFrame = titleRectForBounds(aRect)
        super.editWithFrame(textFrame, inView: controlView, editor: textObj, delegate: anObject, event: theEvent)
    }

    override func titleRectForBounds(theRect: NSRect) -> NSRect {
        switch iconType {
        case .Folder:

            var imageFrame = NSRect.zero
            var cellRect = NSRect.zero

            NSDivideRect(theRect, &imageFrame, &cellRect, 15, .MinX)

            imageFrame.origin.x += 5
            imageFrame.origin.y += 4
            imageFrame.size = CGSize(width: 16, height: 15)

            imageFrame.origin.y += ceil((cellRect.size.height - imageFrame.size.height) / 2)

            var newFrame = cellRect
            newFrame.origin.x += 10
            newFrame.origin.y += 2
            newFrame.size.height -= 2

            return newFrame

        case .None:
            var newFrame = theRect
            newFrame.origin.y += 2
            newFrame.size.height -= 2
            return newFrame
        }
    }
}

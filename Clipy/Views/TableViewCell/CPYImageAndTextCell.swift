//
//  CPYImageAndTextCell.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/28.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa

private let kIconImageSize      = 16.0
private let kImageOriginXOffset = 5
private let kImageOriginYOffset = -3

private let kTextOriginXOffset	= 4
private let kTextOriginYOffset	= 0
private let kTextHeightAdjust	= 0

class CPYImageAndTextCell: NSTextFieldCell {

    // MARK: - Properties
    internal var cellImageType: ImageType = .Folder
    enum ImageType {
        case Folder, File, Application
    }
    
    
    // MARK: - Init
    required init(coder aDecoder: NSCoder) {
        super.init(coder: aDecoder)
        self.font = NSFont.systemFontOfSize(NSFont.systemFontSize())
    }
    
    init(textCell aString: String) {
        super.init(textCell: aString)
    }
    
    // MARK: - Override Methods
    override func copyWithZone(zone: NSZone) -> AnyObject {
        let cell = super.copyWithZone(zone) as! CPYImageAndTextCell
        cell.cellImageType = self.cellImageType
        return cell
    }
    
    override func titleRectForBounds(theRect: NSRect) -> NSRect {
        var imageFrame = NSZeroRect
        var cellRect = NSZeroRect
        
        NSDivideRect(theRect, &imageFrame, &cellRect, 3 + 16.0, NSMinXEdge)
        
        imageFrame.origin.x += CGFloat(kImageOriginXOffset)
        imageFrame.origin.y -= CGFloat(kImageOriginYOffset)
        imageFrame.size = CGSizeMake(16.0, 16.0);
        
        imageFrame.origin.y += ceil((cellRect.size.height - imageFrame.size.height) / 2);
        
        var newFrame = cellRect;
        newFrame.origin.x += CGFloat(kTextOriginXOffset)
        newFrame.origin.y += CGFloat(kTextOriginYOffset)
        newFrame.size.height -= CGFloat(kTextHeightAdjust)
        
        return newFrame;
    }
    
    override func editWithFrame(aRect: NSRect, inView controlView: NSView, editor textObj: NSText, delegate anObject: AnyObject?, event theEvent: NSEvent) {
        let textFrame = self.titleRectForBounds(aRect)
        super.editWithFrame(textFrame, inView: controlView, editor: textObj, delegate: anObject, event: theEvent)
    }
    
    override func selectWithFrame(aRect: NSRect, inView controlView: NSView, editor textObj: NSText, delegate anObject: AnyObject?, start selStart: Int, length selLength: Int) {
        let textFrame = self.titleRectForBounds(aRect)
        super.selectWithFrame(textFrame, inView: controlView, editor: textObj, delegate: anObject, start: selStart, length: selLength)
    }
    
    override func drawWithFrame(var cellFrame: NSRect, inView controlView: NSView) {
        var imageFrame = NSZeroRect
        NSDivideRect(cellFrame, &imageFrame, &cellFrame, 3 + 15.0, NSMinXEdge)
        
        imageFrame.origin.x += CGFloat(kImageOriginXOffset)
        imageFrame.origin.y -= CGFloat(kImageOriginYOffset)
        
        if self.cellImageType == .Folder {
            imageFrame.size = NSMakeSize(15.0, 13.0)
            NSImage(named: "icon_folder")?.drawInRect(imageFrame, fromRect: NSZeroRect, operation: NSCompositingOperation.CompositeSourceOver, fraction: 1.0, respectFlipped: true, hints: nil)
        } else if self.cellImageType == .File {
            imageFrame.size = NSMakeSize(12.0, 13.0)
            NSImage(named: "icon_text")?.drawInRect(imageFrame, fromRect: NSZeroRect, operation: NSCompositingOperation.CompositeSourceOver, fraction: 1.0, respectFlipped: true, hints: nil)
        } else if self.cellImageType == .Application {
            imageFrame.size = NSMakeSize(16.0, 16.0)
            //NSWorkspace.sharedWorkspace().iconForFileType(NSFileTypeForHFSTypeCode(OSType(kGenericApplicationIcon))).drawInRect(imageFrame, fromRect: NSZeroRect, operation: NSCompositingOperation.CompositeSourceOver, fraction: 1.0, respectFlipped: true, hints: nil)
        }
        var newFrame = cellFrame;
        newFrame.origin.x += CGFloat(kTextOriginXOffset)
        newFrame.origin.y += CGFloat(kTextOriginYOffset)
        newFrame.size.height -= CGFloat(kTextHeightAdjust)
        super.drawWithFrame(newFrame, inView: controlView)
    }
    
    override var cellSize: NSSize {
        var size = super.cellSize
        size.width = size.width + 3.0 + 16.0
        return size
    }
    
}

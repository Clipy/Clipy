//
//  NSImage+Resize.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/07/26.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation

extension NSImage {
    func resizeImage(width: CGFloat, _ height: CGFloat) -> NSImage {
        let img = NSImage(size: CGSizeMake(width, height))
        
        img.lockFocus()
        let ctx = NSGraphicsContext.currentContext()
        ctx?.imageInterpolation = .High
        self.drawInRect(NSMakeRect(0, 0, width, height), fromRect: NSMakeRect(0, 0, size.width, size.height), operation: .CompositeCopy, fraction: 1)
        img.unlockFocus()
        
        return img
    }
}
//
//  NSImage+Resize.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/07/26.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation
import Cocoa

extension NSImage {
    func resizeImage(width: CGFloat, _ height: CGFloat) -> NSImage? {

        let representations = self.representations
        var bitmapRep: NSBitmapImageRep?

        for rep in representations {
            if let rep = rep as? NSBitmapImageRep {
                bitmapRep = rep
                break
            }
        }

        if bitmapRep == nil {
            return nil
        }

        let origWidth = CGFloat(bitmapRep!.pixelsWide)
        let origHeight = CGFloat(bitmapRep!.pixelsHigh)

        let aspect = CGFloat(origWidth) / CGFloat(origHeight)

        let targetWidth = width
        let targetHeight = height
        var newWidth: CGFloat
        var newHeight: CGFloat

        if 1 <= aspect {
            newWidth = targetWidth
            newHeight = newWidth / aspect

            if targetHeight < newHeight {
                newHeight = targetHeight
                newWidth = targetHeight * aspect
            }
        } else {
            newHeight = targetHeight
            newWidth = targetHeight * aspect

            if targetWidth < newWidth {
                newWidth = targetWidth
                newHeight = targetWidth / aspect
            }
        }

        if origWidth < newWidth {
            newWidth = origWidth
        }
        if origHeight < newHeight {
            newHeight = origHeight
        }

        let newImageRep = self.bestRepresentationForRect(NSMakeRect(0, 0, newWidth, newHeight), context: nil, hints: nil)
        if newImageRep == nil {
            return nil
        }

        let thumbnail = NSImage(size: NSMakeSize(newWidth, newHeight))
        thumbnail.addRepresentation(newImageRep!)

        return thumbnail
    }
}

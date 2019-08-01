//
//  NSImage+Resize.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2015/07/26.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Foundation
import Cocoa

extension NSImage {
    func resizeImage(_ width: CGFloat, _ height: CGFloat) -> NSImage? {

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

        if aspect >= 1 {
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

        let newImageRep = self.bestRepresentation(for: NSRect(x: 0, y: 0, width: newWidth, height: newHeight), context: nil, hints: nil)
        if newImageRep == nil {
            return nil
        }

        let thumbnail = NSImage(size: NSSize(width: newWidth, height: newHeight))
        thumbnail.addRepresentation(newImageRep!)

        return thumbnail
    }
}

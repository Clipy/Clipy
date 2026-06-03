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
        guard let newSize = aspectFitSize(width, height, allowsUpscaling: false),
              let cgImage = self.cgImage(forProposedRect: nil, context: nil, hints: nil) else {
            return nil
        }

        let originalPixelWidth = CGFloat(cgImage.width)
        let originalPixelHeight = CGFloat(cgImage.height)
        let ratio = min(newSize.width / size.width, newSize.height / size.height)
        let newPixelWidth = max(1, Int(floor(originalPixelWidth * ratio)))
        let newPixelHeight = max(1, Int(floor(originalPixelHeight * ratio)))

        let colorSpace = CGColorSpaceCreateDeviceRGB()
        let bitmapInfo = CGImageAlphaInfo.premultipliedLast.rawValue
        guard let context = CGContext(
            data: nil,
            width: newPixelWidth,
            height: newPixelHeight,
            bitsPerComponent: 8,
            bytesPerRow: 0,
            space: colorSpace,
            bitmapInfo: bitmapInfo
        ) else {
            return nil
        }

        context.interpolationQuality = .high
        context.draw(cgImage, in: CGRect(x: 0, y: 0, width: newPixelWidth, height: newPixelHeight))

        guard let resizedCGImage = context.makeImage() else {
            return nil
        }

        let thumbnail = NSImage(size: newSize)
        let bitmapRep = NSBitmapImageRep(cgImage: resizedCGImage)
        bitmapRep.size = newSize
        thumbnail.addRepresentation(bitmapRep)

        return thumbnail
    }

    func aspectFitImage(_ width: CGFloat, _ height: CGFloat) -> NSImage? {
        guard let newSize = aspectFitSize(width, height, allowsUpscaling: true),
              let image = copy() as? NSImage else {
            return nil
        }

        image.size = newSize
        return image
    }

    private func aspectFitSize(_ width: CGFloat, _ height: CGFloat, allowsUpscaling: Bool) -> NSSize? {
        guard width > 0, height > 0, size.width > 0, size.height > 0 else {
            return nil
        }

        let scale = min(width / size.width, height / size.height)
        let ratio = allowsUpscaling ? scale : min(scale, 1)
        return NSSize(
            width: max(1, floor(size.width * ratio)),
            height: max(1, floor(size.height * ratio))
        )
    }
}

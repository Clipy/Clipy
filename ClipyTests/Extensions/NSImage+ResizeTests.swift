//
//  NSImage+ResizeTests.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/06/03.
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import Testing
@testable import Clipy

@MainActor
@Suite
struct NSImageResizeTests {
    @Test
    func resizeImageDoesNotShrinkImagesThatAlreadyFit() throws {
        let image = NSImage.create(with: .blue, size: NSSize(width: 20, height: 10))

        let thumbnail = try #require(image.resizeImage(100, 32))

        #expect(thumbnail.size == NSSize(width: 20, height: 10))
    }

    @Test
    func aspectFitImageUsesMenuBoundsWithoutDistortingAspectRatio() throws {
        let wideImage = NSImage.create(with: .blue, size: NSSize(width: 20, height: 10))
        let squareImage = NSImage.create(with: .red, size: NSSize(width: 20, height: 20))

        let wideFittedImage = try #require(wideImage.aspectFitImage(100, 32))
        let squareFittedImage = try #require(squareImage.aspectFitImage(100, 32))

        #expect(wideFittedImage.size == NSSize(width: 64, height: 32))
        #expect(squareFittedImage.size == NSSize(width: 32, height: 32))
    }

    @Test
    func aspectFitImagesKeepAtLeastOnePointForExtremeAspectRatios() throws {
        let wideImage = NSImage.create(with: .blue, size: NSSize(width: 1_000, height: 1))
        let tallImage = NSImage.create(with: .red, size: NSSize(width: 1, height: 1_000))

        let wideThumbnail = try #require(wideImage.resizeImage(100, 32))
        let tallThumbnail = try #require(tallImage.resizeImage(100, 32))
        let wideFittedImage = try #require(wideImage.aspectFitImage(100, 32))
        let tallFittedImage = try #require(tallImage.aspectFitImage(100, 32))

        #expect(wideThumbnail.size == NSSize(width: 100, height: 1))
        #expect(tallThumbnail.size == NSSize(width: 1, height: 32))
        #expect(wideFittedImage.size == NSSize(width: 100, height: 1))
        #expect(tallFittedImage.size == NSSize(width: 1, height: 32))
    }
}

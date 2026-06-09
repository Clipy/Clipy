//
//  PasteboardContentTests.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/05/28.
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import Testing
@testable import Clipy

@MainActor
@Suite
struct PasteboardContentTests {
    @Test
    func typesAreDerivedFromAssetsInOrder() throws {
        let content = try #require(
            PasteboardContent(
                assets: [
                    PasteboardContent.Asset(type: .rtf, data: Data("rtf".utf8)),
                    PasteboardContent.Asset(type: .string, data: Data("Hello".utf8)),
                    PasteboardContent.Asset(type: .pdf, data: Data("pdf".utf8))
                ]
            )
        )
        #expect(content.types == [.rtf, .string, .pdf])
    }

    @Test
    func assetsInitializerFailsForEmptyAssets() {
        #expect(PasteboardContent(assets: []) == nil)
    }

    @Test
    func pasteboardInitializerSortsAssetsByRequestedTypes() throws {
        let pasteboard = NSPasteboard.withUniqueName()
        defer { pasteboard.clearContents() }

        let stringAsset = PasteboardContent.Asset(type: .string, data: Data("Hello".utf8))
        let firstPDFAsset = PasteboardContent.Asset(type: .pdf, data: Data("pdf1".utf8))
        let secondPDFAsset = PasteboardContent.Asset(type: .pdf, data: Data("pdf2".utf8))
        let copiedContent = try #require(
            PasteboardContent(
                assets: [
                    stringAsset,
                    firstPDFAsset,
                    secondPDFAsset
                ]
            )
        )
        copiedContent.writeObjects(to: pasteboard)

        let content = try #require(PasteboardContent(pasteboard: pasteboard, types: [.pdf, .string]))

        #expect(content.assets == [firstPDFAsset, secondPDFAsset, stringAsset])
    }

    @Test
    func pasteboardInitializerFallsBackToRootFilenames() throws {
        let pasteboard = NSPasteboard.withUniqueName()
        defer { pasteboard.clearContents() }

        let stringAsset = PasteboardContent.Asset(type: .string, data: Data("Hello".utf8))
        let tiffData = try #require(NSImage.create(with: .blue, size: NSSize(width: 4, height: 4)).tiffRepresentation)
        let tiffAsset = PasteboardContent.Asset(type: .tiff, data: tiffData)
        let filenamesAsset = try deprecatedFilenamesAsset(["/tmp/file.txt"])
        let copiedContent = try #require(
            PasteboardContent(
                assets: [
                    stringAsset,
                    tiffAsset,
                    filenamesAsset
                ]
            )
        )
        copiedContent.writeObjects(to: pasteboard)

        let content = try #require(PasteboardContent(
            pasteboard: pasteboard,
            types: [.tiff, .deprecatedFilenames, .string]
        ))

        #expect(content.assets == [tiffAsset, filenamesAsset, stringAsset])
    }

    @Test
    func writeObjectsWritesAndRestoresPasteboardItemsAndRootFilenames() throws {
        let pasteboard = NSPasteboard.withUniqueName()
        defer { pasteboard.clearContents() }

        let firstStringAsset = PasteboardContent.Asset(type: .string, data: Data("First".utf8))
        let firstPDFAsset = PasteboardContent.Asset(type: .pdf, data: Data("pdf1".utf8))
        let tiffData = try #require(NSImage.create(with: .blue, size: NSSize(width: 4, height: 4)).tiffRepresentation)
        let tiffAsset = PasteboardContent.Asset(type: .tiff, data: tiffData)
        let filenamesAsset = try deprecatedFilenamesAsset(["/tmp/first.txt", "/tmp/second.txt"])
        let secondStringAsset = PasteboardContent.Asset(type: .string, data: Data("Second".utf8))
        let secondPDFAsset = PasteboardContent.Asset(type: .pdf, data: Data("pdf2".utf8))
        let content = try #require(
            PasteboardContent(
                assets: [
                    firstStringAsset,
                    firstPDFAsset,
                    tiffAsset,
                    filenamesAsset,
                    secondStringAsset,
                    secondPDFAsset
                ]
            )
        )

        let items = content.pasteboardItems
        #expect(items.count == 2)
        #expect(items[0].types == [.string, .pdf, .tiff])
        #expect(items[1].types == [.string, .pdf])
        content.writeObjects(to: pasteboard)
        #expect(pasteboard.data(forType: .deprecatedFilenames) != nil)

        let restoredContent = try #require(PasteboardContent(
            pasteboard: pasteboard,
            types: [.string, .pdf, .deprecatedFilenames, .tiff]
        ))
        #expect(
            restoredContent.assets == [
                firstStringAsset,
                secondStringAsset,
                firstPDFAsset,
                secondPDFAsset,
                filenamesAsset,
                tiffAsset
            ]
        )
    }

    @Test
    func imageInitializerStoresTiffAsset() throws {
        let image = NSImage.create(with: .red, size: NSSize(width: 10, height: 10))
        let content = try #require(PasteboardContent(image: image))

        #expect(content.types == [.tiff])
        #expect(content.assets.count == 1)
        #expect(content.assets.first?.type == .tiff)
        #expect(content.assets.first?.data.isEmpty == false)
    }

    @Test
    func stringPropertiesUseModernAndDeprecatedStringData() throws {
        let modernContent = try #require(
            PasteboardContent(
                assets: [
                    PasteboardContent.Asset(type: .string, data: Data("Hello".utf8))
                ]
            )
        )
        let deprecatedContent = try #require(
            PasteboardContent(
                assets: [
                    PasteboardContent.Asset(type: .deprecatedString, data: Data("Legacy".utf8))
                ]
            )
        )
        let mixedContent = try #require(
            PasteboardContent(
                assets: [
                    PasteboardContent.Asset(type: .string, data: Data("Hello".utf8)),
                    PasteboardContent.Asset(type: .rtf, data: Data("rtf".utf8))
                ]
            )
        )

        #expect(modernContent.isOnlyStringType)
        #expect(modernContent.stringValue == "Hello")
        #expect(deprecatedContent.isOnlyStringType)
        #expect(deprecatedContent.stringValue == "Legacy")
        #expect(!mixedContent.isOnlyStringType)
        #expect(mixedContent.stringValue == "Hello")
    }

    @Test
    func colorCodeImageIsCreatedFromHexString() throws {
        let colorContent = try #require(
            PasteboardContent(
                assets: [
                    PasteboardContent.Asset(type: .string, data: Data("#ff0000".utf8))
                ]
            )
        )
        let invalidContent = try #require(
            PasteboardContent(
                assets: [
                    PasteboardContent.Asset(type: .string, data: Data("not a color".utf8))
                ]
            )
        )

        #expect(colorContent.colorCodeImage?.size == NSSize(width: 20, height: 20))
        #expect(invalidContent.colorCodeImage == nil)
    }

    @Test
    func thumbnailImageIsCreatedFromStoredTiffData() {
        let defaults = UserDefaults.standard
        let previousWidth = defaults.object(forKey: Constants.UserDefaults.thumbnailWidth)
        let previousHeight = defaults.object(forKey: Constants.UserDefaults.thumbnailHeight)
        defer {
            if let previousWidth {
                defaults.set(previousWidth, forKey: Constants.UserDefaults.thumbnailWidth)
            } else {
                defaults.removeObject(forKey: Constants.UserDefaults.thumbnailWidth)
            }
            if let previousHeight {
                defaults.set(previousHeight, forKey: Constants.UserDefaults.thumbnailHeight)
            } else {
                defaults.removeObject(forKey: Constants.UserDefaults.thumbnailHeight)
            }
        }
        defaults.set(8, forKey: Constants.UserDefaults.thumbnailWidth)
        defaults.set(6, forKey: Constants.UserDefaults.thumbnailHeight)

        let image = NSImage.create(with: .blue, size: NSSize(width: 20, height: 10))
        let content = PasteboardContent(image: image)

        #expect(content?.thumbnailImage?.size == NSSize(width: 8, height: 4))
    }

    @Test
    func contentHashIsStableAndContentBased() throws {
        let content = try #require(
            PasteboardContent(
                assets: [
                    PasteboardContent.Asset(type: .string, data: Data("Hello".utf8)),
                    PasteboardContent.Asset(type: .rtf, data: Data("rtf".utf8))
                ]
            )
        )
        let equivalentContent = try #require(
            PasteboardContent(
                assets: [
                    PasteboardContent.Asset(type: .string, data: Data("Hello".utf8)),
                    PasteboardContent.Asset(type: .rtf, data: Data("rtf".utf8))
                ]
            )
        )
        let changedDataContent = try #require(
            PasteboardContent(
                assets: [
                    PasteboardContent.Asset(type: .string, data: Data("Hello!".utf8)),
                    PasteboardContent.Asset(type: .rtf, data: Data("rtf".utf8))
                ]
            )
        )
        let changedOrderContent = try #require(
            PasteboardContent(
                assets: [
                    PasteboardContent.Asset(type: .rtf, data: Data("rtf".utf8)),
                    PasteboardContent.Asset(type: .string, data: Data("Hello".utf8))
                ]
            )
        )

        #expect(content.hash == "4c6a4ba3cd6a6aad6a2c6620542b11c94edf2af3297611aeba21a86e79dbeb20")
        #expect(content.hash == equivalentContent.hash)
        #expect(content.hash != changedDataContent.hash)
        #expect(content.hash != changedOrderContent.hash)
    }
}

private func deprecatedFilenamesAsset(_ filenames: [String]) throws -> PasteboardContent.Asset {
    PasteboardContent.Asset(
        type: .deprecatedFilenames,
        data: try PropertyListSerialization.data(
            fromPropertyList: filenames,
            format: .xml,
            options: 0
        )
    )
}

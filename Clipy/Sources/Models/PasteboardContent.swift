//
//  PasteboardContent.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/05/28.
//
//  Copyright © 2015-2026 Clipy Project.
//

import Cocoa
import CryptoKit
import Sharing
import SwiftHEXColors

struct PasteboardContent: Equatable {
    struct Asset: Equatable {
        let type: NSPasteboard.PasteboardType
        let data: Data
    }

    // MARK: - Properties
    let types: [NSPasteboard.PasteboardType]
    let assets: [Asset]
    let hash: String

    var isBlankText: Bool {
        guard
            let primaryType = types.first,
            [.string, .deprecatedString, .rtf, .deprecatedRTF].contains(primaryType),
            let stringValue
        else {
            return false
        }
        return stringValue.trimmingCharacters(in: .whitespacesAndNewlines).isEmpty
    }
    var stringValue: String? {
        guard let data = data(for: .string) ?? data(for: .deprecatedString) else { return nil }
        return String(data: data, encoding: .utf8)
    }
    var colorCodeImage: NSImage? {
        guard let stringValue, let color = NSColor(hexString: stringValue) else { return nil }
        return NSImage.create(with: color, size: NSSize(width: 20, height: 20))
    }
    var thumbnailImage: NSImage? {
        @Shared(.thumbnailWidth) var thumbnailWidth
        @Shared(.thumbnailHeight) var thumbnailHeight

        guard let imageData else { return nil }
        return NSImage(data: imageData)?.resizeImage(CGFloat(thumbnailWidth), CGFloat(thumbnailHeight))
    }
    var imageData: Data? {
        if let imageFileURL {
            return try? Data(contentsOf: imageFileURL)
        }
        return storedImageData
    }
    var pasteboardItems: [NSPasteboardItem] {
        var countsByType: [NSPasteboard.PasteboardType: Int] = [:]
        // History assets intentionally do not persist the original NSPasteboardItem boundaries.
        // Replaying groups assets by each pasteboard type's occurrence index, which keeps
        // multi-item file writes working without making item boundaries part of the schema.
        return assets
            .filter { $0.type != .deprecatedFilenames }
            .reduce(into: [NSPasteboardItem]()) { items, asset in
                let index = countsByType[asset.type] ?? 0
                countsByType[asset.type] = index + 1
                if !items.indices.contains(index) {
                    items.append(NSPasteboardItem())
                }
                items[index].setData(asset.data, forType: asset.type)
            }
    }

    // MARK: - Initialize
    init?(assets: [Asset]) {
        guard !assets.isEmpty else { return nil }
        self.types = assets.map(\.type)
        self.assets = assets
        var data = Data()
        assets.forEach { asset in
            data.append(value: Data(asset.type.rawValue.utf8))
            data.append(value: asset.data)
        }
        self.hash = SHA256.hash(data: data)
            .map { String(format: "%02x", $0) }
            .joined()
    }

    init?(pasteboard: NSPasteboard, types: [NSPasteboard.PasteboardType]) {
        let itemAssets = pasteboard.pasteboardItems?.compactMap { item -> [Asset]? in
            item.types.filter { types.contains($0) }
                .compactMap { type -> Asset? in
                    guard let data = item.data(forType: type) else { return nil }
                    return Asset(type: type, data: data)
                }
        }
        .flatMap { $0 } ?? []
        // Fall back to the pasteboard root for types that may only be available there,
        // such as .tiff and .deprecatedFilenames.
        let itemAssetTypes = itemAssets.map(\.type)
        let rootAssets = types
            .filter { !itemAssetTypes.contains($0) }
            .compactMap { type -> Asset? in
                guard let data = pasteboard.data(forType: type) else { return nil }
                return Asset(type: type, data: data)
            }
        let assets = (itemAssets + rootAssets).sorted(by: types)
        guard !assets.isEmpty else { return nil }
        self.init(assets: assets)
    }

    init?(image: NSImage) {
        guard let data = image.tiffRepresentation else { return nil }
        self.init(assets: [Asset(type: .tiff, data: data)])
    }
}

extension PasteboardContent {
    func writeObjects(to pasteboard: NSPasteboard) {
        let pasteboardItems = self.pasteboardItems
        let filenamesAsset = self.assets.first(where: { $0.type == .deprecatedFilenames })

        pasteboard.clearContents()
        // File URLs are normally written as per-item fileURL data.
        // Keep NSFilenamesPboardType on the pasteboard root for legacy history entries
        // and apps that only understand the deprecated filenames flavor.
        if let filenamesAsset {
            pasteboard.setData(filenamesAsset.data, forType: filenamesAsset.type)
        }
        pasteboard.writeObjects(pasteboardItems)
    }
}

private extension PasteboardContent {
    var storedImageData: Data? {
        data(for: .png) ?? data(for: .tiff) ?? data(for: .deprecatedTIFF)
    }

    var imageFileURL: URL? {
        assets.filter { $0.type == .fileURL }
            .compactMap { URL(dataRepresentation: $0.data, relativeTo: nil) }
            .first { ["jpg", "jpeg", "png", "bmp", "tiff"].contains($0.pathExtension.lowercased()) }
    }

    func data(for type: NSPasteboard.PasteboardType) -> Data? {
        assets.first(where: { $0.type == type })?.data
    }
}

private extension [PasteboardContent.Asset] {
    func sorted(by types: [NSPasteboard.PasteboardType]) -> Self {
        types.flatMap { type in
            filter { $0.type == type }
        }
    }
}

private extension Data {
    mutating func append(value: Data) {
        var length = UInt64(value.count).bigEndian
        Swift.withUnsafeBytes(of: &length) {
            append(contentsOf: $0)
        }
        append(value)
    }
}

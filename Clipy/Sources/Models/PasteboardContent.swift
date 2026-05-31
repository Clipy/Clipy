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

    var isOnlyStringType: Bool {
        types == [.string] || types == [.deprecatedString]
    }
    var stringValue: String {
        guard let data = data(for: .string) ?? data(for: .deprecatedString) else { return "" }
        return String(data: data, encoding: .utf8) ?? ""
    }
    var fileNames: [String] {
        propertyList(for: .fileURL) ?? propertyList(for: .deprecatedFilenames) ?? []
    }
    var colorCodeImage: NSImage? {
        guard let color = NSColor(hexString: stringValue) else { return nil }
        return NSImage.create(with: color, size: NSSize(width: 20, height: 20))
    }
    var thumbnailImage: NSImage? {
        let defaults = UserDefaults.standard
        let width = defaults.integer(forKey: Constants.UserDefaults.thumbnailWidth)
        let height = defaults.integer(forKey: Constants.UserDefaults.thumbnailHeight)

        if let data = data(for: .tiff) ?? data(for: .deprecatedTIFF), fileNames.isEmpty {
            return NSImage(data: data)?.resizeImage(CGFloat(width), CGFloat(height))
        } else if let fileName = fileNames.first {
            switch URL(fileURLWithPath: fileName).pathExtension.lowercased() {
            case "jpg", "jpeg", "png", "bmp", "tiff":
                return NSImage(contentsOfFile: fileName)?.resizeImage(CGFloat(width), CGFloat(height))
            default:
                break
            }
        }
        return nil
    }

    // MARK: - Initialize
    init(assets: [Asset]) {
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
        let assets = types.compactMap { type -> Asset? in
            guard let data = pasteboard.data(forType: type) else { return nil }
            return Asset(type: type, data: data)
        }
        guard !assets.isEmpty else { return nil }
        self.init(assets: assets)
    }

    init?(image: NSImage) {
        guard let data = image.tiffRepresentation else { return nil }
        self.init(assets: [Asset(type: .tiff, data: data)])
    }
}

private extension PasteboardContent {
    func data(for type: NSPasteboard.PasteboardType) -> Data? {
        assets.first(where: { $0.type == type })?.data
    }

    func propertyList<T>(for type: NSPasteboard.PasteboardType) -> T? {
        guard let data = data(for: type) else { return nil }
        return (try? PropertyListSerialization.propertyList(from: data, options: [], format: nil)) as? T
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

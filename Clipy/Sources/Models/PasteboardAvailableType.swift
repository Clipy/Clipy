//
//  PasteboardAvailableType.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Shunsuke Furubayashi on 2026/05/31.
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import Collections
import Foundation

enum PasteboardAvailableType: String, Equatable, CaseIterable {
    case string = "String"
    case rtf = "RTF"
    case rtfd = "RTFD"
    case pdf = "PDF"
    case filenames = "Filenames"
    case url = "URL"
    case tiff = "TIFF"

    static func availableTypes(
        from pasteboardTypes: [NSPasteboard.PasteboardType],
        storeAvailableTypes: [PasteboardAvailableType]
    ) -> [NSPasteboard.PasteboardType] {
        let uniquePasteboardTypes = OrderedSet(pasteboardTypes)
        return uniquePasteboardTypes.compactMap { pasteboardType -> NSPasteboard.PasteboardType? in
            guard let availableType = pasteboardType.availableType,
                storeAvailableTypes.contains(availableType) else { return nil }
            if pasteboardType.isCovered(by: uniquePasteboardTypes) {
                return nil
            }
            if let modernType = pasteboardType.modernType,
               uniquePasteboardTypes.contains(modernType) {
                return nil
            }
            return pasteboardType
        }
    }
}

private extension NSPasteboard.PasteboardType {
    var availableType: PasteboardAvailableType? {
        switch self {
        case .string, .deprecatedString:
            return .string
        case .rtf, .deprecatedRTF:
            return .rtf
        case .rtfd, .deprecatedRTFD:
            return .rtfd
        case .pdf, .deprecatedPDF:
            return .pdf
        case .fileURL, .deprecatedFilenames:
            return .filenames
        case .URL, .deprecatedURL:
            return .url
        case .png, .tiff, .deprecatedTIFF:
            return .tiff
        default:
            return nil
        }
    }

    var modernType: NSPasteboard.PasteboardType? {
        switch self {
        case .deprecatedString:
            return .string
        case .deprecatedRTF:
            return .rtf
        case .deprecatedRTFD:
            return .rtfd
        case .deprecatedPDF:
            return .pdf
        case .deprecatedURL:
            return .URL
        case .deprecatedFilenames:
            return .fileURL
        case .deprecatedTIFF:
            return .tiff
        default:
            return nil
        }
    }

    func isCovered(by pasteboardTypes: OrderedSet<NSPasteboard.PasteboardType>) -> Bool {
        switch self {
        case .tiff, .deprecatedTIFF:
            // Prefer PNG when it is available. Undefined image types can still be
            // recovered as TIFF from the pasteboard root fallback.
            return pasteboardTypes.contains(.png)
        default:
            return false
        }
    }
}

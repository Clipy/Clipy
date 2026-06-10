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
        storeAvailableTypes: [PasteboardAvailableType],
        ignoresConcealedType: Bool
    ) -> [NSPasteboard.PasteboardType] {
        let uniquePasteboardTypes = OrderedSet(pasteboardTypes)
        // Do not save pasteboards marked as temporary.
        guard uniquePasteboardTypes.allSatisfy({ $0 != .transient }) else { return [] }
        // When concealed pasteboards are ignored, do not save items containing sensitive data.
        guard !ignoresConcealedType || uniquePasteboardTypes.allSatisfy({ $0 != .concealed }) else { return [] }
        // Universal Clipboard file URLs can point to Apple-managed temporary storage,
        // such as `Group Containers/group.com.apple.coreservices.useractivityd`.
        // When a non-Apple app stores and reuses that file URL, macOS may fail to
        // provide the required sandbox extension, so the paste target cannot open the
        // file. If the file URL is the primary data, do not save the history item.
        // Otherwise, drop only the file URL and keep other image or text representations.
        let isUniversalClipboard = uniquePasteboardTypes.contains(.universalClipboard)
        if isUniversalClipboard && uniquePasteboardTypes.first?.isFileReference == true {
            return []
        }
        let availableTypes = uniquePasteboardTypes.compactMap { pasteboardType -> NSPasteboard.PasteboardType? in
            if isUniversalClipboard && pasteboardType.isFileReference {
                return nil
            }
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
        guard !availableTypes.isEmpty else { return [] }
        if uniquePasteboardTypes.contains(.concealed) {
            return availableTypes + [.concealed]
        }
        return availableTypes
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

    var isFileReference: Bool {
        self == .fileURL || self == .deprecatedFilenames
    }
}

extension NSPasteboard.PasteboardType {
    static let universalClipboard = NSPasteboard.PasteboardType(rawValue: "com.apple.is-remote-clipboard")
}

// ref: https://nspasteboard.org/
extension NSPasteboard.PasteboardType {
    static let transient = NSPasteboard.PasteboardType(rawValue: "org.nspasteboard.TransientType")
    static let concealed = NSPasteboard.PasteboardType(rawValue: "org.nspasteboard.ConcealedType")
}

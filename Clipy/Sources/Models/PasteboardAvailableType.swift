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
        let availableTypes = storeAvailableTypes.compactMap { availableType -> NSPasteboard.PasteboardType? in
            switch availableType {
            case .string where pasteboardTypes.contains(.string):
                return .string
            case .string where pasteboardTypes.contains(.deprecatedString):
                return .deprecatedString

            case .rtf where pasteboardTypes.contains(.rtf):
                return .rtf
            case .rtf where pasteboardTypes.contains(.deprecatedRTF):
                return .deprecatedRTF

            case .rtfd where pasteboardTypes.contains(.rtfd):
                return .rtfd
            case .rtfd where pasteboardTypes.contains(.deprecatedRTFD):
                return .deprecatedRTFD

            case .pdf where pasteboardTypes.contains(.pdf):
                return .pdf
            case .pdf where pasteboardTypes.contains(.deprecatedPDF):
                return .deprecatedPDF

            case .filenames where pasteboardTypes.contains(.fileURL):
                return .fileURL
            case .filenames where pasteboardTypes.contains(.deprecatedFilenames):
                return .deprecatedFilenames

            case .url where pasteboardTypes.contains(.URL):
                return .URL
            case .url where pasteboardTypes.contains(.deprecatedURL):
                return .deprecatedURL

            case .tiff where pasteboardTypes.contains(.tiff):
                return .tiff
            case .tiff where pasteboardTypes.contains(.deprecatedTIFF):
                return .deprecatedTIFF

            default:
                return nil
            }
        }
        return availableTypes.sorted {
            guard let lhsIndex = pasteboardTypes.firstIndex(of: $0),
                  let rhsIndex = pasteboardTypes.firstIndex(of: $1) else { return false }
            return lhsIndex < rhsIndex
        }
    }
}

// 
//  AvailableType.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
// 
//  Created by Econa77 on 2018/11/25.
// 
//  Copyright © 2015-2018 Clipy Project.
//

import Foundation
import AppKit

enum AvailableType: String {
    case string = "String"
    case rtf = "RTF"
    case rtfd = "RTFD"
    case pdf = "PDF"
    case filenames = "Filenames"
    case url = "URL"
    case tiff = "TIFF"

    // MARK: - Properties
    // TODO: - Replace with CaseIterable after migrating to swift 4.2
    static var allCases: [AvailableType] {
        return [.string, .rtf, .rtfd, .pdf, .filenames, .url, .tiff]
    }

    static func available(by pasteboardType: NSPasteboard.PasteboardType) -> AvailableType? {
        for availableType in AvailableType.allCases {
            guard availableType.targetPasteboardTypes.contains(pasteboardType) else { continue }
            return availableType
        }
        return nil
    }

    var targetPasteboardTypes: [NSPasteboard.PasteboardType] {
        switch self {
        case .string:
            return [.deprecatedString, .string]
        case .rtf:
            return [.deprecatedRTF, .rtf]
        case .rtfd:
            return [.deprecatedRTFD, .rtfd]
        case .pdf:
            return [.deprecatedPDF, .pdf]
        case .filenames:
            if #available(OSX 10.13, *) {
                return [.deprecatedFilenames, .fileURL]
            } else {
                return [.deprecatedFilenames]
            }
        case .url:
            if #available(OSX 10.13, *) {
                return [.deprecatedURL, .URL]
            } else {
                return [.deprecatedURL]
            }
        case .tiff:
            return [.deprecatedTIFF, .tiff]
        }
    }

}

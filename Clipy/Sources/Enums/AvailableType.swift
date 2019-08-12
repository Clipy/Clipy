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

enum AvailableType: String, CaseIterable {
    case string = "String"
    case rtf = "RTF"
    case rtfd = "RTFD"
    case pdf = "PDF"
    case filenames = "Filenames"
    case url = "URL"
    case tiff = "TIFF"

    // MARK: - Properties
    var isString: Bool {
        return self == .string
    }

    var isRTF: Bool {
        return self == .rtf
    }

    var isRTFD: Bool {
        return self == .rtfd
    }

    var isTIFF: Bool {
        return self == .tiff
    }

    var isFilenames: Bool {
        return self == .filenames
    }

    var isPDF: Bool {
        return self == .pdf
    }

    var isURL: Bool {
        return self == .url
    }

    static func available(by pasteboardType: NSPasteboard.PasteboardType) -> AvailableType? {
        for availableType in AvailableType.allCases {
            guard availableType.targetPbTypes.contains(pasteboardType) else { continue }
            return availableType
        }
        return nil
    }

    var targetPbTypes: [NSPasteboard.PasteboardType] {
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

    var primaryPbType: NSPasteboard.PasteboardType {
        return targetPbTypes.last!
    }
}

fileprivate extension NSPasteboard.PasteboardType {
    static var deprecatedString: NSPasteboard.PasteboardType {
        return NSPasteboard.PasteboardType(rawValue: "NSStringPboardType")
    }

    static var deprecatedRTF: NSPasteboard.PasteboardType {
        return NSPasteboard.PasteboardType(rawValue: "NSRTFPboardType")
    }

    static var deprecatedRTFD: NSPasteboard.PasteboardType {
        return NSPasteboard.PasteboardType(rawValue: "NSRTFDPboardType")
    }

    static var deprecatedPDF: NSPasteboard.PasteboardType {
        return NSPasteboard.PasteboardType(rawValue: "NSPDFPboardType")
    }

    static var deprecatedFilenames: NSPasteboard.PasteboardType {
        return NSPasteboard.PasteboardType(rawValue: "NSFilenamesPboardType")
    }

    static var deprecatedURL: NSPasteboard.PasteboardType {
        return NSPasteboard.PasteboardType(rawValue: "NSURLPboardType")
    }

    static var deprecatedTIFF: NSPasteboard.PasteboardType {
        return NSPasteboard.PasteboardType(rawValue: "NSTIFFPboardType")
    }
}

extension NSPasteboard.PasteboardType {

    var isString: Bool {
        return AvailableType.available(by: self)?.isString ?? false
    }

    var isRTF: Bool {
        return AvailableType.available(by: self)?.isRTF ?? false
    }

    var isRTFD: Bool {
        return AvailableType.available(by: self)?.isRTFD ?? false
    }

    var isTIFF: Bool {
        return AvailableType.available(by: self)?.isTIFF ?? false
    }

    var isPDF: Bool {
        return AvailableType.available(by: self)?.isPDF ?? false
    }

    var isFilenames: Bool {
        return AvailableType.available(by: self)?.isFilenames ?? false
    }

    var isURL: Bool {
        return AvailableType.available(by: self)?.isURL ?? false
    }
}

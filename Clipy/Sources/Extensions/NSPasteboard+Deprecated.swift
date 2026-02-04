//
//  NSPasteboard+Deprecated.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2017/12/30.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Cocoa

/**
 *  The contents of PasteboardType has been changed with swift 4.
 *  However, we will use the swift 3 style to keep compatibility with existing items
 *  Help wanted - If there is a good implementation I would like to replace it.
 **/
extension NSPasteboard.PasteboardType {

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

    // MARK: - Modern UTI Type Mapping
    // Modern macOS uses UTI-based type strings (e.g., "public.tiff") instead of
    // the old NS-style strings (e.g., "NSTIFFPboardType").
    // This mapping allows Clipy to recognize modern types while keeping backward
    // compatibility with existing stored clip data.
    static var modernToDeprecatedMap: [NSPasteboard.PasteboardType: NSPasteboard.PasteboardType] {
        return [
            .init(rawValue: "public.utf8-plain-text"): .deprecatedString,
            .init(rawValue: "public.rtf"): .deprecatedRTF,
            .init(rawValue: "com.apple.flat-rtfd"): .deprecatedRTFD,
            .init(rawValue: "com.adobe.pdf"): .deprecatedPDF,
            .init(rawValue: "public.tiff"): .deprecatedTIFF,
            .init(rawValue: "public.url"): .deprecatedURL,
        ]
    }

    /// Normalizes a modern UTI type to its deprecated equivalent for internal consistency.
    /// Returns self if no mapping exists.
    var normalizedType: NSPasteboard.PasteboardType {
        return NSPasteboard.PasteboardType.modernToDeprecatedMap[self] ?? self
    }

}

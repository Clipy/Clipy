//
//  NSPasteboard+Deprecated.swift
//  Clipy
//
//  Created by Shunsuke Furubayashi on 2017/12/30.
//  Copyright © 2017年 Shunsuke Furubayashi. All rights reserved.
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

}

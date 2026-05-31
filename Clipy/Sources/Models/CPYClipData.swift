//
//  CPYClipData.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2015/06/21.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Cocoa
import SwiftHEXColors

final class CPYClipData: NSObject {

    // MARK: - Properties
    fileprivate let kTypesKey       = "types"
    fileprivate let kStringValueKey = "stringValue"
    fileprivate let kRTFDataKey     = "RTFData"
    fileprivate let kPDFKey         = "PDF"
    fileprivate let kFileNamesKey   = "filenames"
    fileprivate let kURLsKey        = "URL"
    fileprivate let kImageKey       = "image"

    var types          = [NSPasteboard.PasteboardType]()
    var fileNames      = [String]()
    var URLs           = [String]()
    var stringValue    = ""
    var RTFData: Data?
    var PDF: Data?
    var image: NSImage?

    // MARK: - NSCoding
    @objc required init(coder aDecoder: NSCoder) {
        types = (aDecoder.decodeObject(forKey: kTypesKey) as? [String])?.compactMap { NSPasteboard.PasteboardType(rawValue: $0) } ?? []
        fileNames = aDecoder.decodeObject(forKey: kFileNamesKey) as? [String] ?? [String]()
        URLs = aDecoder.decodeObject(forKey: kURLsKey) as? [String] ?? [String]()
        stringValue = aDecoder.decodeObject(forKey: kStringValueKey) as? String ?? ""
        RTFData = aDecoder.decodeObject(forKey: kRTFDataKey) as? Data
        PDF = aDecoder.decodeObject(forKey: kPDFKey) as? Data
        image = aDecoder.decodeObject(forKey: kImageKey) as? NSImage
        super.init()
    }
}

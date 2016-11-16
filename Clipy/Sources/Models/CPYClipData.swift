//
//  CPYClipData.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/21.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa

final class CPYClipData: NSObject {

    // MARK: - Properties
    fileprivate let kTypesKey       = "types"
    fileprivate let kStringValueKey = "stringValue"
    fileprivate let kRTFDataKey     = "RTFData"
    fileprivate let kPDFKey         = "PDF"
    fileprivate let kFileNamesKey   = "filenames"
    fileprivate let kURLsKey        = "URL"
    fileprivate let kImageKey       = "image"

    var types          = [String]()
    var fileNames      = [String]()
    var URLs           = [String]()
    var stringValue    = ""
    var RTFData: Data?
    var PDF: Data?
    var image: NSImage?

    override var hash: Int {
        var hash = types.joined(separator: "").hash
        if let image = self.image, let imageData = image.tiffRepresentation {
            hash ^= imageData.count
        } else if let image = self.image {
            hash ^= image.hash
        }
        if !fileNames.isEmpty {
            fileNames.forEach { hash ^= $0.hash }
        } else if !self.URLs.isEmpty {
            URLs.forEach { hash ^= $0.hash }
        } else if let pdf = PDF {
            hash ^= pdf.count
        } else if !stringValue.isEmpty {
            hash ^= stringValue.hash
        }
        if let data = RTFData {
            hash ^= data.count
        }
        return hash
    }
    var primaryType: String? {
        return types.first
    }
    var isOnlyStringType: Bool {
        return types == [NSStringPboardType]
    }
    static var availableTypes: [String] {
        return [NSStringPboardType, NSRTFPboardType, NSRTFDPboardType, NSPDFPboardType, NSFilenamesPboardType, NSURLPboardType, NSTIFFPboardType]
    }
    static var availableTypesString: [String] {
        return ["String", "RTF", "RTFD", "PDF", "Filenames", "URL", "TIFF"]
    }
    static var availableTypesDictinary: [String: String] {
        var availableTypes = [String: String]()
        zip(CPYClipData.availableTypes, CPYClipData.availableTypesString).forEach { availableTypes[$0] = $1 }
        return availableTypes
    }

    // MARK: - Init
    override init () {
        super.init()
    }

    init(pasteboard: NSPasteboard, types: [String]) {
        super.init()
        self.types = types
        types.forEach { type in
            switch type {
            case NSStringPboardType:
                if let pbString = pasteboard.string(forType: NSStringPboardType) {
                    stringValue = pbString
                }
            case NSRTFDPboardType:
                RTFData = pasteboard.data(forType: NSRTFDPboardType)
            case NSRTFPboardType where RTFData == nil:
                RTFData = pasteboard.data(forType: NSRTFPboardType)
            case NSPDFPboardType:
                PDF = pasteboard.data(forType: NSPDFPboardType)
            case NSFilenamesPboardType:
                if let fileNames = pasteboard.propertyList(forType: NSFilenamesPboardType) as? [String] {
                    self.fileNames = fileNames
                }
            case NSURLPboardType:
                if let url = pasteboard.propertyList(forType: NSURLPboardType) as? [String] {
                    URLs = url
                }
            case NSTIFFPboardType:
                if NSImage.canInit(with: pasteboard) {
                    image =  NSImage(pasteboard: pasteboard)
                }
            default: break
            }
        }
    }

    init(image: NSImage) {
        self.types = [NSTIFFPboardType]
        self.image = image
    }

    deinit {
        self.RTFData = nil
        self.PDF = nil
        self.image = nil
    }

    // MARL- Archiving
    func encodeWithCoder(_ aCoder: NSCoder) {
        aCoder.encode(types, forKey: kTypesKey)
        aCoder.encode(stringValue, forKey: kStringValueKey)
        aCoder.encode(RTFData, forKey: kRTFDataKey)
        aCoder.encode(PDF, forKey: kPDFKey)
        aCoder.encode(fileNames, forKey: kFileNamesKey)
        aCoder.encode(URLs, forKey: kURLsKey)
        aCoder.encode(image, forKey: kImageKey)
    }

    required init(coder aDecoder: NSCoder) {
        types          = aDecoder.decodeObject(forKey: kTypesKey)        as? [String] ?? [String]()
        fileNames      = aDecoder.decodeObject(forKey: kFileNamesKey)    as? [String] ?? [String]()
        URLs           = aDecoder.decodeObject(forKey: kURLsKey)         as? [String] ?? [String]()
        stringValue    = aDecoder.decodeObject(forKey: kStringValueKey)  as? String ?? ""
        RTFData        = aDecoder.decodeObject(forKey: kRTFDataKey)      as? Data
        PDF            = aDecoder.decodeObject(forKey: kPDFKey)          as? Data
        image          = aDecoder.decodeObject(forKey: kImageKey)        as? NSImage
        super.init()
    }
}

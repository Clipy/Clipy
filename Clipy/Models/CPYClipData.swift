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
    private let kTypesKey       = "types"
    private let kStringValueKey = "stringValue"
    private let kRTFDataKey     = "RTFData"
    private let kPDFKey         = "PDF"
    private let kFileNamesKey   = "filenames"
    private let kURLsKey        = "URL"
    private let kImageKey       = "image"

    var types          = [String]()
    var fileNames      = [String]()
    var URLs           = [String]()
    var stringValue    = ""
    var RTFData: NSData?
    var PDF: NSData?
    var image: NSImage?

    override var hash: Int {
        var hash = types.joinWithSeparator("").hash
        if let image = self.image {
            hash ^= image.TIFFRepresentation!.length
        }
        if !fileNames.isEmpty {
            fileNames.forEach { hash ^= $0.hash }
        } else if !self.URLs.isEmpty {
            URLs.forEach { hash ^= $0.hash }
        } else if let pdf = PDF {
            hash ^= pdf.length
        } else if !stringValue.isEmpty {
            hash ^= stringValue.hash
        }
        if let data = RTFData {
            hash ^= data.length
        }
        return hash
    }
    var primaryType: String? {
        return types.first
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
                if let pbString = pasteboard.stringForType(NSStringPboardType) {
                    stringValue = pbString
                }
            case NSRTFDPboardType:
                RTFData = pasteboard.dataForType(NSRTFDPboardType)
            case NSRTFPboardType where RTFData == nil:
                RTFData = pasteboard.dataForType(NSRTFPboardType)
            case NSPDFPboardType:
                PDF = pasteboard.dataForType(NSPDFPboardType)
            case NSFilenamesPboardType:
                if let fileNames = pasteboard.propertyListForType(NSFilenamesPboardType) as? [String] {
                    self.fileNames = fileNames
                }
            case NSURLPboardType:
                if let url = pasteboard.propertyListForType(NSURLPboardType) as? [String] {
                    URLs = url
                }
            case NSTIFFPboardType:
                if NSImage.canInitWithPasteboard(pasteboard) {
                    image =  NSImage(pasteboard: pasteboard)
                }
            default: break
            }
        }
    }

    deinit {
        self.RTFData = nil
        self.PDF = nil
        self.image = nil
    }

    // MARL- Archiving
    func encodeWithCoder(aCoder: NSCoder) {
        aCoder.encodeObject(types, forKey: kTypesKey)
        aCoder.encodeObject(stringValue, forKey: kStringValueKey)
        aCoder.encodeObject(RTFData, forKey: kRTFDataKey)
        aCoder.encodeObject(PDF, forKey: kPDFKey)
        aCoder.encodeObject(fileNames, forKey: kFileNamesKey)
        aCoder.encodeObject(URLs, forKey: kURLsKey)
        aCoder.encodeObject(image, forKey: kImageKey)
    }

    required init(coder aDecoder: NSCoder) {
        types          = aDecoder.decodeObjectForKey(kTypesKey)        as? [String] ?? [String]()
        fileNames      = aDecoder.decodeObjectForKey(kFileNamesKey)    as? [String] ?? [String]()
        URLs           = aDecoder.decodeObjectForKey(kURLsKey)         as? [String] ?? [String]()
        stringValue    = aDecoder.decodeObjectForKey(kStringValueKey)  as? String ?? ""
        RTFData        = aDecoder.decodeObjectForKey(kRTFDataKey)      as? NSData
        PDF            = aDecoder.decodeObjectForKey(kPDFKey)          as? NSData
        image          = aDecoder.decodeObjectForKey(kImageKey)        as? NSImage
        super.init()
    }
}

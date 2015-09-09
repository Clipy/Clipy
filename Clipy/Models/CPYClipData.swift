//
//  CPYClipData.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/21.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa

class CPYClipData: NSObject {

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
    var RTFData:   NSData?
    var PDF:       NSData?
    var image:     NSImage?
    
    override var hash: Int {
        var hash = 0
        hash = (self.types as NSArray).componentsJoinedByString("").hash
        if self.image != nil {
            hash ^= self.image!.TIFFRepresentation!.length
        }
        if !self.fileNames.isEmpty {
            for fileName in self.fileNames {
                hash ^= fileName.hash
            }
        } else if !self.URLs.isEmpty {
            for aURL in self.URLs {
                hash ^= aURL.hash
            }
        } else if self.PDF != nil {
            hash ^= self.PDF!.length
        } else if self.stringValue != "" {
            hash ^= self.stringValue.hash
        }
        if self.RTFData != nil {
            hash ^= self.RTFData!.length
        }
        return hash
    }
    var primaryType: String? {
        if self.types.count <= 0 {
            return nil
        }
        return self.types[0]
    }
    
    // MARK: - Init
    override init () {
        super.init()
    }
    
    deinit {
        self.RTFData = nil
        self.PDF = nil
        self.image = nil
    }
    
    // MARL- Archiving
    func encodeWithCoder(aCoder: NSCoder) {
        aCoder.encodeObject(self.types,         forKey: kTypesKey)
        aCoder.encodeObject(self.stringValue,   forKey: kStringValueKey)
        aCoder.encodeObject(self.RTFData,       forKey: kRTFDataKey)
        aCoder.encodeObject(self.PDF,           forKey: kPDFKey)
        aCoder.encodeObject(self.fileNames,     forKey: kFileNamesKey)
        aCoder.encodeObject(self.URLs,          forKey: kURLsKey)
        aCoder.encodeObject(self.image,         forKey: kImageKey)
    }
    
    required init(coder aDecoder: NSCoder) {
        self.types          = aDecoder.decodeObjectForKey(kTypesKey)        as! [String]
        self.fileNames      = aDecoder.decodeObjectForKey(kFileNamesKey)    as! [String]
        self.URLs           = aDecoder.decodeObjectForKey(kURLsKey)         as! [String]
        self.stringValue    = aDecoder.decodeObjectForKey(kStringValueKey)  as! String
        self.RTFData        = aDecoder.decodeObjectForKey(kRTFDataKey)      as? NSData
        self.PDF            = aDecoder.decodeObjectForKey(kPDFKey)          as? NSData
        self.image          = aDecoder.decodeObjectForKey(kImageKey)        as? NSImage
        super.init()
    }
    
    // MARK: - Class Methods
    static func availableTypes() -> [String] {
        return [NSStringPboardType, NSRTFPboardType, NSRTFDPboardType, NSPDFPboardType, NSFilenamesPboardType, NSURLPboardType, NSTIFFPboardType]
    }
    
    static func availableTypesString() -> [String] {
        return ["String", "RTF", "RTFD", "PDF", "Filenames", "URL", "TIFF"]
    }
    
    static func availableTypesDictinary() -> [String: String] {
        var availableTypes = [String: String]()
        for i in 0...6 {
            let key = self.availableTypes()[i]
            let object = self.availableTypesString()[i]
            availableTypes[key] = object
        }
        return availableTypes
    }
    
}

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

    var types          = [NSPasteboard.PasteboardType]()
    var fileNames      = [String]()
    var URLs           = [String]()
    var stringValue    = ""
    var RTFData: Data?
    var PDF: Data?
    var image: NSImage?

    override var hash: Int {
        var hash = types.map { $0.rawValue }.joined(separator: "").hash
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
    var primaryType: NSPasteboard.PasteboardType? {
        return types.first
    }
    var isOnlyStringType: Bool {
        return types == [.string]
    }
    var thumbnailImage: NSImage? {
        let defaults = UserDefaults.standard
        let width = defaults.integer(forKey: Constants.UserDefaults.thumbnailWidth)
        let height = defaults.integer(forKey: Constants.UserDefaults.thumbnailHeight)

        if let image = image, fileNames.isEmpty {
            // Image only data
            return image.resizeImage(CGFloat(width), CGFloat(height))
        } else if let fileName = fileNames.first, let path = fileName.addingPercentEncoding(withAllowedCharacters: .urlQueryAllowed), let url = URL(string: path) {
            /**
             *  In the case of the local file correct data is not included in the image variable
             *  Judge the image from the path and create a thumbnail
             */
            switch url.pathExtension.lowercased() {
                case "jpg", "jpeg", "png", "bmp", "tiff":
                    return NSImage(contentsOfFile: fileName)?.resizeImage(CGFloat(width), CGFloat(height))
                default: break
            }
        }
        return nil
    }
    var colorCodeImage: NSImage? {
        guard let color = NSColor(hex: stringValue) else { return nil }
        return NSImage.create(with: color, size: NSSize(width: 20, height: 20))
    }

    static var availableTypes: [NSPasteboard.PasteboardType] {
        return [NSPasteboard.PasteboardType(rawValue: "NSStringPboardType"),
                NSPasteboard.PasteboardType(rawValue: "NSRTFPboardType"),
                NSPasteboard.PasteboardType(rawValue: "NSRTFDPboardType"),
                NSPasteboard.PasteboardType(rawValue: "NSPDFPboardType"),
                NSPasteboard.PasteboardType(rawValue: "NSFilenamesPboardType"),
                NSPasteboard.PasteboardType(rawValue: "NSURLPboardType"),
                NSPasteboard.PasteboardType(rawValue: "NSTIFFPboardType")]
    }
    static var availableTypesString: [String] {
        return ["String", "RTF", "RTFD", "PDF", "Filenames", "URL", "TIFF"]
    }
    static var availableTypesDictinary: [NSPasteboard.PasteboardType: String] {
        var availableTypes = [NSPasteboard.PasteboardType: String]()
        zip(CPYClipData.availableTypes, CPYClipData.availableTypesString).forEach { availableTypes[$0] = $1 }
        return availableTypes
    }

    // MARK: - Init
    init(pasteboard: NSPasteboard, types: [NSPasteboard.PasteboardType]) {
        super.init()
        self.types = types
        types.forEach { type in
            print(type)
            switch type {
            case NSPasteboard.PasteboardType(rawValue: "NSStringPboardType"):
                if let pbString = pasteboard.string(forType: NSPasteboard.PasteboardType(rawValue: "NSStringPboardType")) {
                    stringValue = pbString
                }
            case NSPasteboard.PasteboardType(rawValue: "NSRTFDPboardType"):
                RTFData = pasteboard.data(forType: NSPasteboard.PasteboardType(rawValue: "NSRTFDPboardType"))
            case NSPasteboard.PasteboardType(rawValue: "NSRTFPboardType") where RTFData == nil:
                RTFData = pasteboard.data(forType: NSPasteboard.PasteboardType(rawValue: "NSRTFPboardType"))
            case NSPasteboard.PasteboardType(rawValue: "NSPDFPboardType"):
                PDF = pasteboard.data(forType: NSPasteboard.PasteboardType(rawValue: "NSPDFPboardType"))
            case NSPasteboard.PasteboardType(rawValue: "NSFilenamesPboardType"):
                if let fileNames = pasteboard.propertyList(forType: NSPasteboard.PasteboardType(rawValue: "NSFilenamesPboardType")) as? [String] {
                    self.fileNames = fileNames
                }
            case NSPasteboard.PasteboardType(rawValue: "NSURLPboardType"):
                if let url = pasteboard.propertyList(forType: NSPasteboard.PasteboardType(rawValue: "NSURLPboardType")) as? [String] {
                    URLs = url
                }
            case NSPasteboard.PasteboardType(rawValue: "NSTIFFPboardType"):
                image = pasteboard.readObjects(forClasses: [NSImage.self], options: nil)?.first as? NSImage
            default: break
            }
        }
    }

    init(image: NSImage) {
        self.types = [NSPasteboard.PasteboardType(rawValue: "NSTIFFPboardType")]
        self.image = image
    }

    deinit {
        self.RTFData = nil
        self.PDF = nil
        self.image = nil
    }

    // MARK: - NSCoding
    @objc func encodeWithCoder(_ aCoder: NSCoder) {
        aCoder.encode(types.map { $0.rawValue }, forKey: kTypesKey)
        aCoder.encode(stringValue, forKey: kStringValueKey)
        aCoder.encode(RTFData, forKey: kRTFDataKey)
        aCoder.encode(PDF, forKey: kPDFKey)
        aCoder.encode(fileNames, forKey: kFileNamesKey)
        aCoder.encode(URLs, forKey: kURLsKey)
        aCoder.encode(image, forKey: kImageKey)
    }

    @objc required init(coder aDecoder: NSCoder) {
        types          = (aDecoder.decodeObject(forKey: kTypesKey)       as? [String] ?? [String]()).flatMap { NSPasteboard.PasteboardType(rawValue: $0) }
        fileNames      = aDecoder.decodeObject(forKey: kFileNamesKey)    as? [String] ?? [String]()
        URLs           = aDecoder.decodeObject(forKey: kURLsKey)         as? [String] ?? [String]()
        stringValue    = aDecoder.decodeObject(forKey: kStringValueKey)  as? String ?? ""
        RTFData        = aDecoder.decodeObject(forKey: kRTFDataKey)      as? Data
        PDF            = aDecoder.decodeObject(forKey: kPDFKey)          as? Data
        image          = aDecoder.decodeObject(forKey: kImageKey)        as? NSImage
        super.init()
    }
}

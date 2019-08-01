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

    override var hash: Int {
        var hash = types.map { $0.rawValue }.joined().hash
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
        return types == [.deprecatedString]
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
        guard let color = NSColor(hexString: stringValue) else { return nil }
        return NSImage.create(with: color, size: NSSize(width: 20, height: 20))
    }

    // MARK: - Init
    init(pasteboard: NSPasteboard, types: [NSPasteboard.PasteboardType]) {
        super.init()
        self.types = types
        readString(with: pasteboard, types: types)
        readRTDData(with: pasteboard, types: types)
        readPDF(with: pasteboard, types: types)
        readFilenems(with: pasteboard, types: types)
        readURLs(with: pasteboard, types: types)
        readImage(with: pasteboard, types: types)
    }

    init(image: NSImage) {
        self.types = [.deprecatedTIFF]
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

// MARK: - Values
private extension CPYClipData {
    func readString(with pasteboard: NSPasteboard, types: [NSPasteboard.PasteboardType]) {
        if types.contains(.string) {
            stringValue = pasteboard.string(forType: .string) ?? ""
        } else if types.contains(.deprecatedString) && stringValue.isEmpty {
            stringValue = pasteboard.string(forType: .deprecatedString) ?? ""
        }
    }

    func readRTDData(with pasteboard: NSPasteboard, types: [NSPasteboard.PasteboardType]) {
        if types.contains(.rtfd) {
            RTFData = pasteboard.data(forType: .rtfd)
        } else if types.contains(.deprecatedRTFD) && RTFData == nil {
            RTFData = pasteboard.data(forType: .deprecatedRTFD)
        } else if types.contains(.rtf) && RTFData == nil {
            RTFData = pasteboard.data(forType: .rtf)
        } else if types.contains(.deprecatedRTF) && RTFData == nil {
            RTFData = pasteboard.data(forType: .deprecatedRTF)
        }
    }

    func readPDF(with pasteboard: NSPasteboard, types: [NSPasteboard.PasteboardType]) {
        if types.contains(.pdf) {
            PDF = pasteboard.data(forType: .pdf)
        } else if types.contains(.deprecatedPDF) && PDF == nil {
            PDF = pasteboard.data(forType: .deprecatedPDF)
        }
    }

    func readFilenems(with pasteboard: NSPasteboard, types: [NSPasteboard.PasteboardType]) {
        if #available(OSX 10.13, *) {
            if types.contains(.fileURL) {
                fileNames = (pasteboard.propertyList(forType: .fileURL) as? [String]) ?? []
            } else if types.contains(.deprecatedFilenames) && fileNames.isEmpty {
                fileNames = (pasteboard.propertyList(forType: .deprecatedFilenames) as? [String]) ?? []
            }
        } else {
            fileNames = (pasteboard.propertyList(forType: .deprecatedFilenames) as? [String]) ?? []
        }
    }

    func readURLs(with pasteboard: NSPasteboard, types: [NSPasteboard.PasteboardType]) {
        if #available(OSX 10.13, *) {
            if types.contains(.URL) {
                URLs = (pasteboard.propertyList(forType: .URL) as? [String]) ?? []
            } else if types.contains(.deprecatedURL) && URLs.isEmpty {
                URLs = (pasteboard.propertyList(forType: .deprecatedURL) as? [String]) ?? []
            }
        } else {
            URLs = (pasteboard.propertyList(forType: .deprecatedURL) as? [String]) ?? []
        }
    }

    func readImage(with pasteboard: NSPasteboard, types: [NSPasteboard.PasteboardType]) {
        if types.contains(.tiff) || types.contains(.deprecatedTIFF) {
            image = pasteboard.readObjects(forClasses: [NSImage.self], options: nil)?.first as? NSImage
        }
    }
}

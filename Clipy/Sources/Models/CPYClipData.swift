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
import CryptoSwift

final class CPYClipData: NSObject {

    // MARK: - Properties
    fileprivate static let kTypesKey       = "types"
    fileprivate static let kStringValueKey = "stringValue"
    fileprivate static let kRTFDataKey     = "RTFData"
    fileprivate static let kPDFKey         = "PDF"
    fileprivate static let kFileNamesKey   = "filenames"
    fileprivate static let kURLsKey        = "URL"
    fileprivate static let kImageKey       = "image"
    fileprivate static let kHash           = "hash"
    fileprivate var lazyHash: Int?

    var types          = [NSPasteboard.PasteboardType]()
    var fileNames      = [String]()
    var URLs           = [String]()
    var stringValue    = ""
    var RTFData: Data?
    var PDF: Data?
    var image: NSImage?

    override var hash: Int {
        if lazyHash != nil {
            return lazyHash!
        }
        var digest = SHA2(variant: .sha256)

        types.forEach { _ = try! digest.update(withBytes: $0.rawValue.bytes) }
        // plain data
        if stringValue.isNotEmpty {
            _ = try! digest.update(withBytes: stringValue.bytes)
        }
        if URLs.isNotEmpty {
            URLs.forEach { _ = try! digest.update(withBytes: $0.bytes) }
        }
        if fileNames.isNotEmpty {
            fileNames.forEach { _ = try! digest.update(withBytes: $0.bytes) }
        }
        // unstructured data
        let type = primaryType ?? AvailableType.string.primaryPbType
        // try hash image that cover some pdf
        if let imageHash = image?.tiffRepresentation {
            //hash ^= imageHash.hashValue
            _ = try! digest.update(withBytes: imageHash.bytes)
        } else if let imageRep = image?.representations.first {
            //hash ^= imageRep.hashValue
            _ = try! digest.update(withBytes: imageRep.archive().bytes)
        } else if (type.isRTF || type.isRTFD) && RTFData != nil {
            // RTF data should be parsed the get the hash
            if let attr = NSAttributedString(rtfd: RTFData!, documentAttributes: nil) ?? NSAttributedString(rtf: RTFData!, documentAttributes: nil) {
                //hash ^= "\(attr)".removePtr().hashValue
                _ = try! digest.update(withBytes: "\(attr)".removePtr().bytes)
            } else {
                //hash ^= RTFData!.hashValue
                _ = try! digest.update(withBytes: RTFData!.bytes)
            }
        } else if type.isPDF && PDF != nil {
            //hash ^= PDF!.count
            _ = try! digest.update(withBytes: PDF!.bytes)
        }
        let crc32value = try! digest.finish().crc32()
        lazyHash = Int(crc32value)
        return lazyHash!
    }
    var primaryType: NSPasteboard.PasteboardType? {
        return types.first
    }
    var isOnlyStringType: Bool {
        return types.allSatisfy { $0.isString }
    }
    var thumbnailImage: NSImage? {
        let defaults = UserDefaults.standard
        let width = defaults.integer(forKey: Constants.UserDefaults.thumbnailWidth)
        let height = defaults.integer(forKey: Constants.UserDefaults.thumbnailHeight)

        if fileNames.count == 1, let path = fileNames[0].addingPercentEncoding(withAllowedCharacters: .urlQueryAllowed), let url = URL(string: path) {
            // file itself is a image
            let ext = url.pathExtension.lowercased() as CFString
            if let uti = UTTypeCreatePreferredIdentifierForTag(kUTTagClassFilenameExtension, ext, nil),
                UTTypeConformsTo(uti.takeRetainedValue(), kUTTypeImage) {
                return NSImage(contentsOfFile: path)?.resizeImage(CGFloat(width), CGFloat(height))
            }
        }
        if fileNames.count > 1 {
            // get file icon
            if let img = NSWorkspace.shared.icon(forFiles: fileNames)?
                    .resizeImage(CGFloat(width), CGFloat(height)) {
                return img
            }
        }

        return image?.resizeImage(CGFloat(width), CGFloat(height))
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
        super.init()
        self.types = AvailableType.tiff.targetPbTypes
        self.image = image
    }

    deinit {
        self.RTFData = nil
        self.PDF = nil
        self.image = nil
    }

    // MARK: - NSCoding
    @objc func encodeWithCoder(_ aCoder: NSCoder) {
        aCoder.encode(types.map { $0.rawValue }, forKey: CPYClipData.kTypesKey)
        aCoder.encode(stringValue, forKey: CPYClipData.kStringValueKey)
        aCoder.encode(RTFData, forKey: CPYClipData.kRTFDataKey)
        aCoder.encode(PDF, forKey: CPYClipData.kPDFKey)
        aCoder.encode(fileNames, forKey: CPYClipData.kFileNamesKey)
        aCoder.encode(URLs, forKey: CPYClipData.kURLsKey)
        aCoder.encode(image, forKey: CPYClipData.kImageKey)
        aCoder.encode(hash, forKey: CPYClipData.kHash)
    }

    @objc required init(coder aDecoder: NSCoder) {
        types = (aDecoder.decodeObject(forKey: CPYClipData.kTypesKey) as? [String])?
            .compactMap { NSPasteboard.PasteboardType(rawValue: $0) } ?? []
        fileNames = aDecoder.decodeObject(forKey: CPYClipData.kFileNamesKey) as? [String] ?? [String]()
        URLs = aDecoder.decodeObject(forKey: CPYClipData.kURLsKey) as? [String] ?? [String]()
        stringValue = aDecoder.decodeObject(forKey: CPYClipData.kStringValueKey) as? String ?? ""
        RTFData = aDecoder.decodeObject(forKey: CPYClipData.kRTFDataKey) as? Data
        PDF = aDecoder.decodeObject(forKey: CPYClipData.kPDFKey) as? Data
        image = aDecoder.decodeObject(forKey: CPYClipData.kImageKey) as? NSImage
        lazyHash = aDecoder.decodeObject(forKey: CPYClipData.kHash) as? Int
        super.init()
    }

}

// MARK: - Values
private extension CPYClipData {
    func readString(with pasteboard: NSPasteboard, types: [NSPasteboard.PasteboardType]) {
        stringValue = types.filter({ $0.isString })
            .compactMap({ pasteboard.string(forType: $0) })
            .first(where: { $0.isNotEmpty }) ?? ""
    }

    func readRTDData(with pasteboard: NSPasteboard, types: [NSPasteboard.PasteboardType]) {
        RTFData = types.filter({ $0.isRTF || $0.isRTFD })
            .compactMap({ pasteboard.data(forType: $0) })
            .first
    }

    func readPDF(with pasteboard: NSPasteboard, types: [NSPasteboard.PasteboardType]) {
        PDF = types.filter({ $0.isPDF })
            .compactMap({ pasteboard.data(forType: $0) })
            .first
    }

    func readFilenems(with pasteboard: NSPasteboard, types: [NSPasteboard.PasteboardType]) {
        fileNames = types.filter({ $0.isFilenames })
            .compactMap({ (pasteboard.propertyList(forType: $0) as? [String]) })
            .first(where: { $0.isNotEmpty }) ?? []
    }

    func readURLs(with pasteboard: NSPasteboard, types: [NSPasteboard.PasteboardType]) {
        URLs = types.filter({ $0.isURL })
            .compactMap({ (pasteboard.propertyList(forType: $0) as? [String]) })
            .first(where: { $0.isNotEmpty }) ?? []
    }

    func readImage(with pasteboard: NSPasteboard, types: [NSPasteboard.PasteboardType]) {
        guard types.contains(where: { $0.isTIFF }) else {
            return
        }
        image = pasteboard.readObjects(forClasses: [NSImage.self], options: nil)?.first as? NSImage
    }
}

private extension String {

    static let ptrRegex = try! NSRegularExpression(pattern: "0x[a-f0-9]+", options: [.caseInsensitive])

    func removePtr() -> String {
        return String.ptrRegex.stringByReplacingMatches(in: self, options: [], range: NSRange(location: 0, length: self.count), withTemplate: "")
    }
}

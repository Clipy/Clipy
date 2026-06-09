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

    // MARK: - Initialize
    override init() {
        super.init()
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

extension CPYClipData {
    func toPasteboardContent() -> PasteboardContent? {
        let assets = types.compactMap { type -> [PasteboardContent.Asset]? in
            switch type {
            case .deprecatedString where !stringValue.isEmpty:
                guard let data = stringValue.data(using: .utf8) else { return nil }
                return [.init(type: .string, data: data)]

            case .deprecatedRTFD:
                guard let data = RTFData else { return nil }
                return [.init(type: .rtfd, data: data)]
            case .deprecatedRTF:
                guard let data = RTFData else { return nil }
                return [.init(type: .rtf, data: data)]

            case .deprecatedPDF:
                guard let data = PDF else { return nil }
                return [.init(type: .pdf, data: data)]

            case .deprecatedFilenames where !fileNames.isEmpty:
                guard let data = propertyListData(from: fileNames) else { return nil }
                return [.init(type: .deprecatedFilenames, data: data)]

            case .deprecatedURL where !URLs.isEmpty:
                return URLs
                    .compactMap { URL(string: $0) }
                    .compactMap { $0.dataRepresentation }
                    .map { .init(type: .URL, data: $0) }

            case .deprecatedTIFF:
                guard let data = image?.tiffRepresentation else { return nil }
                return [.init(type: .tiff, data: data)]

            default:
                return nil
            }
        }
        .flatMap { $0 }
        return PasteboardContent(assets: assets)
    }

    private func propertyListData(from propertyList: [String]) -> Data? {
        try? PropertyListSerialization.data(
            fromPropertyList: propertyList,
            format: .binary,
            options: 0
        )
    }
}

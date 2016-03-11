//
//  PasteboardManager.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/03/11.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation
import Crashlytics

final class PasteboardManager {
    // MARK: - Properties
    static let sharedManager = PasteboardManager()
    private let pasteboard = NSPasteboard.generalPasteboard()
    private let lock = NSRecursiveLock(name: "com.clipy-app.Clipy.Pastable")
}

// MARK: - Copy
extension PasteboardManager {
    func copyStringToPasteboard(aString: String) {
        lock.lock()
        pasteboard.declareTypes([NSStringPboardType], owner: self)
        pasteboard.setString(aString, forType: NSStringPboardType)
        lock.unlock()
    }
    
    func copyClipToPasteboard(clip: CPYClip) {
        lock.lock()
        if let data = NSKeyedUnarchiver.unarchiveObjectWithFile(clip.dataPath) as? CPYClipData {
            let types = data.types
            pasteboard.declareTypes(types, owner: self)
            
            types.forEach { type in
                switch type {
                case NSStringPboardType:
                    let pbString = data.stringValue
                    pasteboard.setString(pbString, forType: NSStringPboardType)
                case NSRTFDPboardType:
                    if let rtfData = data.RTFData {
                        pasteboard.setData(rtfData, forType: NSRTFDPboardType)
                    }
                case NSRTFPboardType:
                    if let rtfData = data.RTFData {
                        pasteboard.setData(rtfData, forType: NSRTFPboardType)
                    }
                case NSPDFPboardType:
                    if let pdfData = data.PDF, pdfRep = NSPDFImageRep(data: pdfData) {
                        pasteboard.setData(pdfRep.PDFRepresentation, forType: NSPDFPboardType)
                    }
                case NSFilenamesPboardType:
                    let fileNames = data.fileNames
                    pasteboard.setPropertyList(fileNames, forType: NSFilenamesPboardType)
                case NSURLPboardType:
                    let url = data.URLs
                    pasteboard.setPropertyList(url, forType: NSURLPboardType)
                case NSTIFFPboardType:
                    if let image = data.image {
                        pasteboard.setData(image.TIFFRepresentation!, forType: NSTIFFPboardType)
                    }
                default:
                    Answers.logCustomEventWithName("No suppert paste type \(type)", customAttributes: nil)
                }
            }
        }
        lock.unlock()
    }
}

// MARK: - Paste

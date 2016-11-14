//
//  PasteboardManager.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/03/11.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation
import Cocoa
import Crashlytics

final class PasteboardManager {
    // MARK: - Properties
    static let sharedManager = PasteboardManager()
    fileprivate let pasteboard = NSPasteboard.general()
    fileprivate let lock = NSRecursiveLock(name: "com.clipy-app.Clipy.Pastable")
    fileprivate let defaults = UserDefaults.standard
    fileprivate var isPastePlainText: Bool {
        guard let flags = NSApp.currentEvent?.modifierFlags else { return false }
        if !defaults.bool(forKey: Constants.Beta.pastePlainText) { return false }

        let modifierSetting = defaults.integer(forKey: Constants.Beta.pastePlainTextModifier)
        if modifierSetting == 0 && flags.contains(.command) {
            return true
        } else if modifierSetting == 1 && flags.contains(.shift) {
            return true
        } else if modifierSetting == 2 && flags.contains(.control) {
            return true
        } else if modifierSetting == 3 && flags.contains(.option) {
            return true
        }
        return false
    }
}

// MARK: - Copy
extension PasteboardManager {
    func copyStringToPasteboard(_ aString: String) {
        lock.lock()
        pasteboard.declareTypes([NSStringPboardType], owner: self)
        pasteboard.setString(aString, forType: NSStringPboardType)
        lock.unlock()
    }

    func copyClipToPasteboard(_ clip: CPYClip) {
        lock.lock()
        if let data = NSKeyedUnarchiver.unarchiveObject(withFile: clip.dataPath) as? CPYClipData {
            let types = data.types
            let isPastePlainText = self.isPastePlainText
            pasteboard.declareTypes(types, owner: self)
            types.forEach { type in
                switch type {
                case NSStringPboardType:
                    let pbString = data.stringValue
                    pasteboard.setString(pbString, forType: NSStringPboardType)
                case NSRTFDPboardType where !isPastePlainText:
                    if let rtfData = data.RTFData {
                        pasteboard.setData(rtfData, forType: NSRTFDPboardType)
                    }
                case NSRTFPboardType where !isPastePlainText:
                    if let rtfData = data.RTFData {
                        pasteboard.setData(rtfData, forType: NSRTFPboardType)
                    }
                case NSPDFPboardType:
                    if let pdfData = data.PDF, let pdfRep = NSPDFImageRep(data: pdfData) {
                        pasteboard.setData(pdfRep.pdfRepresentation, forType: NSPDFPboardType)
                    }
                case NSFilenamesPboardType:
                    let fileNames = data.fileNames
                    pasteboard.setPropertyList(fileNames, forType: NSFilenamesPboardType)
                case NSURLPboardType:
                    let url = data.URLs
                    pasteboard.setPropertyList(url, forType: NSURLPboardType)
                case NSTIFFPboardType:
                    if let image = data.image, let imageData = image.tiffRepresentation {
                        pasteboard.setData(imageData, forType: NSTIFFPboardType)
                    }
                default: break
                }
            }
        }
        lock.unlock()
    }
}

// MARK: - Paste
extension PasteboardManager {
    static func paste() {
        if !UserDefaults.standard.bool(forKey: Constants.UserDefaults.inputPasteCommand) { return }

        let source = CGEventSource(stateID: .combinedSessionState)
        // Press Command + V
        let keyVDown = CGEvent(keyboardEventSource: source, virtualKey: CGKeyCode(9), keyDown: true)
        keyVDown?.flags = .maskCommand
        // Release Command + V
        let keyVUp = CGEvent(keyboardEventSource: source, virtualKey: CGKeyCode(9), keyDown: false)
        keyVUp?.flags = .maskCommand
        // Post Paste Command
        keyVDown?.post(tap: .cgAnnotatedSessionEventTap)
        keyVUp?.post(tap: .cgAnnotatedSessionEventTap)
    }
}

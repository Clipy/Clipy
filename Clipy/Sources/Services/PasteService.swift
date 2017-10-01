//
//  PasteService.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/11/23.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation
import Cocoa

final class PasteService {

    // MARK: - Properties
    fileprivate let lock = NSRecursiveLock(name: "com.clipy-app.Clipy.Pastable")
    fileprivate var isPastePlainText: Bool {
        guard AppEnvironment.current.defaults.bool(forKey: Constants.Beta.pastePlainText) else { return false }

        let modifierSetting = AppEnvironment.current.defaults.integer(forKey: Constants.Beta.pastePlainTextModifier)
        return isPressedModifier(modifierSetting)
    }
    fileprivate var isDeleteHistory: Bool {
        guard AppEnvironment.current.defaults.bool(forKey: Constants.Beta.deleteHistory) else { return false }

        let modifierSetting = AppEnvironment.current.defaults.integer(forKey: Constants.Beta.deleteHistoryModifier)
        return isPressedModifier(modifierSetting)
    }
    fileprivate var isPasteAndDeleteHistory: Bool {
        guard AppEnvironment.current.defaults.bool(forKey: Constants.Beta.pasteAndDeleteHistory) else { return false }

        let modifierSetting = AppEnvironment.current.defaults.integer(forKey: Constants.Beta.pasteAndDeleteHistoryModifier)
        return isPressedModifier(modifierSetting)
    }

    // MARK: - Modifiers
    private func isPressedModifier(_ flag: Int) -> Bool {
        let flags = NSEvent.modifierFlags()
        if flag == 0 && flags.contains(.command) {
            return true
        } else if flag == 1 && flags.contains(.shift) {
            return true
        } else if flag == 2 && flags.contains(.control) {
            return true
        } else if flag == 3 && flags.contains(.option) {
            return true
        }
        return false
    }
}

// MARK: - Copy
extension PasteService {
    func paste(with clip: CPYClip) {
        guard !clip.isInvalidated else { return }
        guard let data = NSKeyedUnarchiver.unarchiveObject(withFile: clip.dataPath) as? CPYClipData else { return }

        // Handling modifier actions
        let isPastePlainText = self.isPastePlainText
        let isPasteAndDeleteHistory = self.isPasteAndDeleteHistory
        let isDeleteHistory = self.isDeleteHistory
        guard isPastePlainText || isPasteAndDeleteHistory || isDeleteHistory else {
            copyToPasteboard(with: clip)
            paste()
            return
        }

        // Increment change count for don't copy paste item
        if isPasteAndDeleteHistory {
            AppEnvironment.current.clipService.incrementChangeCount()
        }
        // Paste hisotry
        if isPastePlainText {
            copyToPasteboard(with: data.stringValue)
            paste()
        } else if isPasteAndDeleteHistory {
            copyToPasteboard(with: clip)
            paste()
        }
        // Delete clip
        if isDeleteHistory || isPasteAndDeleteHistory {
            AppEnvironment.current.clipService.delete(with: clip)
        }
    }

    func copyToPasteboard(with string: String) {
        lock.lock(); defer { lock.unlock() }

        let pasteboard = NSPasteboard.general()
        pasteboard.declareTypes([NSStringPboardType], owner: nil)
        pasteboard.setString(string, forType: NSStringPboardType)
    }

    func copyToPasteboard(with clip: CPYClip) {
        lock.lock(); defer { lock.unlock() }

        guard let data = NSKeyedUnarchiver.unarchiveObject(withFile: clip.dataPath) as? CPYClipData else { return }

        if isPastePlainText {
            copyToPasteboard(with: data.stringValue)
            return
        }

        let pasteboard = NSPasteboard.general()
        let types = data.types
        pasteboard.declareTypes(types, owner: nil)
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
}

// MARK: - Paste
extension PasteService {
    func paste() {
        if !AppEnvironment.current.defaults.bool(forKey: Constants.UserDefaults.inputPasteCommand) { return }

        DispatchQueue.main.async {
            let source = CGEventSource(stateID: .combinedSessionState)
            // Disable local keyboard events while pasting
            source?.setLocalEventsFilterDuringSuppressionState([.permitLocalMouseEvents, .permitSystemDefinedEvents], state: .eventSuppressionStateSuppressionInterval)
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
}

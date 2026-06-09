// 
//  AccessibilityService.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
// 
//  Created by Econa77 on 2018/10/03.
// 
//  Copyright © 2015-2018 Clipy Project.
//

import Foundation
import Cocoa

final class AccessibilityService {}

// MARK: - Permission
extension AccessibilityService {
    // Accessibility permission is required for simulating paste (Cmd+V) via CGEvent from macOS 10.14 Mojave.
    @discardableResult
    func isAccessibilityEnabled(isPrompt: Bool) -> Bool {
        guard #available(macOS 10.14, *) else { return true }

        let checkOptionPromptKey = kAXTrustedCheckOptionPrompt.takeUnretainedValue() as String
        let opts = [checkOptionPromptKey: false] as CFDictionary
        if AXIsProcessTrustedWithOptions(opts) {
            return true
        }
        // AXIsProcessTrustedWithOptions can return false for unsigned/ad-hoc signed
        // builds even when accessibility is granted. Verify with a practical test
        // before showing any prompt.
        var value: CFTypeRef?
        let result = AXUIElementCopyAttributeValue(
            AXUIElementCreateSystemWide(),
            kAXFocusedApplicationAttribute as CFString,
            &value
        )
        // .success means an app has focus and we got it — trust is granted.
        // .noValue means no app has focus (e.g. menu just closed) but the API
        // accepted the call — trust is still granted.
        // .apiDisabled or .cannotComplete means no trust.
        if result == .success || result == .noValue {
            return true
        }
        if isPrompt {
            let promptOpts = [checkOptionPromptKey: true] as CFDictionary
            AXIsProcessTrustedWithOptions(promptOpts)
        }
        return false
    }

    func showAccessibilityAuthenticationAlert() {
        let alert = NSAlert()
        alert.messageText = String(localized: "Please allow Accessibility")
        alert.informativeText = String(localized: "To do this action please allow Accessibility in Security Privacy preferences located in System Preferences")
        alert.addButton(withTitle: String(localized: "Open System Preferences"))
        NSApp.activate(ignoringOtherApps: true)

        if alert.runModal() == NSApplication.ModalResponse.alertFirstButtonReturn {
            guard !openAccessibilitySettingWindow() else { return }
            isAccessibilityEnabled(isPrompt: true)
        }
    }

    func openAccessibilitySettingWindow() -> Bool {
        guard let url = URL(string: "x-apple.systempreferences:com.apple.preference.security?Privacy_Accessibility") else { return false }
        return NSWorkspace.shared.open(url)
    }
}

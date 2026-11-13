//
//  AlternativeMenuTypingSelection.swift
//  AlternativeMenu
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import CoreServices

/// Finds the menu item to highlight as the user types its title.
///
/// Consecutive keystrokes accumulate in CoreServices' input buffer. Repeated characters
/// extend the typed text, and the system controls when a pause starts a new sequence
/// and how Delete/Backspace affect the buffer. `reset()` explicitly starts a new sequence.
///
/// Matching is case-insensitive and locale-aware: an exact title takes priority,
/// followed by the first prefix match in menu order, then the system's closest match.
/// Candidates are enabled actions and submenu parents in the supplied items, including
/// empty submenus. Section headers, separators, and disabled items are excluded;
/// submenu contents are not searched recursively.
///
/// Accepts printable key-down events, including Shift, and Delete/Backspace. Events
/// with Command, Option, or Control, and other navigation/control keys are ignored.
/// `AlternativeMenuSession` routes events here only when a header control is not
/// focused and IME composition is inactive, after handling menu commands and shortcuts.
/// The session applies the returned index to highlight and reveal the matching row.
@MainActor
final class AlternativeMenuTypingSelection {
    private var selector: UCTypeSelectRef?

    init() {
        let options = UCCollateOptions(
            kUCCollateCaseInsensitiveMask | kUCCollatePunctuationSignificantMask | kUCCollateDigitsOverrideMask
                | kUCCollateDigitsAsNumberMask
        )
        UCTypeSelectCreateSelector(nil, 0, options, &selector)
    }

    isolated deinit {
        if selector != nil {
            UCTypeSelectReleaseSelector(&selector)
        }
    }

    /// Clears the current typing sequence.
    func reset() {
        if let selector {
            UCTypeSelectFlushSelectorData(selector)
        }
    }

    /// Processes a typing event and returns a selection candidate.
    ///
    /// - Returns: `handled` indicates whether the event was consumed. A `nil` index
    ///   leaves the current selection unchanged, even when the event was handled.
    func select(_ event: NSEvent, in items: [AlternativeMenuItem]) -> (handled: Bool, index: Int?) {
        guard let selector, event.type == .keyDown,
            event.modifierFlags.isDisjoint(with: [.command, .option, .control]),
            let key = event.characters, let first = key.unicodeScalars.first
        else {
            return (false, nil)
        }
        let deletes = event.specialKey == .delete || event.specialKey == .backspace
        guard deletes || (event.specialKey == nil && !CharacterSet.controlCharacters.contains(first)) else {
            return (false, nil)
        }

        let timestamp = event.timestamp == 0 ? ProcessInfo.processInfo.systemUptime : event.timestamp
        var updatesSelection = DarwinBoolean(false)
        guard UCTypeSelectAddKeyToSelector(selector, key as CFString, timestamp, &updatesSelection) == noErr else {
            reset()
            return (false, nil)
        }
        guard updatesSelection.boolValue else {
            return (true, nil)
        }
        return (true, closestItem(in: items, using: selector))
    }

    private func closestItem(in items: [AlternativeMenuItem], using selector: UCTypeSelectRef) -> Int? {
        guard !items.isEmpty else {
            return nil
        }
        var match: UInt32 = 0
        let status = items.withUnsafeBufferPointer { items in
            UCTypeSelectFindItem(
                selector,
                UInt32(items.count),
                UnsafeMutableRawPointer(mutating: items.baseAddress),
                nil,
                { index, rows, _, title, options in
                    guard let rows else {
                        return false
                    }
                    let item = rows.assumingMemoryBound(to: AlternativeMenuItem.self)[Int(index)]
                    title?.pointee = item.isEnabled ? Unmanaged.passRetained(item.title as CFString) : nil
                    // NSMenu uses undocumented bit 0x4 to prefer the first prefix match
                    // in menu order, while still prioritizing exact matches.
                    options?.pointee = UCTypeSelectOptions(kUCTSOptionsReleaseStringMask | 0x4)
                    return true
                },
                &match
            )
        }
        let index = Int(match)
        guard status == noErr, items.indices.contains(index), items[index].isEnabled else {
            return nil
        }
        return index
    }
}

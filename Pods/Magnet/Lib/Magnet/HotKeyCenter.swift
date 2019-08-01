//
//  HotKeyCenter.swift
//  Magnet
//
//  Created by 古林俊佑 on 2016/03/09.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa
import Carbon

public final class HotKeyCenter {

    // MARK: - Properties
    public static let shared = HotKeyCenter()
    fileprivate var hotKeys = [String: HotKey]()
    fileprivate var hotKeyMap = [NSNumber: HotKey]()
    fileprivate var hotKeyCount: UInt32 = 0

    fileprivate var tappedModifierKey = NSEvent.ModifierFlags(rawValue: 0)
    fileprivate var multiModifiers = false

    // MARK: - Initialize
    init() {
        installEventHandler()
        observeApplicationTerminate()
    }

}

// MARK: - Register & Unregister
public extension HotKeyCenter {
    func register(with hotKey: HotKey) -> Bool {
        guard !hotKeys.keys.contains(hotKey.identifier) else { return false }
        guard !hotKeys.values.contains(hotKey) else { return false }

        if !hotKey.keyCombo.doubledModifiers {
            // Normal HotKey
            let hotKeyId = EventHotKeyID(signature: UTGetOSTypeFromString("Magnet" as CFString), id: hotKeyCount)
            var carbonHotKey: EventHotKeyRef? = nil
            let error = RegisterEventHotKey(UInt32(hotKey.keyCombo.keyCode),
                                            UInt32(hotKey.keyCombo.modifiers),
                                            hotKeyId,
                                            GetEventDispatcherTarget(),
                                            0,
                                            &carbonHotKey)
            if error != 0 { return false }

            hotKey.hotKeyId = hotKeyId.id
            hotKey.hotKeyRef = carbonHotKey
        }

        let kId = NSNumber(value: hotKeyCount as UInt32)
        hotKeyMap[kId] = hotKey
        hotKeyCount += 1

        hotKeys[hotKey.identifier] = hotKey

        return true
    }
    
    func unregister(with hotKey: HotKey) {
        guard hotKeys.values.contains(hotKey) else { return }

        if !hotKey.keyCombo.doubledModifiers {
            // Notmal HotKey
            guard let carbonHotKey = hotKey.hotKeyRef else { return }
            UnregisterEventHotKey(carbonHotKey)
        }

        hotKeys.removeValue(forKey: hotKey.identifier)

        hotKey.hotKeyId = nil
        hotKey.hotKeyRef = nil

        hotKeyMap
            .filter { $1 == hotKey }
            .map { $0.0 }
            .forEach { hotKeyMap.removeValue(forKey: $0) }
    }

    func unregisterHotKey(with identifier: String) {
        guard let hotKey = hotKeys[identifier] else { return }
        unregister(with: hotKey)
    }

    func unregisterAll() {
        hotKeys.forEach { unregister(with: $1) }
    }
}

// MARK: - Terminate
extension HotKeyCenter {
    private func observeApplicationTerminate() {
        NotificationCenter.default.addObserver(self,
                                               selector: #selector(HotKeyCenter.applicationWillTerminate),
                                               name: NSApplication.willTerminateNotification,
                                               object: nil)
    }

    @objc func applicationWillTerminate() {
        unregisterAll()
    }
}

// MARK: - HotKey Events
private extension HotKeyCenter {
    func installEventHandler() {
        // Press HotKey Event
        var pressedEventType = EventTypeSpec()
        pressedEventType.eventClass = OSType(kEventClassKeyboard)
        pressedEventType.eventKind = OSType(kEventHotKeyPressed)
        InstallEventHandler(GetEventDispatcherTarget(), { (_, inEvent, _) -> OSStatus in
            return HotKeyCenter.shared.sendCarbonEvent(inEvent!)
        }, 1, &pressedEventType, nil, nil)

        // Press Modifiers Event
        let mask = CGEventMask((1 << CGEventType.flagsChanged.rawValue))
        let event = CGEvent.tapCreate(tap: .cghidEventTap,
                                     place: .headInsertEventTap,
                                     options: .listenOnly,
                                     eventsOfInterest: mask,
                                     callback: { (_, _, event, _) in return HotKeyCenter.shared.sendModifiersEvent(event) },
                                     userInfo: nil)
        if event == nil { return }
        let source = CFMachPortCreateRunLoopSource(kCFAllocatorDefault, event!, 0)
        CFRunLoopAddSource(CFRunLoopGetCurrent(), source, CFRunLoopMode.commonModes)
        CGEvent.tapEnable(tap: event!, enable: true)
    }

    func sendCarbonEvent(_ event: EventRef) -> OSStatus {
        assert(Int(GetEventClass(event)) == kEventClassKeyboard, "Unknown event class")

        var hotKeyId = EventHotKeyID()
        let error = GetEventParameter(event,
                                      EventParamName(kEventParamDirectObject),
                                      EventParamName(typeEventHotKeyID),
                                      nil,
                                      MemoryLayout<EventHotKeyID>.size,
                                      nil,
                                      &hotKeyId)

        if error != 0 { return error }

        assert(hotKeyId.signature == UTGetOSTypeFromString("Magnet" as CFString), "Invalid hot key id")

        let kId = NSNumber(value: hotKeyId.id as UInt32)
        let hotKey = hotKeyMap[kId]

        switch GetEventKind(event) {
        case EventParamName(kEventHotKeyPressed):
            hotKeyDown(hotKey)
        default:
            assert(false, "Unknown event kind")
        }

        return noErr
    }

    func hotKeyDown(_ hotKey: HotKey?) {
        guard let hotKey = hotKey else { return }
        hotKey.invoke()
    }
}

// MARK: - Double Tap Modifier Event
private extension HotKeyCenter {
    func sendModifiersEvent(_ event: CGEvent) -> Unmanaged<CGEvent>? {
        let flags = event.flags

        let commandTapped = flags.contains(.maskCommand)
        let shiftTapped = flags.contains(.maskShift)
        let controlTapped = flags.contains(.maskControl)
        let altTapped = flags.contains(.maskAlternate)

        // Only one modifier key
        let totalHash = commandTapped.intValue + altTapped.intValue + shiftTapped.intValue + controlTapped.intValue
        if totalHash == 0 { return Unmanaged.passUnretained(event) }
        if totalHash > 1 {
            multiModifiers = true
            return Unmanaged.passUnretained(event)
        }
        if multiModifiers {
            multiModifiers = false
            return Unmanaged.passUnretained(event)
        }

        if (tappedModifierKey.contains(.command) && commandTapped) ||
            (tappedModifierKey.contains(.shift) && shiftTapped)    ||
            (tappedModifierKey.contains(.control) && controlTapped) ||
            (tappedModifierKey.contains(.option) && altTapped) {
            doubleTapped(with: KeyTransformer.carbonFlags(from: tappedModifierKey))
            tappedModifierKey = NSEvent.ModifierFlags(rawValue: 0)
        } else {
            if commandTapped {
                tappedModifierKey = .command
            } else if shiftTapped {
                tappedModifierKey = .shift
            } else if controlTapped {
                tappedModifierKey = .control
            } else if altTapped {
                tappedModifierKey = .option
            } else {
                tappedModifierKey = NSEvent.ModifierFlags(rawValue: 0)
            }
        }

        // Clean Flag
        let delay = 0.3 * Double(NSEC_PER_SEC)
        let time  = DispatchTime.now() + Double(Int64(delay)) / Double(NSEC_PER_SEC)
        DispatchQueue.main.asyncAfter(deadline: time, execute: { [weak self] in
            self?.tappedModifierKey = NSEvent.ModifierFlags(rawValue: 0)
        })

        return Unmanaged.passUnretained(event)
    }

    func doubleTapped(with key: Int) {
        hotKeys.values
            .filter { $0.keyCombo.doubledModifiers && $0.keyCombo.modifiers == key }
            .forEach { $0.invoke() }
    }
}

//
//  AlternativeMenuToolTip.swift
//  AlternativeMenu
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit

/// Manages the delay and lifetime of text tooltips for menu rows.
///
/// The first tooltip waits for the initial delay; subsequent targets appear immediately
/// until `hide(resetDelay:)` resets that behavior. AppKit handles placement, sizing, and drawing.
/// Replacing or hiding a target cancels its pending task. A task that reaches `show`
/// finishes naturally after the display attempt.
@MainActor
final class AlternativeMenuToolTip {
    private struct Target: Equatable {
        let text: String
        let rowFrame: NSRect
    }

    private static let initialDelay: Duration = {
        let milliseconds = UserDefaults.standard.integer(forKey: "NSInitialToolTipDelay")
        return .milliseconds((1...10_000).contains(milliseconds) ? milliseconds : 1_000)
    }()

    private var target: Target?
    private var pending: Task<Void, Never>?
    private var hasShown = false
    private let nativeToolTip = NativeMenuToolTip()

    func schedule(_ text: String, beside menu: NSWindow, rowFrame: NSRect) {
        guard !text.isEmpty else {
            hide()
            return
        }

        let target = Target(text: text, rowFrame: rowFrame)
        guard self.target != target else {
            return
        }
        hide(resetDelay: false)
        self.target = target
        if hasShown {
            show(target, beside: menu)
        } else {
            pending = Task { [weak self, weak menu] in
                try? await Task.sleep(for: Self.initialDelay)
                guard !Task.isCancelled, let self, let menu else {
                    return
                }
                show(target, beside: menu)
            }
        }
    }

    func hide(resetDelay: Bool = true) {
        pending?.cancel()
        pending = nil
        target = nil
        nativeToolTip?.hide()
        if resetDelay {
            hasShown = false
        }
    }

    private func show(_ target: Target, beside menu: NSWindow) {
        pending = nil
        self.target = nil
        guard menu.isVisible,
            target.rowFrame.contains(NSEvent.mouseLocation),
            nativeToolTip?.show(target.text, beside: menu, rowFrame: target.rowFrame) == true
        else {
            return
        }
        self.target = target
        hasShown = true
    }
}

@MainActor
private final class NativeMenuToolTip {
    private static let showSelector = NSSelectorFromString("_displayTemporaryToolTipForView:withString:")
    private static let hideSelector = NSSelectorFromString("orderOutToolTipForView:")
    private let manager: NSObject
    private let anchor = NSView()

    init?() {
        let shared = NSSelectorFromString("sharedToolTipManager")
        guard let type = NSClassFromString("_NSMenuToolTipManager") as? NSObject.Type,
            type.responds(to: shared),
            let manager = type.perform(shared)?.takeUnretainedValue() as? NSObject,
            manager.responds(to: Self.showSelector),
            manager.responds(to: Self.hideSelector)
        else {
            return nil
        }
        self.manager = manager
        anchor.isHidden = true
    }

    // Returns whether a display request was submitted to AppKit.
    func show(_ text: String, beside menu: NSWindow, rowFrame: NSRect) -> Bool {
        guard let content = menu.contentView else {
            return false
        }
        hide()
        var frame = content.convert(menu.convertFromScreen(rowFrame), from: nil)
        frame.origin.x = content.bounds.minX
        frame.size.width = content.bounds.width
        anchor.frame = frame
        // AppKit converts the anchor's frame as a rectangle in its local coordinates.
        anchor.bounds = frame
        content.addSubview(anchor)

        typealias Show = @convention(c) (AnyObject, Selector, NSView, NSString) -> Void
        let invoke = unsafeBitCast(manager.method(for: Self.showSelector), to: Show.self)
        invoke(manager, Self.showSelector, anchor, text as NSString)
        return true
    }

    func hide() {
        guard anchor.superview != nil else {
            return
        }
        typealias Hide = @convention(c) (AnyObject, Selector, NSView) -> Void
        let invoke = unsafeBitCast(manager.method(for: Self.hideSelector), to: Hide.self)
        invoke(manager, Self.hideSelector, anchor)
        anchor.removeFromSuperview()
    }
}

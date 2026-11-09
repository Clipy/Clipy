//
//  AlternativeMenuVisualEffectView.swift
//  AlternativeMenu
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import SwiftUI

struct AlternativeMenuVisualEffectView: NSViewRepresentable {
    // NSMenu's private highlight material also preserves the backdrop in unemphasized rows.
    static let menuSelectionMaterial = NSVisualEffectView.Material(rawValue: 36) ?? .selection

    let material: NSVisualEffectView.Material
    let cornerRadius: CGFloat
    let isEmphasized: Bool

    func makeNSView(context: Context) -> NSVisualEffectView {
        let view = NSVisualEffectView()
        view.material = material
        view.blendingMode = .behindWindow
        view.state = .active
        view.isEmphasized = isEmphasized
        view.wantsLayer = true
        view.layer?.cornerRadius = cornerRadius
        view.layer?.cornerCurve = .continuous
        view.layer?.masksToBounds = true
        view.setAccessibilityElement(false)
        return view
    }

    func updateNSView(_ view: NSVisualEffectView, context: Context) {
        view.isEmphasized = isEmphasized
    }
}

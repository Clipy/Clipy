//
//  AlternativeMenuVisualEffectView.swift
//  AlternativeMenu
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import SwiftUI

struct AlternativeMenuVisualEffectView: NSViewRepresentable {
    let material: NSVisualEffectView.Material
    let cornerRadius: CGFloat

    func makeNSView(context: Context) -> NSVisualEffectView {
        let view = NSVisualEffectView()
        view.material = material
        view.blendingMode = .behindWindow
        view.state = .active
        view.isEmphasized = material == .selection
        view.wantsLayer = true
        view.layer?.cornerRadius = cornerRadius
        view.layer?.cornerCurve = .continuous
        view.layer?.masksToBounds = true
        view.setAccessibilityElement(false)
        return view
    }

    func updateNSView(_ view: NSVisualEffectView, context: Context) {}
}

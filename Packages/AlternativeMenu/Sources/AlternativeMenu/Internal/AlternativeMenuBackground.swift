//
//  AlternativeMenuBackground.swift
//  AlternativeMenu
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import SwiftUI

private struct AlternativeMenuBackground: View {
    let cornerRadius: CGFloat

    @Environment(\.accessibilityReduceTransparency)
    private var reducesTransparency
    @Environment(\.colorScheme)
    private var colorScheme

    var body: some View {
        let shape = RoundedRectangle(cornerRadius: cornerRadius)
        Group {
            if reducesTransparency {
                Color(nsColor: .windowBackgroundColor)
            } else if #available(macOS 26.0, *) {
                MenuGlassEffectView(cornerRadius: cornerRadius)
                    // Measured against NSMenu over colored and neutral backdrops.
                    // The extra material is closer in Dark Mode, but washes out light glass.
                    .background(Material.ultraThin.materialActiveAppearance(.active).opacity(colorScheme == .dark ? 0.3 : 0))
            } else {
                AlternativeMenuVisualEffectView(material: .menu, cornerRadius: cornerRadius, isEmphasized: false)
            }
        }
        .clipShape(shape)
        .accessibilityHidden(true)
    }
}

extension View {
    /// Applies the menu background using the current appearance and accessibility settings.
    func alternativeMenuBackground(cornerRadius: CGFloat) -> some View {
        background(AlternativeMenuBackground(cornerRadius: cornerRadius))
    }
}

@available(macOS 26.0, *)
private struct MenuGlassEffectView: NSViewRepresentable {
    let cornerRadius: CGFloat

    func makeNSView(context: Context) -> NSGlassEffectView {
        let view = NSGlassEffectView()
        view.cornerRadius = cornerRadius
        view.style = .regular
        view.tintColor = nil

        // These settings approximate the native menu's backdrop and
        // keep its appearance aligned with the system rather than the wallpaper.
        // Apply each supported setting independently, leaving unavailable ones at their defaults.
        let options: [(key: String, value: Any)] = [
            ("_variant", 0),
            ("_subvariant", "menu"),
            ("_scrimState", 0),
            ("_subduedState", 0),
            ("_interactionState", 0),
            ("_contentLensing", 0),
            ("_adaptiveAppearance", 1),
            ("_useReducedShadowRadius", false)
        ]
        for option in options where view.responds(to: NSSelectorFromString("set\(option.key):")) {
            view.setValue(option.value, forKey: option.key)
        }
        return view
    }

    func updateNSView(_ view: NSGlassEffectView, context: Context) {}
}

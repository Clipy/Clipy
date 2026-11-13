//
//  ScrollViewIntrospectionView.swift
//  AlternativeMenu
//
//  Copyright © 2015-2026 Clipy Project.
//

import AppKit
import SwiftUI

/// Provides access to the underlying `NSScrollView` instance of an enclosing SwiftUI `ScrollView`.
private struct ScrollViewIntrospectionView: NSViewRepresentable {
    let resolve: (NSScrollView?) -> Void

    func makeNSView(context: Context) -> Resolver {
        Resolver(resolve: resolve)
    }

    func updateNSView(_ view: Resolver, context: Context) {}

    final class Resolver: NSView {
        private let resolve: (NSScrollView?) -> Void

        init(resolve: @escaping (NSScrollView?) -> Void) {
            self.resolve = resolve
            super.init(frame: .zero)
        }

        @available(*, unavailable)
        required init?(coder: NSCoder) { fatalError("init(coder:) is unavailable") }

        override func viewDidMoveToWindow() {
            super.viewDidMoveToWindow()
            resolve(window == nil ? nil : enclosingScrollView)
        }
    }
}

extension View {
    /// Accesses the underlying `NSScrollView` when applied inside a SwiftUI `ScrollView`.
    func introspectScrollView(_ resolve: @escaping (NSScrollView?) -> Void) -> some View {
        background {
            ScrollViewIntrospectionView(resolve: resolve)
                .allowsHitTesting(false)
                .accessibilityHidden(true)
        }
    }
}

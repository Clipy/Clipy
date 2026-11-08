//
//  AlternativeMenuHeader.swift
//  AlternativeMenu
//
//  Copyright © 2015-2026 Clipy Project.
//

import SwiftUI

struct AlternativeMenuHeader {
    let height: CGFloat
    let minimumWidth: CGFloat
    let content: AnyView

    init<Content: View>(height: CGFloat, minimumWidth: CGFloat, @ViewBuilder content: () -> Content) {
        precondition(height.isFinite && height > 0, "The popup menu header needs a positive height.")
        precondition(minimumWidth.isFinite && minimumWidth >= 0, "The popup menu minimum width must be nonnegative.")
        self.height = height
        self.minimumWidth = minimumWidth
        self.content = AnyView(content())
    }
}

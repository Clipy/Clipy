import AppKit
import SwiftUI

struct SettingsScrollView<Content: View>: View {
    @State
    private var maximumHeight: CGFloat?

    private let content: Content

    init(@ViewBuilder content: () -> Content) {
        self.content = content()
    }

    var body: some View {
        scrollView
            .frame(maxHeight: maximumHeight)
            // Settings needs a definite height to resize the window when switching panes.
            .fixedSize(horizontal: false, vertical: true)
            .background {
                SettingsWindowSizing(maximumHeight: $maximumHeight)
            }
    }

    @ViewBuilder
    private var scrollView: some View {
        if #available(macOS 13.3, *) {
            ScrollView {
                content
            }
            .scrollBounceBehavior(.basedOnSize)
        } else {
            ScrollView {
                content
            }
        }
    }
}

private struct SettingsWindowSizing: NSViewRepresentable {
    @Binding
    var maximumHeight: CGFloat?

    func makeNSView(context: Context) -> WindowObserverView {
        WindowObserverView()
    }

    func updateNSView(_ nsView: WindowObserverView, context: Context) {
        nsView.onHeightChange = { height in
            if maximumHeight != height {
                maximumHeight = height
            }
        }
    }

    final class WindowObserverView: NSView {
        var onHeightChange: ((CGFloat) -> Void)?

        override func viewDidMoveToWindow() {
            super.viewDidMoveToWindow()
            let center = NotificationCenter.default
            center.removeObserver(self)
            guard let window else { return }

            for name in [NSWindow.didChangeScreenNotification, NSWindow.didResizeNotification] {
                center.addObserver(self, selector: #selector(updateHeight), name: name, object: window)
            }
            center.addObserver(
                self,
                selector: #selector(updateHeight),
                name: NSApplication.didChangeScreenParametersNotification,
                object: nil
            )
            updateHeight()
        }

        @objc private func updateHeight() {
            // Defer the state change until AppKit has finished attaching or resizing the view.
            DispatchQueue.main.async { [weak self] in
                guard let self, let window = self.window, let screen = window.screen else { return }
                let visibleFrame = screen.visibleFrame
                let chromeHeight = window.frameRect(forContentRect: .zero).height
                self.onHeightChange?(max(0, visibleFrame.height - chromeHeight))

                // A pane can grow downward while keeping the window's top edge in place.
                let frame = window.frame
                guard frame.height <= visibleFrame.height else { return }
                let originY = min(max(frame.minY, visibleFrame.minY), visibleFrame.maxY - frame.height)
                if originY != frame.minY {
                    window.setFrameOrigin(NSPoint(x: frame.minX, y: originY))
                }
            }
        }

        deinit {
            NotificationCenter.default.removeObserver(self)
        }
    }
}

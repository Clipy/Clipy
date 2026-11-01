import AppKit
import Dependencies
import DependenciesTestSupport
import Testing
@testable import Clipy

@MainActor
@Suite(
    .dependencies {
        try $0.bootstrapDatabase()
    }
)
struct SnippetsEditorWindowTests {
    @Test
    func firstPresentationKeepsInitialSizeAndMinimum() throws {
        try withWindow { controller in
            let window = try #require(controller.window)
            #expect(window.contentView?.frame.size == NSSize(width: 800, height: 600))

            controller.showWindow(nil)
            window.contentView?.layoutSubtreeIfNeeded()

            #expect(window.contentMinSize == NSSize(width: 800, height: 600))
            #expect(try #require(window.contentView).frame.width >= 800)
            #expect(try #require(window.contentView).frame.height >= 600)
        }
    }

    @Test
    func repairsSavedFrameThatIsSmallerThanMinimum() throws {
        try withWindow(savedContentSize: NSSize(width: 1, height: 10)) { controller in
            controller.showWindow(nil)
            let window = try #require(controller.window)
            window.contentView?.layoutSubtreeIfNeeded()

            #expect(window.contentMinSize == NSSize(width: 800, height: 600))
            #expect(try #require(window.contentView).frame.width >= 800)
            #expect(try #require(window.contentView).frame.height >= 600)
        }
    }

    @Test
    func preservesValidSavedWidth() throws {
        try withWindow(savedContentSize: NSSize(width: 1100, height: 760)) { controller in
            controller.showWindow(nil)
            let window = try #require(controller.window)
            window.contentView?.layoutSubtreeIfNeeded()

            #expect(try #require(window.contentView).frame.width == 1100)
            #expect(window.contentMinSize == NSSize(width: 800, height: 600))
        }
    }

    private func withWindow(
        savedContentSize: NSSize? = nil,
        operation: (SnippetsEditorWindowController) throws -> Void
    ) throws {
        let autosaveName = "SnippetsEditorWindowTests.\(UUID().uuidString)"
        defer { NSWindow.removeFrame(usingName: autosaveName) }
        if let savedContentSize {
            let savedWindow = NSWindow(
                contentRect: NSRect(origin: NSPoint(x: 100, y: 100), size: savedContentSize),
                styleMask: [.titled, .closable, .miniaturizable, .resizable],
                backing: .buffered,
                defer: false
            )
            savedWindow.isReleasedWhenClosed = false
            savedWindow.saveFrame(usingName: autosaveName)
            savedWindow.close()
        }

        let repository = SnippetRepository()
        try withDependencies {
            $0.snippetRepository = repository
        } operation: {
            let controller = SnippetsEditorWindowController(frameAutosaveName: autosaveName)
            defer { controller.close() }
            try operation(controller)
        }
    }
}

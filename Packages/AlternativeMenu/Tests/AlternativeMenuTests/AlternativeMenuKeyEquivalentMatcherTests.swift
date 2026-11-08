import AppKit
import Carbon
import Testing
@testable import AlternativeMenu

@MainActor
@Suite(.serialized)
struct AlternativeMenuKeyEquivalentMatcherTests {
    init() {
        _ = NSApplication.shared
    }

    @Test
    func findsTheFirstDuplicateWithoutRunningItsAction() throws {
        let matcher = AlternativeMenuKeyEquivalentMatcher()
        var activated = false
        matcher.updateItems([
            .action("First", keyEquivalent: "q") { _ in activated = true },
            .action("Second", keyEquivalent: "q") { _ in activated = true }
        ])

        let result = matcher.match(try keyEvent("q", code: kVK_ANSI_Q))
        #expect(result.handled)
        #expect(result.item?.title == "First")
        #expect(result.path == [0])
        #expect(!activated)

        let extraModifier = matcher.match(try keyEvent("q", code: kVK_ANSI_Q, modifiers: [.command, .option]))
        #expect(!extraModifier.handled)
        #expect(extraModifier.item?.title == nil)
        #expect(extraModifier.path == nil)
    }

    @Test(arguments: [false, true])
    func searchesUnopenedSubmenusInMenuOrder(rootFirst: Bool) throws {
        let matcher = AlternativeMenuKeyEquivalentMatcher()
        let root = AlternativeMenuItem.action("Root", keyEquivalent: "q") { _ in }
        let submenu = AlternativeMenuItem.submenu("Parent", menu: .init(items: [
            .submenu("Nested", menu: .init(items: [.action("Grandchild", keyEquivalent: "q") { _ in }]))
        ]))
        matcher.updateItems(rootFirst ? [root, submenu] : [submenu, root])

        let result = matcher.match(try keyEvent("q", code: kVK_ANSI_Q))
        #expect(result.handled)
        #expect(result.item?.title == (rootFirst ? "Root" : "Grandchild"))
        #expect(result.path == (rootFirst ? [0] : [0, 0, 0]))
    }

    @Test(arguments: [false, true], [false, true])
    func handlesDisabledEquivalentsLikeNSMenu(usesCGEvent: Bool, hasEnabledDuplicate: Bool) throws {
        let matcher = AlternativeMenuKeyEquivalentMatcher()
        let disabled = AlternativeMenuItem.action("Disabled", isEnabled: false, keyEquivalent: "q") { _ in }
        let enabled = AlternativeMenuItem.action("Enabled", keyEquivalent: "q") { _ in }
        let items = hasEnabledDuplicate ? [disabled, enabled] : [disabled]
        matcher.updateItems(items)

        let event = try usesCGEvent ? cgKeyEvent(kVK_ANSI_Q) : keyEvent("q", code: kVK_ANSI_Q)
        let native = nativeMatch(event, items: items)
        let result = matcher.match(event)
        #expect(result.handled == native.handled)
        #expect(result.item?.title == native.title)
        #expect(result.item?.title != "Disabled")
    }

    @Test(arguments: [false, true])
    func handlesImplicitAndExplicitShiftLikeNSMenu(usesImplicitShift: Bool) throws {
        let matcher = AlternativeMenuKeyEquivalentMatcher()
        let modifiers: NSEvent.ModifierFlags = usesImplicitShift ? .command : [.command, .shift]
        let item = AlternativeMenuItem.action(
            "Shifted", keyEquivalent: usesImplicitShift ? "A" : "a", keyEquivalentModifierMask: modifiers
        ) { _ in }
        matcher.updateItems([item])

        for eventModifiers: NSEvent.ModifierFlags in [.command, [.command, .shift], [.command, .shift, .option]] {
            let event = try cgKeyEvent(kVK_ANSI_A, modifiers: eventModifiers)
            let native = nativeMatch(event, items: [item])
            let result = matcher.match(event)
            #expect(result.handled == native.handled)
            #expect(result.item?.title == native.title)
        }
    }

    @Test
    func replacingItemsReleasesOldActionsAndRemovesTheirEquivalents() throws {
        let matcher = AlternativeMenuKeyEquivalentMatcher()
        let oldKey = try keyEvent("o", code: kVK_ANSI_O)
        // Drain AppKit's temporary references from performing the key equivalent.
        let retainsOldAction = autoreleasepool {
            let token = NSObject()
            matcher.updateItems([.action("Old", keyEquivalent: "o") { [token] _ in
                withExtendedLifetime(token) {}
            }])
            #expect(matcher.match(oldKey).item?.title == "Old")
            return { [weak token] in token != nil }
        }
        try #require(retainsOldAction())

        matcher.updateItems([.separator, .action("New", keyEquivalent: "n") { _ in }, .action("No key") { _ in }])

        #expect(!retainsOldAction())
        #expect(matcher.display(at: [0]) == nil)
        #expect(matcher.display(at: [1])?.accessibilityText == "⌘N")
        #expect(matcher.display(at: [2]) == nil)
        let stale = matcher.match(oldKey)
        #expect(!stale.handled)
        #expect(stale.item?.title == nil)
        let newKey = try keyEvent("n", code: kVK_ANSI_N)
        let replacement = matcher.match(newKey)
        #expect(replacement.item?.title == "New")

        matcher.updateItems([])
        #expect(matcher.display(at: [1]) == nil)
        #expect(!matcher.match(newKey).handled)
    }

    private func nativeMatch(_ event: NSEvent, items: [AlternativeMenuItem]) -> (handled: Bool, title: String?) {
        let receiver = NativeReceiver()
        let menu = NSMenu()
        menu.autoenablesItems = false
        menu.items = items.map { nativeItem(for: $0, receiver: receiver) }
        _ = menu.accessibilityChildren()
        let handled = menu.performKeyEquivalent(with: event)
        return (handled, receiver.title)
    }

    private func nativeItem(for item: AlternativeMenuItem, receiver: NativeReceiver) -> NSMenuItem {
        let native = NSMenuItem(title: item.title, action: nil, keyEquivalent: item.keyEquivalent)
        native.keyEquivalentModifierMask = item.keyEquivalentModifierMask
        native.allowsAutomaticKeyEquivalentLocalization = true
        native.allowsAutomaticKeyEquivalentMirroring = true
        if let submenu = item.submenu {
            let menu = NSMenu()
            menu.autoenablesItems = false
            menu.items = submenu.items.map { nativeItem(for: $0, receiver: receiver) }
            native.submenu = menu
        } else {
            native.action = #selector(NativeReceiver.record(_:))
            native.target = receiver
        }
        native.isEnabled = item.isEnabled
        return native
    }

    private func keyEvent(
        _ characters: String, code: Int, modifiers: NSEvent.ModifierFlags = .command
    ) throws -> NSEvent {
        try #require(
            NSEvent.keyEvent(
                with: .keyDown,
                location: .zero,
                modifierFlags: modifiers,
                timestamp: 0,
                windowNumber: 0,
                context: nil,
                characters: characters,
                charactersIgnoringModifiers: characters,
                isARepeat: false,
                keyCode: UInt16(code)
            )
        )
    }

    private func cgKeyEvent(_ code: Int, modifiers: NSEvent.ModifierFlags = .command) throws -> NSEvent {
        let event = try #require(
            CGEvent(
                keyboardEventSource: nil,
                virtualKey: UInt16(code),
                keyDown: true
            )
        )
        event.flags = CGEventFlags(rawValue: UInt64(modifiers.rawValue))
        return try #require(NSEvent(cgEvent: event))
    }

    @MainActor
    private final class NativeReceiver: NSObject {
        private(set) var title: String?

        @objc func record(_ item: NSMenuItem) {
            title = item.title
        }
    }
}

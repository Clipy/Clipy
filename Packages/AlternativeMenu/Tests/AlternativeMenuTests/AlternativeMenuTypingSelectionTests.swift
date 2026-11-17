import AppKit
import Testing

@testable import AlternativeMenu

@MainActor
struct AlternativeMenuTypingSelectionTests {
    @Test
    func matchesTitlesInMenuOrderAndSkipsUnavailableRows() throws {
        let typingSelection = AlternativeMenuTypingSelection()
        let items: [AlternativeMenuItem] = [
            .sectionHeader("A"),
            .action("A", isEnabled: false) { _ in },
            .separator,
            .action("Apple") { _ in },
            .submenu("Apricot", menu: .init(items: [.action("Child") { _ in }])),
            .action("Aardvark") { _ in }
        ]

        let first = typingSelection.select(try key("A", modifiers: .shift), in: items)
        #expect(first.handled)
        #expect(first.index == 3)
        #expect(typingSelection.select(try key("p", at: 10.1), in: items).index == 3)
        #expect(typingSelection.select(try key("r", at: 10.2), in: items).index == 4)
    }

    @Test
    func prefersExactTitleAndAppendsRepeatedCharacters() throws {
        let typingSelection = AlternativeMenuTypingSelection()
        let items: [AlternativeMenuItem] = [
            .action("Apple") { _ in },
            .action("A") { _ in },
            .action("Aardvark") { _ in }
        ]

        #expect(typingSelection.select(try key("a"), in: items).index == 1)
        #expect(typingSelection.select(try key("a", at: 10.1), in: items).index == 2)
        #expect(typingSelection.select(try key("a", at: 10.2), in: items).index == 2)
    }

    @Test(arguments: [
        (["Éclair", "Eagle"], "e", 1),
        (["é", "e"], "e", 1),
        (["ßeta", "sseta"], "ss", 1),
        (["10 Ten", "1 One", "11 Eleven"], "1", 0)
    ])
    func followsNativeTitleCollation(titles: [String], input: String, expectedIndex: Int) throws {
        let typingSelection = AlternativeMenuTypingSelection()
        let items = titles.map { AlternativeMenuItem.action($0) { _ in } }
        var selection: Int?
        for (index, character) in input.enumerated() {
            selection = typingSelection.select(try key(String(character), at: 10 + Double(index) * 0.1), in: items).index
        }
        #expect(selection == expectedIndex)
    }

    @Test
    func followsSystemCollationForFullWidthLatinTitles() throws {
        let typingSelection = AlternativeMenuTypingSelection()
        let items: [AlternativeMenuItem] = [
            .action("Ａpple") { _ in },
            .action("Apple") { _ in }
        ]
        // CoreServices prefers the half-width prefix on macOS 27; earlier versions match in menu order.
        let expectedIndex = if #available(macOS 27, *) { 1 } else { 0 }

        #expect(typingSelection.select(try key("a"), in: items).index == expectedIndex)
    }

    @Test
    func startsFreshAfterPausingOrResetting() throws {
        let typingSelection = AlternativeMenuTypingSelection()
        let items: [AlternativeMenuItem] = [
            .action("Apple") { _ in },
            .action("Pear") { _ in }
        ]

        #expect(typingSelection.select(try key("a"), in: items).index == 0)
        #expect(typingSelection.select(try key("p", at: 10.1), in: items).index == 0)
        #expect(typingSelection.select(try key("p", at: 30), in: items).index == 1)
        typingSelection.reset()
        #expect(typingSelection.select(try key("a", at: 30.1), in: items).index == 0)
    }

    @Test
    func followsSystemDeletionRules() throws {
        let typingSelection = AlternativeMenuTypingSelection()
        let items: [AlternativeMenuItem] = [
            .action("A") { _ in },
            .action("AB") { _ in },
            .action("B") { _ in }
        ]

        #expect(typingSelection.select(try key("a"), in: items).index == 0)
        #expect(typingSelection.select(try key("\u{7f}", at: 10.1), in: items).index == 0)
        #expect(typingSelection.select(try key("b", at: 10.2), in: items).index == 1)
        let clear = typingSelection.select(try key("\u{8}", at: 10.3), in: items)
        #expect(clear.handled)
        #expect(clear.index == nil)
        #expect(typingSelection.select(try key("b", at: 10.4), in: items).index == 2)
    }

    @Test
    func nearestMatchSkipsDisabledRows() throws {
        let typingSelection = AlternativeMenuTypingSelection()
        let items: [AlternativeMenuItem] = [
            .action("Apple") { _ in },
            .action("Zulu", isEnabled: false) { _ in },
            .separator,
            .action("Grapes") { _ in },
            .sectionHeader("Zebra")
        ]

        #expect(typingSelection.select(try key("z"), in: items).index == 3)
        typingSelection.reset()
        #expect(typingSelection.select(try key("z"), in: [.action("Zulu", isEnabled: false) { _ in }, .separator]).index == nil)
    }

    @Test
    func matchesEmptyEnabledSubmenusAndSkipsDisabledSubmenus() throws {
        let typingSelection = AlternativeMenuTypingSelection()
        let items: [AlternativeMenuItem] = [
            .submenu("Apple", menu: .init(items: [.action("Child") { _ in }]), isEnabled: false),
            .submenu("Apricot", menu: .init(items: [])),
            .action("Avocado") { _ in }
        ]

        #expect(typingSelection.select(try key("a"), in: items).index == 1)
        #expect(typingSelection.select(try key("v", at: 10.1), in: items).index == 2)
    }

    @Test
    func ignoresModifiedAndNavigationKeys() throws {
        let typingSelection = AlternativeMenuTypingSelection()
        let items: [AlternativeMenuItem] = [
            .action("Apple") { _ in },
            .action("Pear") { _ in }
        ]

        #expect(typingSelection.select(try key("a"), in: items).index == 0)
        for modifier: NSEvent.ModifierFlags in [.command, .option, .control] {
            #expect(!typingSelection.select(try key("q", at: 10.1, modifiers: modifier), in: items).handled)
        }
        #expect(!typingSelection.select(try key("\u{f701}", at: 10.2), in: items).handled)
        #expect(typingSelection.select(try key("p", at: 10.3), in: items).index == 0)
    }

    private func key(_ characters: String, at timestamp: TimeInterval = 10, modifiers: NSEvent.ModifierFlags = []) throws -> NSEvent {
        try #require(
            NSEvent.keyEvent(
                with: .keyDown,
                location: .zero,
                modifierFlags: modifiers,
                timestamp: timestamp,
                windowNumber: 0,
                context: nil,
                characters: characters,
                charactersIgnoringModifiers: characters,
                isARepeat: false,
                keyCode: 0
            )
        )
    }
}

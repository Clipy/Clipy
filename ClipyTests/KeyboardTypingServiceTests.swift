import Cocoa
import Testing
@testable import Clipy

@MainActor
struct KeyboardTypingServiceTests {
    @Test(arguments: [
        "Hello, World! 123 @#$%^&*()",
        "中文输入 café e\u{301}",
        "😀👨‍👩‍👧‍👦🇸🇬",
        "first\nsecond\r\nthird\tlast",
        "a" + String(repeating: "\u{301}", count: 40)
    ])
    func preservesUnicodeAndPostsKeyPairs(_ text: String) {
        let harness = Harness()
        harness.service.type(text, into: 123)

        #expect(harness.events.isEmpty)
        #expect(harness.pending.first?.delay == 0.2)
        #expect(harness.service.isTyping)
        harness.drain()

        #expect(harness.typedText == text)
        #expect(harness.events.count == text.unicodeScalars.count * 2)
        #expect(harness.checkedProcessIDs.allSatisfy { $0 == 123 })
        for index in stride(from: 0, to: harness.events.count, by: 2) {
            let keyDown = harness.events[index]
            let keyUp = harness.events[index + 1]
            #expect(keyDown.type == .keyDown)
            #expect(keyUp.type == .keyUp)
            #expect(keyDown.flags.isEmpty && keyUp.flags.isEmpty)
            #expect(Self.text(in: keyDown) == Self.text(in: keyUp))
        }
        #expect(!harness.service.isTyping)
    }

    @Test
    func emptyTextDoesNotScheduleInput() {
        let harness = Harness()
        harness.service.type("", into: 123)
        #expect(harness.pending.isEmpty)
        #expect(harness.events.isEmpty)
        #expect(!harness.service.isTyping)
    }

    @Test
    func waitsForShortcutModifiersToBeReleased() {
        let harness = Harness()
        harness.hasModifiers = true
        harness.service.type("abc", into: 123)
        harness.step()
        #expect(harness.events.isEmpty)
        #expect(harness.pending.first?.delay == 0.02)

        harness.hasModifiers = false
        harness.step()
        #expect(harness.typedText == "a")
        #expect(harness.pending.first?.delay == 0.01)
        harness.drain()
        #expect(harness.typedText == "abc")
    }

    @Test
    func heldModifiersEventuallyCancel() {
        let harness = Harness()
        harness.hasModifiers = true
        harness.service.type("abc", into: 123)
        harness.drain()
        #expect(harness.events.isEmpty)
        #expect(!harness.service.isTyping)
    }

    @Test
    func focusChangeOrEscapeBeforeStartingCancelsInput() {
        let harness = Harness()
        harness.service.type("abc", into: 123)
        harness.shouldContinue = false
        harness.drain()
        #expect(harness.events.isEmpty)
        #expect(!harness.service.isTyping)
    }

    @Test
    func focusChangeOrEscapeDuringTypingStopsRemainingInput() {
        let harness = Harness()
        harness.service.type("abc", into: 123)
        harness.step()
        harness.shouldContinue = false
        harness.drain()
        #expect(harness.typedText == "a")
        #expect(!harness.service.isTyping)
    }

    @Test
    func modifiersDuringTypingCancelInput() {
        let harness = Harness()
        harness.service.type("abc", into: 123)
        harness.step()
        harness.hasModifiers = true
        harness.drain()
        #expect(harness.typedText == "a")
        #expect(!harness.service.isTyping)
    }

    @Test
    func aNewRequestCancelsPendingCharacters() {
        let harness = Harness()
        harness.service.type("abc", into: 123)
        harness.step()
        harness.service.type("XYZ", into: 456)
        harness.drain()
        #expect(harness.typedText == "aXYZ")
        #expect(!harness.service.isTyping)
    }

    @Test
    func explicitCancellationStopsPendingInput() {
        let harness = Harness()
        harness.service.type("abc", into: 123)
        harness.service.cancel()
        harness.drain()
        #expect(harness.events.isEmpty)
        #expect(!harness.service.isTyping)
    }

    private static func text(in event: CGEvent) -> String {
        var length = 0
        var characters = [UniChar](repeating: 0, count: 2)
        event.keyboardGetUnicodeString(maxStringLength: characters.count, actualStringLength: &length, unicodeString: &characters)
        return String(decoding: characters.prefix(length), as: UTF16.self)
    }

    @MainActor
    private final class Harness {
        var pending: [(delay: TimeInterval, action: @MainActor @Sendable () -> Void)] = []
        var events: [CGEvent] = []
        var checkedProcessIDs: [pid_t] = []
        var shouldContinue = true
        var hasModifiers = false

        lazy var service = KeyboardTypingService(
            schedule: { [unowned self] in pending.append(($0, $1)) },
            canContinue: { [unowned self] processID in
                checkedProcessIDs.append(processID)
                return shouldContinue
            },
            modifiersArePressed: { [unowned self] in hasModifiers },
            postEvent: { [unowned self] in events.append($0) }
        )

        var typedText: String {
            events.filter { $0.type == .keyDown }.map { KeyboardTypingServiceTests.text(in: $0) }.joined()
        }

        func step() {
            guard !pending.isEmpty else { return }
            pending.removeFirst().action()
        }

        func drain() {
            for _ in 0..<1_000 {
                guard !pending.isEmpty else { return }
                step()
            }
            Issue.record("Typing did not finish within 1,000 scheduled callbacks")
        }
    }
}

//
//  KeyboardTypingService.swift
//  Clipy
//

import Cocoa

/// Sends text as Unicode keyboard events without invoking the paste command.
@MainActor
final class KeyboardTypingService {
    typealias Schedule = (TimeInterval, @escaping @MainActor @Sendable () -> Void) -> Void

    private let schedule: Schedule
    private let canContinue: (pid_t) -> Bool
    private let modifiersArePressed: () -> Bool
    private let postEvent: (CGEvent) -> Void
    private var requestID: UUID?

    var isTyping: Bool { requestID != nil }

    init(
        schedule: @escaping Schedule = { delay, action in
            DispatchQueue.main.asyncAfter(deadline: .now() + delay, execute: action)
        },
        canContinue: @escaping (pid_t) -> Bool = { processID in
            NSWorkspace.shared.frontmostApplication?.processIdentifier == processID
                && !CGEventSource.keyState(.combinedSessionState, key: 53) // Escape
        },
        modifiersArePressed: @escaping () -> Bool = {
            let modifiers: CGEventFlags = [.maskCommand, .maskControl, .maskAlternate, .maskShift]
            return !CGEventSource.flagsState(.combinedSessionState).intersection(modifiers).isEmpty
        },
        postEvent: @escaping (CGEvent) -> Void = { $0.post(tap: .cgAnnotatedSessionEventTap) }
    ) {
        self.schedule = schedule
        self.canContinue = canContinue
        self.modifiersArePressed = modifiersArePressed
        self.postEvent = postEvent
    }

    func type(_ text: String, into processID: pid_t) {
        cancel()
        guard !text.isEmpty, let source = CGEventSource(stateID: .privateState) else { return }
        // Keep real input available so the user can interrupt a long operation.
        source.localEventsSuppressionInterval = 0
        let requestID = UUID()
        self.requestID = requestID
        // Let menu tracking finish and focus return to the original input.
        schedule(0.2) { [weak self] in
            self?.typeNext(text.unicodeScalars.makeIterator(), source: source, into: processID, requestID: requestID, modifierRetries: 100)
        }
    }

    func cancel() {
        requestID = nil
    }

    private func typeNext(
        _ iterator: String.UnicodeScalarView.Iterator,
        source: CGEventSource,
        into processID: pid_t,
        requestID: UUID,
        modifierRetries: Int
    ) {
        guard self.requestID == requestID else { return }
        guard canContinue(processID) else {
            cancel()
            return
        }
        // A menu shortcut may still be held down. Wait up to two seconds before
        // starting; modifiers pressed during typing cancel the operation.
        if modifiersArePressed() {
            guard modifierRetries > 0 else {
                cancel()
                return
            }
            schedule(0.02) { [weak self] in
                self?.typeNext(iterator, source: source, into: processID, requestID: requestID, modifierRetries: modifierRetries - 1)
            }
            return
        }

        var iterator = iterator
        guard let scalar = iterator.next(),
              let keyDown = CGEvent(keyboardEventSource: source, virtualKey: 0, keyDown: true),
              let keyUp = CGEvent(keyboardEventSource: source, virtualKey: 0, keyDown: false) else {
            cancel()
            return
        }

        // One scalar per event keeps surrogate pairs intact and stays below the
        // event's Unicode length limit, even for very long combining sequences.
        let characters = Array(String(scalar).utf16)
        for event in [keyDown, keyUp] {
            event.flags = []
            event.keyboardSetUnicodeString(stringLength: characters.count, unicodeString: characters)
            postEvent(event)
        }
        let remaining = iterator
        schedule(0.01) { [weak self] in
            self?.typeNext(remaining, source: source, into: processID, requestID: requestID, modifierRetries: 0)
        }
    }
}

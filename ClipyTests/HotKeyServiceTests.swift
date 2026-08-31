import Carbon
import Dependencies
import DependenciesTestSupport
import Foundation
import Magnet
import Sharing
import Testing
@testable import Clipy

@Suite(.serialized, .dependencies)
final class HotKeyServiceTests {
    @Shared(.mainKeyCombo) var mainKeyCombo
    @Shared(.historyKeyCombo) var historyKeyCombo
    @Shared(.snippetKeyCombo) var snippetKeyCombo
    @Shared(.editSnippetsKeyCombo) var editSnippetsKeyCombo
    @Shared(.clearHistoryKeyCombo) var clearHistoryKeyCombo
    @Shared(.folderKeyCombos) var folderKeyCombos

    @Test
    func saveKeyCombos() throws {
        let service = HotKeyService()
        service.change(with: .main, keyCombo: KeyCombo(QWERTYKeyCode: 9, carbonModifiers: 768))
        service.change(with: .history, keyCombo: KeyCombo(doubledCocoaModifiers: .command))
        service.change(with: .snippet, keyCombo: KeyCombo(QWERTYKeyCode: 0, cocoaModifiers: .shift))

        #expect(mainKeyCombo?.QWERTYKeyCode == 9)
        #expect(mainKeyCombo?.modifiers == 768)
        #expect(mainKeyCombo?.doubledModifiers == false)
        #expect(mainKeyCombo?.keyEquivalent.uppercased() == "V")

        #expect(historyKeyCombo?.QWERTYKeyCode == 0)
        #expect(historyKeyCombo?.modifiers == cmdKey)
        #expect(historyKeyCombo?.doubledModifiers == true)
        #expect(historyKeyCombo?.keyEquivalent.uppercased() == "")

        #expect(snippetKeyCombo?.QWERTYKeyCode == 0)
        #expect(snippetKeyCombo?.modifiers == shiftKey)
        #expect(snippetKeyCombo?.doubledModifiers == false)
        #expect(snippetKeyCombo?.keyEquivalent.uppercased() == "A")

        service.change(with: .main, keyCombo: nil)
        #expect(mainKeyCombo == nil)
    }

    @Test
    func addAndRemoveClearHistoryHotkey() throws {
        let service = HotKeyService()

        #expect(clearHistoryKeyCombo == nil)

        service.changeClearHistoryKeyCombo(KeyCombo(QWERTYKeyCode: 0, carbonModifiers: cmdKey))

        #expect(clearHistoryKeyCombo?.QWERTYKeyCode == 0)
        #expect(clearHistoryKeyCombo?.modifiers == cmdKey)
        #expect(clearHistoryKeyCombo?.doubledModifiers == false)
        #expect(clearHistoryKeyCombo?.keyEquivalent.uppercased() == "A")

        service.changeClearHistoryKeyCombo(nil)
        #expect(clearHistoryKeyCombo == nil)
    }

    @Test
    func addAndRemoveEditSnippetsHotkey() throws {
        let service = HotKeyService()

        #expect(editSnippetsKeyCombo == nil)

        service.changeEditSnippetsKeyCombo(KeyCombo(QWERTYKeyCode: 14, carbonModifiers: cmdKey))

        #expect(editSnippetsKeyCombo?.QWERTYKeyCode == 14)
        #expect(editSnippetsKeyCombo?.modifiers == cmdKey)
        #expect(editSnippetsKeyCombo?.doubledModifiers == false)
        #expect(editSnippetsKeyCombo?.keyEquivalent.uppercased() == "E")

        service.changeEditSnippetsKeyCombo(nil)
        #expect(editSnippetsKeyCombo == nil)
    }
}

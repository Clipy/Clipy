import Carbon
import Foundation
import Magnet
import Testing
@testable import Clipy

@Suite(.serialized)
final class HotKeyServiceTests {
    init() {
        let defaults = UserDefaults.standard
        defaults.removeObject(forKey: Constants.UserDefaults.hotKeys)
        defaults.removeObject(forKey: Constants.HotKey.migrateNewKeyCombo)
        defaults.removeObject(forKey: Constants.HotKey.mainKeyCombo)
        defaults.removeObject(forKey: Constants.HotKey.historyKeyCombo)
        defaults.removeObject(forKey: Constants.HotKey.snippetKeyCombo)
        defaults.removeObject(forKey: Constants.HotKey.clearHistoryKeyCombo)
        defaults.synchronize()
    }

    deinit {
        let defaults = UserDefaults.standard
        defaults.removeObject(forKey: Constants.UserDefaults.hotKeys)
        defaults.removeObject(forKey: Constants.HotKey.migrateNewKeyCombo)
        defaults.removeObject(forKey: Constants.HotKey.mainKeyCombo)
        defaults.removeObject(forKey: Constants.HotKey.historyKeyCombo)
        defaults.removeObject(forKey: Constants.HotKey.snippetKeyCombo)
        defaults.removeObject(forKey: Constants.HotKey.clearHistoryKeyCombo)
        defaults.synchronize()
    }

    @Test
    func migrateDefaultSettings() throws {
        let service = HotKeyService()
        #expect(service.mainKeyCombo == nil)
        #expect(service.historyKeyCombo == nil)
        #expect(service.snippetKeyCombo == nil)

        let defaults = UserDefaults.standard
        #expect(defaults.bool(forKey: Constants.HotKey.migrateNewKeyCombo) == false)
        service.setupDefaultHotKeys()
        #expect(defaults.bool(forKey: Constants.HotKey.migrateNewKeyCombo) == true)

        let mainKeyCombo = try #require(service.mainKeyCombo)
        #expect(mainKeyCombo.QWERTYKeyCode == 9)
        #expect(mainKeyCombo.modifiers == 768)
        #expect(mainKeyCombo.doubledModifiers == false)
        #expect(mainKeyCombo.keyEquivalent.uppercased() == "V")

        let historyKeyCombo = try #require(service.historyKeyCombo)
        #expect(historyKeyCombo.QWERTYKeyCode == 9)
        #expect(historyKeyCombo.modifiers == 4352)
        #expect(historyKeyCombo.doubledModifiers == false)
        #expect(historyKeyCombo.keyEquivalent.uppercased() == "V")

        let snippetKeyCombo = try #require(service.snippetKeyCombo)
        #expect(snippetKeyCombo.QWERTYKeyCode == 11)
        #expect(snippetKeyCombo.modifiers == 768)
        #expect(snippetKeyCombo.doubledModifiers == false)
        #expect(snippetKeyCombo.keyEquivalent.uppercased() == "B")
    }

    @Test
    func migrateCustomizeSettings() throws {
        let service = HotKeyService()
        #expect(service.mainKeyCombo == nil)
        #expect(service.historyKeyCombo == nil)
        #expect(service.snippetKeyCombo == nil)

        let defaults = UserDefaults.standard
        let defaultKeyCombos: [String: Any] = [Constants.Menu.clip: ["keyCode": 0, "modifiers": 4352],
                                               Constants.Menu.history: ["keyCode": 9, "modifiers": 768],
                                               Constants.Menu.snippet: ["keyCode": 11, "modifiers": 4352]]
        defaults.register(defaults: [Constants.UserDefaults.hotKeys: defaultKeyCombos])
        defaults.synchronize()

        #expect(defaults.bool(forKey: Constants.HotKey.migrateNewKeyCombo) == false)
        service.setupDefaultHotKeys()
        #expect(defaults.bool(forKey: Constants.HotKey.migrateNewKeyCombo) == true)

        let mainKeyCombo = try #require(service.mainKeyCombo)
        #expect(mainKeyCombo.QWERTYKeyCode == 0)
        #expect(mainKeyCombo.modifiers == 4352)
        #expect(mainKeyCombo.doubledModifiers == false)
        #expect(mainKeyCombo.keyEquivalent.uppercased() == "A")

        let historyKeyCombo = try #require(service.historyKeyCombo)
        #expect(historyKeyCombo.QWERTYKeyCode == 9)
        #expect(historyKeyCombo.modifiers == 768)
        #expect(historyKeyCombo.doubledModifiers == false)
        #expect(historyKeyCombo.keyEquivalent.uppercased() == "V")

        let snippetKeyCombo = try #require(service.snippetKeyCombo)
        #expect(snippetKeyCombo.QWERTYKeyCode == 11)
        #expect(snippetKeyCombo.modifiers == 4352)
        #expect(snippetKeyCombo.doubledModifiers == false)
        #expect(snippetKeyCombo.keyEquivalent.uppercased() == "B")
    }

    @Test
    func saveKeyCombos() throws {
        let defaults = UserDefaults.standard
        defaults.set(true, forKey: Constants.HotKey.migrateNewKeyCombo)

        let service = HotKeyService()
        #expect(service.mainKeyCombo == nil)
        #expect(service.historyKeyCombo == nil)
        #expect(service.snippetKeyCombo == nil)

        #expect(defaults.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.mainKeyCombo) == nil)
        #expect(defaults.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.historyKeyCombo) == nil)
        #expect(defaults.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.snippetKeyCombo) == nil)

        service.setupDefaultHotKeys()
        #expect(service.mainKeyCombo == nil)
        #expect(service.historyKeyCombo == nil)
        #expect(service.snippetKeyCombo == nil)

        let mainKeyCombo = try #require(KeyCombo(QWERTYKeyCode: 9, carbonModifiers: 768))
        let historyKeyCombo = try #require(KeyCombo(doubledCocoaModifiers: .command))
        let snippetKeyCombo = try #require(KeyCombo(QWERTYKeyCode: 0, cocoaModifiers: .shift))

        service.change(with: .main, keyCombo: mainKeyCombo)
        service.change(with: .history, keyCombo: historyKeyCombo)
        service.change(with: .snippet, keyCombo: snippetKeyCombo)

        let savedMainKeyCombo = try #require(defaults.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.mainKeyCombo))
        let savedHistoryKeyCombo = try #require(defaults.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.historyKeyCombo))
        let savedSnippetKeyCombo = try #require(defaults.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.snippetKeyCombo))

        #expect(savedMainKeyCombo.QWERTYKeyCode == 9)
        #expect(savedMainKeyCombo.modifiers == 768)
        #expect(savedMainKeyCombo.doubledModifiers == false)
        #expect(savedMainKeyCombo.keyEquivalent.uppercased() == "V")

        #expect(savedHistoryKeyCombo.QWERTYKeyCode == 0)
        #expect(savedHistoryKeyCombo.modifiers == cmdKey)
        #expect(savedHistoryKeyCombo.doubledModifiers == true)
        #expect(savedHistoryKeyCombo.keyEquivalent.uppercased() == "")

        #expect(savedSnippetKeyCombo.QWERTYKeyCode == 0)
        #expect(savedSnippetKeyCombo.modifiers == shiftKey)
        #expect(savedSnippetKeyCombo.doubledModifiers == false)
        #expect(savedSnippetKeyCombo.keyEquivalent.uppercased() == "A")

        service.change(with: .main, keyCombo: nil)
        #expect(service.mainKeyCombo == nil)
        #expect(defaults.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.mainKeyCombo) == nil)
    }

    @Test
    func unarchiveSavedKeyCombos() throws {
        let defaults = UserDefaults.standard
        defaults.set(true, forKey: Constants.HotKey.migrateNewKeyCombo)

        let mainKeyCombo = try #require(KeyCombo(QWERTYKeyCode: 9, carbonModifiers: 768))
        let historyKeyCombo = try #require(KeyCombo(doubledCocoaModifiers: .command))
        let snippetKeyCombo = try #require(KeyCombo(QWERTYKeyCode: 0, cocoaModifiers: .shift))

        defaults.setArchiveData(mainKeyCombo, forKey: Constants.HotKey.mainKeyCombo)
        defaults.setArchiveData(historyKeyCombo, forKey: Constants.HotKey.historyKeyCombo)
        defaults.setArchiveData(snippetKeyCombo, forKey: Constants.HotKey.snippetKeyCombo)

        let service = HotKeyService()
        #expect(service.mainKeyCombo == nil)
        #expect(service.historyKeyCombo == nil)
        #expect(service.snippetKeyCombo == nil)

        service.setupDefaultHotKeys()

        let savedMainKeyCombo = try #require(service.mainKeyCombo)
        #expect(savedMainKeyCombo.QWERTYKeyCode == 9)
        #expect(savedMainKeyCombo.modifiers == 768)
        #expect(savedMainKeyCombo.doubledModifiers == false)
        #expect(savedMainKeyCombo.keyEquivalent.uppercased() == "V")

        let savedHistoryKeyCombo = try #require(service.historyKeyCombo)
        #expect(savedHistoryKeyCombo.QWERTYKeyCode == 0)
        #expect(savedHistoryKeyCombo.modifiers == cmdKey)
        #expect(savedHistoryKeyCombo.doubledModifiers == true)
        #expect(savedHistoryKeyCombo.keyEquivalent.uppercased() == "")

        let savedSnippetKeyCombo = try #require(service.snippetKeyCombo)
        #expect(savedSnippetKeyCombo.QWERTYKeyCode == 0)
        #expect(savedSnippetKeyCombo.modifiers == shiftKey)
        #expect(savedSnippetKeyCombo.doubledModifiers == false)
        #expect(savedSnippetKeyCombo.keyEquivalent.uppercased() == "A")
    }

    @Test
    func defaultKeyCombos() {
        let keyCombos = HotKeyService.defaultKeyCombos
        let mainCombos = keyCombos[Constants.Menu.clip] as? [String: Int]
        let historyCombos = keyCombos[Constants.Menu.history] as? [String: Int]
        let snippetCombos = keyCombos[Constants.Menu.snippet] as? [String: Int]

        #expect(mainCombos?["keyCode"] == 9)
        #expect(mainCombos?["modifiers"] == 768)

        #expect(historyCombos?["keyCode"] == 9)
        #expect(historyCombos?["modifiers"] == 4352)

        #expect(snippetCombos?["keyCode"] == 11)
        #expect(snippetCombos?["modifiers"] == 768)
    }

    @Test
    func addAndRemoveClearHistoryHotkey() throws {
        let service = HotKeyService()

        #expect(service.clearHistoryKeyCombo == nil)

        let keyCombo = try #require(KeyCombo(QWERTYKeyCode: 10, carbonModifiers: cmdKey))
        service.changeClearHistoryKeyCombo(keyCombo)

        #expect(service.clearHistoryKeyCombo != nil)
        #expect(service.clearHistoryKeyCombo == keyCombo)

        let defaults = UserDefaults.standard
        let savedData = try #require(defaults.object(forKey: Constants.HotKey.clearHistoryKeyCombo) as? Data)
        let savedKeyCombo = try #require(NSKeyedUnarchiver.unarchiveObject(with: savedData) as? KeyCombo)
        #expect(savedKeyCombo == keyCombo)

        service.changeClearHistoryKeyCombo(nil)
        #expect(service.clearHistoryKeyCombo == nil)
    }
}

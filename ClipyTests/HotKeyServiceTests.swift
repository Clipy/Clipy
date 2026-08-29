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
    @Dependency(\.defaultAppStorage)
    var appStorage

    init() {
        CPYUtilities.registerUserDefaultKeys(appStorage)
    }

    @Test
    func migrateDefaultSettings() throws {
        let service = HotKeyService()
        #expect(service.mainKeyCombo == nil)
        #expect(service.historyKeyCombo == nil)
        #expect(service.snippetKeyCombo == nil)
        #expect(service.searchKeyCombo == nil)

        #expect(appStorage.bool(forKey: Constants.HotKey.migrateNewKeyCombo) == false)
        service.setupDefaultHotKeys()
        #expect(appStorage.bool(forKey: Constants.HotKey.migrateNewKeyCombo) == true)

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

        let searchKeyCombo = try #require(service.searchKeyCombo)
        #expect(searchKeyCombo.QWERTYKeyCode == 3)
        #expect(searchKeyCombo.modifiers == 768)
        #expect(searchKeyCombo.doubledModifiers == false)
        #expect(searchKeyCombo.keyEquivalent.uppercased() == "F")
    }

    @Test
    func migrateCustomizeSettings() throws {
        let service = HotKeyService()
        #expect(service.mainKeyCombo == nil)
        #expect(service.historyKeyCombo == nil)
        #expect(service.snippetKeyCombo == nil)
        #expect(service.searchKeyCombo == nil)

        let defaultKeyCombos: [String: Any] = [Constants.Menu.clip: ["keyCode": 0, "modifiers": 4352],
                                               Constants.Menu.history: ["keyCode": 9, "modifiers": 768],
                                               Constants.Menu.snippet: ["keyCode": 11, "modifiers": 4352],
                                               Constants.Menu.search: ["keyCode": 3, "modifiers": 768]]
        appStorage.register(defaults: [Constants.UserDefaults.hotKeys: defaultKeyCombos])

        #expect(appStorage.bool(forKey: Constants.HotKey.migrateNewKeyCombo) == false)
        service.setupDefaultHotKeys()
        #expect(appStorage.bool(forKey: Constants.HotKey.migrateNewKeyCombo) == true)

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
        appStorage.set(true, forKey: Constants.HotKey.migrateNewKeyCombo)

        let service = HotKeyService()
        #expect(service.mainKeyCombo == nil)
        #expect(service.historyKeyCombo == nil)
        #expect(service.snippetKeyCombo == nil)
        #expect(service.searchKeyCombo == nil)

        #expect(appStorage.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.mainKeyCombo) == nil)
        #expect(appStorage.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.historyKeyCombo) == nil)
        #expect(appStorage.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.snippetKeyCombo) == nil)
        #expect(appStorage.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.searchKeyCombo) == nil)

        service.setupDefaultHotKeys()
        #expect(service.mainKeyCombo == nil)
        #expect(service.historyKeyCombo == nil)
        #expect(service.snippetKeyCombo == nil)
        let seededSearchKeyCombo = try #require(service.searchKeyCombo)
        #expect(seededSearchKeyCombo.QWERTYKeyCode == 3)
        #expect(seededSearchKeyCombo.modifiers == 768)
        #expect(seededSearchKeyCombo.doubledModifiers == false)
        #expect(seededSearchKeyCombo.keyEquivalent.uppercased() == "F")

        let mainKeyCombo = try #require(KeyCombo(QWERTYKeyCode: 9, carbonModifiers: 768))
        let historyKeyCombo = try #require(KeyCombo(doubledCocoaModifiers: .command))
        let snippetKeyCombo = try #require(KeyCombo(QWERTYKeyCode: 0, cocoaModifiers: .shift))
        let searchKeyCombo = try #require(KeyCombo(QWERTYKeyCode: 3, carbonModifiers: 768))

        service.change(with: .main, keyCombo: mainKeyCombo)
        service.change(with: .history, keyCombo: historyKeyCombo)
        service.change(with: .snippet, keyCombo: snippetKeyCombo)
        service.change(with: .search, keyCombo: searchKeyCombo)

        let savedMainKeyCombo = try #require(appStorage.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.mainKeyCombo))
        let savedHistoryKeyCombo = try #require(appStorage.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.historyKeyCombo))
        let savedSnippetKeyCombo = try #require(appStorage.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.snippetKeyCombo))
        let savedSearchKeyCombo = try #require(appStorage.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.searchKeyCombo))

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

        #expect(savedSearchKeyCombo.QWERTYKeyCode == 3)
        #expect(savedSearchKeyCombo.modifiers == 768)
        #expect(savedSearchKeyCombo.doubledModifiers == false)
        #expect(savedSearchKeyCombo.keyEquivalent.uppercased() == "F")

        service.change(with: .main, keyCombo: nil)
        #expect(service.mainKeyCombo == nil)
        #expect(appStorage.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.mainKeyCombo) == nil)
    }

    @Test
    func unarchiveSavedKeyCombos() throws {
        appStorage.set(true, forKey: Constants.HotKey.migrateNewKeyCombo)

        let mainKeyCombo = try #require(KeyCombo(QWERTYKeyCode: 9, carbonModifiers: 768))
        let historyKeyCombo = try #require(KeyCombo(doubledCocoaModifiers: .command))
        let snippetKeyCombo = try #require(KeyCombo(QWERTYKeyCode: 0, cocoaModifiers: .shift))
        let searchKeyCombo = try #require(KeyCombo(QWERTYKeyCode: 3, carbonModifiers: 768))

        appStorage.setArchiveData(mainKeyCombo, forKey: Constants.HotKey.mainKeyCombo)
        appStorage.setArchiveData(historyKeyCombo, forKey: Constants.HotKey.historyKeyCombo)
        appStorage.setArchiveData(snippetKeyCombo, forKey: Constants.HotKey.snippetKeyCombo)
        appStorage.setArchiveData(searchKeyCombo, forKey: Constants.HotKey.searchKeyCombo)

        let service = HotKeyService()
        #expect(service.mainKeyCombo == nil)
        #expect(service.historyKeyCombo == nil)
        #expect(service.snippetKeyCombo == nil)
        #expect(service.searchKeyCombo == nil)

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

        let savedSearchKeyCombo = try #require(service.searchKeyCombo)
        #expect(savedSearchKeyCombo.QWERTYKeyCode == 3)
        #expect(savedSearchKeyCombo.modifiers == 768)
        #expect(savedSearchKeyCombo.doubledModifiers == false)
        #expect(savedSearchKeyCombo.keyEquivalent.uppercased() == "F")
    }

    @Test
    func defaultKeyCombos() {
        let keyCombos = HotKeyService.defaultKeyCombos
        let mainCombos = keyCombos[Constants.Menu.clip] as? [String: Int]
        let historyCombos = keyCombos[Constants.Menu.history] as? [String: Int]
        let snippetCombos = keyCombos[Constants.Menu.snippet] as? [String: Int]
        let searchCombos = keyCombos[Constants.Menu.search] as? [String: Int]

        #expect(mainCombos?["keyCode"] == 9)
        #expect(mainCombos?["modifiers"] == 768)

        #expect(historyCombos?["keyCode"] == 9)
        #expect(historyCombos?["modifiers"] == 4352)

        #expect(snippetCombos?["keyCode"] == 11)
        #expect(snippetCombos?["modifiers"] == 768)

        #expect(searchCombos?["keyCode"] == 3)
        #expect(searchCombos?["modifiers"] == 768)
    }

    @Test
    func addAndRemoveClearHistoryHotkey() throws {
        let service = HotKeyService()

        #expect(service.clearHistoryKeyCombo == nil)

        let keyCombo = try #require(KeyCombo(QWERTYKeyCode: 10, carbonModifiers: cmdKey))
        service.changeClearHistoryKeyCombo(keyCombo)

        #expect(service.clearHistoryKeyCombo != nil)
        #expect(service.clearHistoryKeyCombo == keyCombo)

        let savedData = try #require(appStorage.object(forKey: Constants.HotKey.clearHistoryKeyCombo) as? Data)
        let savedKeyCombo = try #require(NSKeyedUnarchiver.unarchiveObject(with: savedData) as? KeyCombo)
        #expect(savedKeyCombo == keyCombo)

        service.changeClearHistoryKeyCombo(nil)
        #expect(service.clearHistoryKeyCombo == nil)
    }
}

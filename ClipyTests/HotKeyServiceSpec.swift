import Quick
import Nimble
import Magnet
import Carbon
@testable import Clipy

// swiftlint:disable function_body_length

class HotKeyServiceSpec: QuickSpec {
    override func spec() {

        describe("Migrate HotKey") {

            beforeEach {
                let defaults = UserDefaults.standard
                defaults.removeObject(forKey: Constants.UserDefaults.hotKeys)
                defaults.removeObject(forKey: Constants.HotKey.migrateNewKeyCombo)
                defaults.removeObject(forKey: Constants.HotKey.mainKeyCombo)
                defaults.removeObject(forKey: Constants.HotKey.historyKeyCombo)
                defaults.removeObject(forKey: Constants.HotKey.snippetKeyCombo)
                defaults.synchronize()
            }

            it("Migrate default settings") {
                let service = HotKeyService()
                expect(service.mainKeyCombo).to(beNil())
                expect(service.historyKeyCombo).to(beNil())
                expect(service.snippetKeyCombo).to(beNil())

                let defaults = UserDefaults.standard

                expect(defaults.bool(forKey: Constants.HotKey.migrateNewKeyCombo)) === false
                service.setupDefaultHotKeys()
                expect(defaults.bool(forKey: Constants.HotKey.migrateNewKeyCombo)) === true

                expect(service.mainKeyCombo).toNot(beNil())
                expect(service.mainKeyCombo?.keyCode) === 9
                expect(service.mainKeyCombo?.modifiers) === 768
                expect(service.mainKeyCombo?.doubledModifiers) === false
                expect(service.mainKeyCombo?.characters) === "V"

                expect(service.historyKeyCombo).toNot(beNil())
                expect(service.historyKeyCombo?.keyCode) === 9
                expect(service.historyKeyCombo?.modifiers) === 4352
                expect(service.historyKeyCombo?.doubledModifiers) === false
                expect(service.historyKeyCombo?.characters) === "V"

                expect(service.snippetKeyCombo).toNot(beNil())
                expect(service.snippetKeyCombo?.keyCode) === 11
                expect(service.snippetKeyCombo?.modifiers) === 768
                expect(service.snippetKeyCombo?.doubledModifiers) === false
                expect(service.snippetKeyCombo?.characters) === "B"
            }

            it("Migrate customize settings") {
                let service = HotKeyService()
                expect(service.mainKeyCombo).to(beNil())
                expect(service.historyKeyCombo).to(beNil())
                expect(service.snippetKeyCombo).to(beNil())

                let defaults = UserDefaults.standard
                let defaultKeyCombos: [String: Any] = [Constants.Menu.clip: ["keyCode": 0, "modifiers": 4352],
                                                       Constants.Menu.history: ["keyCode": 9, "modifiers": 768],
                                                       Constants.Menu.snippet: ["keyCode": 11, "modifiers": 4352]]
                defaults.register(defaults: [Constants.UserDefaults.hotKeys: defaultKeyCombos])
                defaults.synchronize()

                expect(defaults.bool(forKey: Constants.HotKey.migrateNewKeyCombo)) === false
                service.setupDefaultHotKeys()
                expect(defaults.bool(forKey: Constants.HotKey.migrateNewKeyCombo)) === true

                expect(service.mainKeyCombo).toNot(beNil())
                expect(service.mainKeyCombo?.keyCode) === 0
                expect(service.mainKeyCombo?.modifiers) === 4352
                expect(service.mainKeyCombo?.doubledModifiers) === false
                expect(service.mainKeyCombo?.characters) === "A"

                expect(service.historyKeyCombo).toNot(beNil())
                expect(service.historyKeyCombo?.keyCode) === 9
                expect(service.historyKeyCombo?.modifiers) === 768
                expect(service.historyKeyCombo?.doubledModifiers) === false
                expect(service.historyKeyCombo?.characters) === "V"

                expect(service.snippetKeyCombo).toNot(beNil())
                expect(service.snippetKeyCombo?.keyCode) === 11
                expect(service.snippetKeyCombo?.modifiers) === 4352
                expect(service.snippetKeyCombo?.doubledModifiers) === false
                expect(service.snippetKeyCombo?.characters) === "B"
            }

            afterEach {
                let defaults = UserDefaults.standard
                defaults.removeObject(forKey: Constants.UserDefaults.hotKeys)
                defaults.removeObject(forKey: Constants.HotKey.migrateNewKeyCombo)
                defaults.removeObject(forKey: Constants.HotKey.mainKeyCombo)
                defaults.removeObject(forKey: Constants.HotKey.historyKeyCombo)
                defaults.removeObject(forKey: Constants.HotKey.snippetKeyCombo)
                defaults.synchronize()
            }
        }

        describe("Save HotKey") {

            beforeEach {
                let defaults = UserDefaults.standard
                defaults.set(true, forKey: Constants.HotKey.migrateNewKeyCombo)
                defaults.removeObject(forKey: Constants.HotKey.mainKeyCombo)
                defaults.removeObject(forKey: Constants.HotKey.historyKeyCombo)
                defaults.removeObject(forKey: Constants.HotKey.snippetKeyCombo)
                defaults.synchronize()
            }

            it("Save key combos") {
                let service = HotKeyService()
                expect(service.mainKeyCombo).to(beNil())
                expect(service.historyKeyCombo).to(beNil())
                expect(service.snippetKeyCombo).to(beNil())

                let defautls = UserDefaults.standard
                expect(defautls.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.mainKeyCombo)).to(beNil())
                expect(defautls.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.historyKeyCombo)).to(beNil())
                expect(defautls.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.snippetKeyCombo)).to(beNil())

                service.setupDefaultHotKeys()
                expect(service.mainKeyCombo).to(beNil())
                expect(service.historyKeyCombo).to(beNil())
                expect(service.snippetKeyCombo).to(beNil())

                let mainKeyCombo = KeyCombo(keyCode: 9, carbonModifiers: 768)
                let historyKeyCombo = KeyCombo(doubledCocoaModifiers: .command)
                let snippetKeyCombo = KeyCombo(keyCode: 0, cocoaModifiers: .shift)

                service.change(with: .main, keyCombo: mainKeyCombo)
                service.change(with: .history, keyCombo: historyKeyCombo)
                service.change(with: .snippet, keyCombo: snippetKeyCombo)

                let savedMainKeyCombo = defautls.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.mainKeyCombo)
                let savedHistoryKeyCombo = defautls.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.historyKeyCombo)
                let savedSnippetKeyCombo = defautls.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.snippetKeyCombo)

                expect(savedMainKeyCombo).toNot(beNil())
                expect(savedMainKeyCombo?.keyCode) === 9
                expect(savedMainKeyCombo?.modifiers) === 768
                expect(savedMainKeyCombo?.doubledModifiers) === false
                expect(savedMainKeyCombo?.characters) === "V"

                expect(savedHistoryKeyCombo).toNot(beNil())
                expect(savedHistoryKeyCombo?.keyCode) === 0
                expect(savedHistoryKeyCombo?.modifiers) === cmdKey
                expect(savedHistoryKeyCombo?.doubledModifiers) === true
                expect(savedHistoryKeyCombo?.characters) === ""

                expect(savedSnippetKeyCombo).toNot(beNil())
                expect(savedSnippetKeyCombo?.keyCode) === 0
                expect(savedSnippetKeyCombo?.modifiers) === shiftKey
                expect(savedSnippetKeyCombo?.doubledModifiers) === false
                expect(savedSnippetKeyCombo?.characters) === "A"

                service.change(with: .main, keyCombo: nil)
                expect(service.mainKeyCombo).to(beNil())
                expect(defautls.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.mainKeyCombo)).to(beNil())
            }

            it("Unarchive saved key combos") {
                let mainKeyCombo = KeyCombo(keyCode: 9, carbonModifiers: 768)
                let historyKeyCombo = KeyCombo(doubledCocoaModifiers: .command)
                let snippetKeyCombo = KeyCombo(keyCode: 0, cocoaModifiers: .shift)

                let defaults = UserDefaults.standard
                defaults.setArchiveData(mainKeyCombo!, forKey: Constants.HotKey.mainKeyCombo)
                defaults.setArchiveData(historyKeyCombo!, forKey: Constants.HotKey.historyKeyCombo)
                defaults.setArchiveData(snippetKeyCombo!, forKey: Constants.HotKey.snippetKeyCombo)

                let service = HotKeyService()
                expect(service.mainKeyCombo).to(beNil())
                expect(service.historyKeyCombo).to(beNil())
                expect(service.snippetKeyCombo).to(beNil())

                service.setupDefaultHotKeys()

                expect(service.mainKeyCombo).toNot(beNil())
                expect(service.mainKeyCombo?.keyCode) === 9
                expect(service.mainKeyCombo?.modifiers) === 768
                expect(service.mainKeyCombo?.doubledModifiers) === false
                expect(service.mainKeyCombo?.characters) === "V"

                expect(service.historyKeyCombo).toNot(beNil())
                expect(service.historyKeyCombo?.keyCode) === 0
                expect(service.historyKeyCombo?.modifiers) === cmdKey
                expect(service.historyKeyCombo?.doubledModifiers) === true
                expect(service.historyKeyCombo?.characters) === ""

                expect(service.snippetKeyCombo).toNot(beNil())
                expect(service.snippetKeyCombo?.keyCode) === 0
                expect(service.snippetKeyCombo?.modifiers) === shiftKey
                expect(service.snippetKeyCombo?.doubledModifiers) === false
                expect(service.snippetKeyCombo?.characters) === "A"
            }

            afterEach {
                let defaults = UserDefaults.standard
                defaults.removeObject(forKey: Constants.UserDefaults.hotKeys)
                defaults.removeObject(forKey: Constants.HotKey.migrateNewKeyCombo)
                defaults.removeObject(forKey: Constants.HotKey.mainKeyCombo)
                defaults.removeObject(forKey: Constants.HotKey.historyKeyCombo)
                defaults.removeObject(forKey: Constants.HotKey.snippetKeyCombo)
                defaults.synchronize()
            }
        }

        describe("Key comobos") {
            it("Default key combos") {
                let keyCombos = HotKeyService.defaultKeyCombos
                let mainCombos = keyCombos[Constants.Menu.clip] as? [String: Int]
                let historyCombos = keyCombos[Constants.Menu.history] as? [String: Int]
                let snippetCombos = keyCombos[Constants.Menu.snippet] as? [String: Int]

                expect(mainCombos?["keyCode"]) === 9
                expect(mainCombos?["modifiers"]) === 768

                expect(historyCombos?["keyCode"]) === 9
                expect(historyCombos?["modifiers"]) === 4352

                expect(snippetCombos?["keyCode"]) === 11
                expect(snippetCombos?["modifiers"]) === 768
            }
        }

        describe("Clear History HotKey") {
            beforeEach {
                let defaults = UserDefaults.standard
                defaults.removeObject(forKey: Constants.HotKey.clearHistoryKeyCombo)
                defaults.synchronize()
            }

            it("Add and remove clear history hokey") {
                let service = HotKeyService()

                expect(service.clearHistoryKeyCombo).to(beNil())

                let keyCombo = KeyCombo(keyCode: 10, carbonModifiers: cmdKey)
                service.changeClearHistoryKeyCombo(keyCombo)

                expect(service.clearHistoryKeyCombo).toNot(beNil())
                expect(service.clearHistoryKeyCombo) === keyCombo

                let savedData = UserDefaults.standard.object(forKey: Constants.HotKey.clearHistoryKeyCombo) as? Data
                let savedKeyCombo = NSKeyedUnarchiver.unarchiveObject(with: savedData!) as? KeyCombo
                expect(savedKeyCombo) === keyCombo

                service.changeClearHistoryKeyCombo(nil)
                expect(service.clearHistoryKeyCombo).to(beNil())
            }

            afterEach {
                let defaults = UserDefaults.standard
                defaults.removeObject(forKey: Constants.HotKey.clearHistoryKeyCombo)
                defaults.synchronize()
            }
        }

        describe("Folder HotKey") {
            beforeEach {
                let defaults = UserDefaults.standard
                defaults.removeObject(forKey: Constants.HotKey.folderKeyCombos)
                defaults.synchronize()
            }

            it("Add and Remove folder hotkey") {
                let service = HotKeyService()

                let identifier = NSUUID().uuidString
                expect(service.snippetKeyCombo(forIdentifier: identifier)).to(beNil())

                let keyCombo = KeyCombo(keyCode: 0, carbonModifiers: cmdKey)!
                service.registerSnippetHotKey(with: identifier, keyCombo: keyCombo)

                expect(service.snippetKeyCombo(forIdentifier: identifier)).toNot(beNil())
                expect(service.snippetKeyCombo(forIdentifier: identifier)) === keyCombo

                let changeKeyCombo = KeyCombo(doubledCarbonModifiers: shiftKey)!
                service.registerSnippetHotKey(with: identifier, keyCombo: changeKeyCombo)

                expect(service.snippetKeyCombo(forIdentifier: identifier)) != keyCombo
                expect(service.snippetKeyCombo(forIdentifier: identifier)) === changeKeyCombo

                service.unregisterSnippetHotKey(with: identifier)
                expect(service.snippetKeyCombo(forIdentifier: identifier)).to(beNil())
            }

            afterEach {
                let defaults = UserDefaults.standard
                defaults.removeObject(forKey: Constants.HotKey.folderKeyCombos)
                defaults.synchronize()
            }
        }
    }
}

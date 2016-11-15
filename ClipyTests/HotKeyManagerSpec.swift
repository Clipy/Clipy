import Quick
import Nimble
import Magnet
import Carbon
@testable import Clipy

class HotKeyManagerSpec: QuickSpec {
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
                let manager = HotKeyManager()
                expect(manager.mainKeyCombo).to(beNil())
                expect(manager.historyKeyCombo).to(beNil())
                expect(manager.snippetKeyCombo).to(beNil())

                let defaults = UserDefaults.standard

                expect(defaults.bool(forKey: Constants.HotKey.migrateNewKeyCombo)).to(beFalse())
                manager.setupDefaultHoyKey()
                expect(defaults.bool(forKey: Constants.HotKey.migrateNewKeyCombo)).to(beTrue())

                expect(manager.mainKeyCombo).toNot(beNil())
                expect(manager.mainKeyCombo?.keyCode).to(equal(9))
                expect(manager.mainKeyCombo?.modifiers).to(equal(768))
                expect(manager.mainKeyCombo?.doubledModifiers).to(beFalse())
                expect(manager.mainKeyCombo?.characters).to(equal("V"))

                expect(manager.historyKeyCombo).toNot(beNil())
                expect(manager.historyKeyCombo?.keyCode).to(equal(9))
                expect(manager.historyKeyCombo?.modifiers).to(equal(4352))
                expect(manager.historyKeyCombo?.doubledModifiers).to(beFalse())
                expect(manager.historyKeyCombo?.characters).to(equal("V"))

                expect(manager.snippetKeyCombo).toNot(beNil())
                expect(manager.snippetKeyCombo?.keyCode).to(equal(11))
                expect(manager.snippetKeyCombo?.modifiers).to(equal(768))
                expect(manager.snippetKeyCombo?.doubledModifiers).to(beFalse())
                expect(manager.snippetKeyCombo?.characters).to(equal("B"))
            }

            it("Migrate customize settings") {
                let manager = HotKeyManager()
                expect(manager.mainKeyCombo).to(beNil())
                expect(manager.historyKeyCombo).to(beNil())
                expect(manager.snippetKeyCombo).to(beNil())

                let defaults = UserDefaults.standard
                let defaultKeyCombos: [String: Any] = [Constants.Menu.clip: ["keyCode": 0, "modifiers": 4352],
                                                       Constants.Menu.history: ["keyCode": 9, "modifiers": 768],
                                                       Constants.Menu.snippet: ["keyCode": 11, "modifiers": 4352]]
                defaults.register(defaults: [Constants.UserDefaults.hotKeys: defaultKeyCombos])
                defaults.synchronize()

                expect(defaults.bool(forKey: Constants.HotKey.migrateNewKeyCombo)).to(beFalse())
                manager.setupDefaultHoyKey()
                expect(defaults.bool(forKey: Constants.HotKey.migrateNewKeyCombo)).to(beTrue())

                expect(manager.mainKeyCombo).toNot(beNil())
                expect(manager.mainKeyCombo?.keyCode).to(equal(0))
                expect(manager.mainKeyCombo?.modifiers).to(equal(4352))
                expect(manager.mainKeyCombo?.doubledModifiers).to(beFalse())
                expect(manager.mainKeyCombo?.characters).to(equal("A"))

                expect(manager.historyKeyCombo).toNot(beNil())
                expect(manager.historyKeyCombo?.keyCode).to(equal(9))
                expect(manager.historyKeyCombo?.modifiers).to(equal(768))
                expect(manager.historyKeyCombo?.doubledModifiers).to(beFalse())
                expect(manager.historyKeyCombo?.characters).to(equal("V"))

                expect(manager.snippetKeyCombo).toNot(beNil())
                expect(manager.snippetKeyCombo?.keyCode).to(equal(11))
                expect(manager.snippetKeyCombo?.modifiers).to(equal(4352))
                expect(manager.snippetKeyCombo?.doubledModifiers).to(beFalse())
                expect(manager.snippetKeyCombo?.characters).to(equal("B"))
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
                let manager = HotKeyManager()
                expect(manager.mainKeyCombo).to(beNil())
                expect(manager.historyKeyCombo).to(beNil())
                expect(manager.snippetKeyCombo).to(beNil())

                let defautls = UserDefaults.standard
                expect(defautls.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.mainKeyCombo)).to(beNil())
                expect(defautls.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.historyKeyCombo)).to(beNil())
                expect(defautls.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.snippetKeyCombo)).to(beNil())

                manager.setupDefaultHoyKey()
                expect(manager.mainKeyCombo).to(beNil())
                expect(manager.historyKeyCombo).to(beNil())
                expect(manager.snippetKeyCombo).to(beNil())

                let mainKeyCombo = KeyCombo(keyCode: 9, carbonModifiers: 768)
                let historyKeyCombo = KeyCombo(doubledCocoaModifiers: .command)
                let snippetKeyCombo = KeyCombo(keyCode: 0, cocoaModifiers: .shift)

                manager.changeKeyCombo(.Main, keyCombo: mainKeyCombo)
                manager.changeKeyCombo(.History, keyCombo: historyKeyCombo)
                manager.changeKeyCombo(.Snippet, keyCombo: snippetKeyCombo)

                let savedMainKeyCombo = defautls.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.mainKeyCombo)
                let savedHistoryKeyCombo = defautls.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.historyKeyCombo)
                let savedSnippetKeyCombo = defautls.archiveDataForKey(KeyCombo.self, key: Constants.HotKey.snippetKeyCombo)

                expect(savedMainKeyCombo).toNot(beNil())
                expect(savedMainKeyCombo?.keyCode).to(equal(9))
                expect(savedMainKeyCombo?.modifiers).to(equal(768))
                expect(savedMainKeyCombo?.doubledModifiers).to(beFalse())
                expect(savedMainKeyCombo?.characters).to(equal("V"))

                expect(savedHistoryKeyCombo).toNot(beNil())
                expect(savedHistoryKeyCombo?.keyCode).to(equal(0))
                expect(savedHistoryKeyCombo?.modifiers).to(equal(cmdKey))
                expect(savedHistoryKeyCombo?.doubledModifiers).to(beTrue())
                expect(savedHistoryKeyCombo?.characters).to(equal(""))

                expect(savedSnippetKeyCombo).toNot(beNil())
                expect(savedSnippetKeyCombo?.keyCode).to(equal(0))
                expect(savedSnippetKeyCombo?.modifiers).to(equal(shiftKey))
                expect(savedSnippetKeyCombo?.doubledModifiers).to(beFalse())
                expect(savedSnippetKeyCombo?.characters).to(equal("A"))

                manager.changeKeyCombo(.Main, keyCombo: nil)
                expect(manager.mainKeyCombo).to(beNil())
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

                let manager = HotKeyManager()
                expect(manager.mainKeyCombo).to(beNil())
                expect(manager.historyKeyCombo).to(beNil())
                expect(manager.snippetKeyCombo).to(beNil())

                manager.setupDefaultHoyKey()

                expect(manager.mainKeyCombo).toNot(beNil())
                expect(manager.mainKeyCombo?.keyCode).to(equal(9))
                expect(manager.mainKeyCombo?.modifiers).to(equal(768))
                expect(manager.mainKeyCombo?.doubledModifiers).to(beFalse())
                expect(manager.mainKeyCombo?.characters).to(equal("V"))

                expect(manager.historyKeyCombo).toNot(beNil())
                expect(manager.historyKeyCombo?.keyCode).to(equal(0))
                expect(manager.historyKeyCombo?.modifiers).to(equal(cmdKey))
                expect(manager.historyKeyCombo?.doubledModifiers).to(beTrue())
                expect(manager.historyKeyCombo?.characters).to(equal(""))

                expect(manager.snippetKeyCombo).toNot(beNil())
                expect(manager.snippetKeyCombo?.keyCode).to(equal(0))
                expect(manager.snippetKeyCombo?.modifiers).to(equal(shiftKey))
                expect(manager.snippetKeyCombo?.doubledModifiers).to(beFalse())
                expect(manager.snippetKeyCombo?.characters).to(equal("A"))
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
                let keyCombos = HotKeyManager.defaultHotKeyCombos()
                let mainCombos = keyCombos[Constants.Menu.clip] as? [String: Int]
                let historyCombos = keyCombos[Constants.Menu.history] as? [String: Int]
                let snippetCombos = keyCombos[Constants.Menu.snippet] as? [String: Int]

                expect(mainCombos?["keyCode"]).to(equal(9))
                expect(mainCombos?["modifiers"]).to(equal(768))

                expect(historyCombos?["keyCode"]).to(equal(9))
                expect(historyCombos?["modifiers"]).to(equal(4352))

                expect(snippetCombos?["keyCode"]).to(equal(11))
                expect(snippetCombos?["modifiers"]).to(equal(768))
            }
        }

        describe("Folder HotKey") {
            beforeEach {
                let defaults = UserDefaults.standard
                defaults.removeObject(forKey: Constants.HotKey.folderKeyCombos)
                defaults.synchronize()
            }

            it("Add and Remove folder hotkey") {
                let manager = HotKeyManager()

                let identifier = NSUUID().uuidString
                expect(manager.folderKeyCombo(identifier)).to(beNil())

                let keyCombo = KeyCombo(keyCode: 0, carbonModifiers: cmdKey)!
                manager.addFolderHotKey(identifier, keyCombo: keyCombo)

                expect(manager.folderKeyCombo(identifier)).toNot(beNil())
                expect(manager.folderKeyCombo(identifier)).to(equal(keyCombo))

                let changeKeyCombo = KeyCombo(doubledCarbonModifiers: shiftKey)!
                manager.addFolderHotKey(identifier, keyCombo: changeKeyCombo)

                expect(manager.folderKeyCombo(identifier)).toNot(equal(keyCombo))
                expect(manager.folderKeyCombo(identifier)).to(equal(changeKeyCombo))

                manager.removeFolderHotKey(identifier)
                expect(manager.folderKeyCombo(identifier)).to(beNil())
            }

            afterEach {
                let defaults = UserDefaults.standard
                defaults.removeObject(forKey: Constants.HotKey.folderKeyCombos)
                defaults.synchronize()
            }
        }
    }
}

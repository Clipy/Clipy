import Quick
import Nimble
import Realm
@testable import Clipy

class FolderSpec: QuickSpec {
    override func spec() {

        beforeEach {
            let config = RLMRealmConfiguration.defaultConfiguration()
            config.inMemoryIdentifier = NSUUID().UUIDString
            RLMRealmConfiguration.setDefaultConfiguration(config)
        }

        describe("Create new") {

            it("deep copy object") {
                // Save Value
                let savedFolder = CPYFolder()
                savedFolder.index = 100
                savedFolder.title = "saved realm folder"

                let savedSnippet = CPYSnippet()
                savedSnippet.index = 10
                savedSnippet.title = "saved realm snippet"
                savedSnippet.content = "content"
                savedFolder.snippets.addObject(savedSnippet)

                let realm = RLMRealm.defaultRealm()
                realm.transaction { realm.addObject(savedFolder) }

                // Saved in Realm
                expect(savedFolder.realm).toNot(beNil())
                expect(savedSnippet.realm).toNot(beNil())

                // Deep copy
                let folder = savedFolder.deepCopy()
                expect(folder.realm).to(beNil())
                expect(folder.index).to(equal(savedFolder.index))
                expect(folder.enable).to(equal(savedFolder.enable))
                expect(folder.title).to(equal(savedFolder.title))
                expect(folder.identifier).to(equal(savedFolder.identifier))
                expect(folder.snippets.count).to(equal(1))

                let snippet = folder.snippets.firstObject() as! CPYSnippet
                expect(snippet.realm).to(beNil())
                expect(snippet.index).to(equal(savedSnippet.index))
                expect(snippet.enable).to(equal(savedSnippet.enable))
                expect(snippet.title).to(equal(savedSnippet.title))
                expect(snippet.content).to(equal(savedSnippet.content))
                expect(snippet.identifier).to(equal(savedSnippet.identifier))
            }

            it("Create folder") {
                let folder = CPYFolder.create()
                expect(folder.title).to(equal("untitled folder"))
                expect(folder.index).to(equal(0))

                let realm = RLMRealm.defaultRealm()
                realm.transaction { realm.addObject(folder) }

                let folder2 = CPYFolder.create()
                expect(folder2.index).to(equal(1))
            }

            it("Create snippet") {
                let folder = CPYFolder()
                let snippet = folder.createSnippet()

                expect(snippet.title).to(equal("untitled snippet"))
                expect(snippet.index).to(equal(0))

                folder.snippets.addObject(snippet)

                let snippet2 = folder.createSnippet()
                expect(snippet2.index).to(equal(1))
            }

            afterEach {
                let realm = RLMRealm.defaultRealm()
                realm.transaction { realm.deleteAllObjects() }
            }

        }

        describe("Sync database") {

            it("Merge snippet") {
                let folder = CPYFolder()
                let realm = RLMRealm.defaultRealm()
                realm.transaction { realm.addObject(folder) }
                let copyFolder = folder.deepCopy()

                let snippet = CPYSnippet()
                let snippet2 = CPYSnippet()
                copyFolder.mergeSnippet(snippet)
                copyFolder.mergeSnippet(snippet2)

                expect(snippet.realm).to(beNil())
                expect(snippet2.realm).to(beNil())
                expect(folder.snippets.count).to(equal(2))

                let savedSnippet = folder.snippets.firstObject() as! CPYSnippet
                let savedSnippet2 = folder.snippets.objectAtIndex(1) as! CPYSnippet
                expect(savedSnippet.identifier).to(equal(snippet.identifier))
                expect(savedSnippet2.identifier).to(equal(snippet2.identifier))
            }

            it("Insert snippet") {
                let folder = CPYFolder()
                let realm = RLMRealm.defaultRealm()
                realm.transaction { realm.addObject(folder) }
                let copyFolder = folder.deepCopy()

                let snippet = CPYSnippet()
                // Don't insert non saved snippt
                copyFolder.insertSnippet(snippet, index: 0)
                expect(folder.snippets.count).to(equal(0))

                realm.transaction { realm.addObject(snippet) }

                // Can insert saved snippet
                copyFolder.insertSnippet(snippet, index: 0)
                expect(folder.snippets.count).to(equal(1))
            }

            it("Remove snippet") {
                let folder = CPYFolder()
                let snippet = CPYSnippet()
                folder.snippets.addObject(snippet)
                let realm = RLMRealm.defaultRealm()
                realm.transaction { realm.addObject(folder) }

                expect(folder.snippets.count).to(equal(1))

                let copyFolder = folder.deepCopy()
                copyFolder.removeSnippet(snippet)

                expect(folder.snippets.count).to(equal(0))
            }

            it("Merge folder") {
                expect(CPYFolder.allObjects().count).to(equal(0))

                let folder = CPYFolder()
                folder.index = 100
                folder.title = "title"
                folder.enable = false
                folder.merge()
                expect(folder.realm).to(beNil())
                expect(CPYFolder.allObjects().count).to(equal(1))

                let savedFolder = CPYFolder(forPrimaryKey: folder.identifier)
                expect(savedFolder).toNot(beNil())
                expect(savedFolder?.index).to(equal(folder.index))
                expect(savedFolder?.title).to(equal(folder.title))
                expect(savedFolder?.enable).to(equal(folder.enable))

                folder.index = 1
                folder.title = "change title"
                folder.enable = true
                folder.merge()
                expect(CPYFolder.allObjects().count).to(equal(1))

                expect(savedFolder?.index).to(equal(folder.index))
                expect(savedFolder?.title).to(equal(folder.title))
                expect(savedFolder?.enable).to(equal(folder.enable))
            }

            it("Remove folder") {
                let folder = CPYFolder()
                let snippet = CPYSnippet()
                folder.snippets.addObject(snippet)
                let realm = RLMRealm.defaultRealm()
                realm.transaction { realm.addObject(folder) }

                expect(CPYFolder.allObjects().count).to(equal(1))
                expect(CPYSnippet.allObjects().count).to(equal(1))

                let copyFolder = folder.deepCopy()
                expect(copyFolder.realm).to(beNil())
                copyFolder.remove()

                expect(CPYFolder.allObjects().count).to(equal(0))
                expect(CPYSnippet.allObjects().count).to(equal(0))
            }

            afterEach {
                let realm = RLMRealm.defaultRealm()
                realm.transaction { realm.deleteAllObjects() }
            }

        }

        describe("Rearrange Index") {

            it("Rearrange folder index") {
                let folder = CPYFolder()
                folder.index = 100
                let folder2 = CPYFolder()
                folder2.index = 10

                let folders = [folder, folder2]
                let realm = RLMRealm.defaultRealm()
                realm.transaction { realm.addObjects(folders) }

                let copyFolder = folder.deepCopy()
                let copyFolder2 = folder2.deepCopy()

                CPYFolder.rearrangesIndex([copyFolder, copyFolder2])

                expect(copyFolder.index).to(equal(0))
                expect(copyFolder2.index).to(equal(1))
                expect(folder.index).to(equal(0))
                expect(folder2.index).to(equal(1))
            }

            it("Rearrange snippet index") {
                let folder = CPYFolder()
                let snippet = CPYSnippet()
                snippet.index = 10
                let snippet2 = CPYSnippet()
                snippet2.index = 100
                folder.snippets.addObject(snippet)
                folder.snippets.addObject(snippet2)
                let realm = RLMRealm.defaultRealm()
                realm.transaction { realm.addObject(folder) }

                let copyFolder = folder.deepCopy()
                copyFolder.rearrangesSnippetIndex()

                let copySnippet = copyFolder.snippets.firstObject() as! CPYSnippet
                let copySnippet2 = copyFolder.snippets.objectAtIndex(1) as! CPYSnippet
                expect(copySnippet.index).to(equal(0))
                expect(copySnippet2.index).to(equal(1))
                expect(snippet.index).to(equal(0))
                expect(snippet2.index).to(equal(1))
            }

            afterEach {
                let realm = RLMRealm.defaultRealm()
                realm.transaction { realm.deleteAllObjects() }
            }

        }

    }
}

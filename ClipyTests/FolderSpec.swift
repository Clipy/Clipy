import Quick
import Nimble
import RealmSwift
@testable import Clipy

class FolderSpec: QuickSpec {
    override func spec() {

        beforeEach {
            Realm.Configuration.defaultConfiguration.inMemoryIdentifier = NSUUID().uuidString
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
                savedFolder.snippets.append(savedSnippet)

                let realm = try! Realm()
                realm.transaction { realm.add(savedFolder) }

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

                let snippet = folder.snippets.first!
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

                let realm = try! Realm()
                realm.transaction { realm.add(folder) }

                let folder2 = CPYFolder.create()
                expect(folder2.index).to(equal(1))
            }

            it("Create snippet") {
                let folder = CPYFolder()
                let snippet = folder.createSnippet()

                expect(snippet.title).to(equal("untitled snippet"))
                expect(snippet.index).to(equal(0))

                folder.snippets.append(snippet)

                let snippet2 = folder.createSnippet()
                expect(snippet2.index).to(equal(1))
            }

            afterEach {
                let realm = try! Realm()
                realm.transaction { realm.deleteAll() }
            }

        }

        describe("Sync database") {

            it("Merge snippet") {
                let folder = CPYFolder()
                let realm = try! Realm()
                realm.transaction { realm.add(folder) }
                let copyFolder = folder.deepCopy()

                let snippet = CPYSnippet()
                let snippet2 = CPYSnippet()
                copyFolder.mergeSnippet(snippet)
                copyFolder.mergeSnippet(snippet2)

                expect(snippet.realm).to(beNil())
                expect(snippet2.realm).to(beNil())
                expect(folder.snippets.count).to(equal(2))

                let savedSnippet = folder.snippets.first!
                let savedSnippet2 = folder.snippets[1]
                expect(savedSnippet.identifier).to(equal(snippet.identifier))
                expect(savedSnippet2.identifier).to(equal(snippet2.identifier))
            }

            it("Insert snippet") {
                let folder = CPYFolder()
                let realm = try! Realm()
                realm.transaction { realm.add(folder) }
                let copyFolder = folder.deepCopy()

                let snippet = CPYSnippet()
                // Don't insert non saved snippt
                copyFolder.insertSnippet(snippet, index: 0)
                expect(folder.snippets.count).to(equal(0))

                realm.transaction { realm.add(snippet) }

                // Can insert saved snippet
                copyFolder.insertSnippet(snippet, index: 0)
                expect(folder.snippets.count).to(equal(1))
            }

            it("Remove snippet") {
                let folder = CPYFolder()
                let snippet = CPYSnippet()
                folder.snippets.append(snippet)
                let realm = try! Realm()
                realm.transaction { realm.add(folder) }

                expect(folder.snippets.count).to(equal(1))

                let copyFolder = folder.deepCopy()
                copyFolder.removeSnippet(snippet)

                expect(folder.snippets.count).to(equal(0))
            }

            it("Merge folder") {
                let realm = try! Realm()
                expect(realm.objects(CPYFolder.self).count).to(equal(0))

                let folder = CPYFolder()
                folder.index = 100
                folder.title = "title"
                folder.enable = false
                folder.merge()
                expect(folder.realm).to(beNil())
                expect(realm.objects(CPYFolder.self).count).to(equal(1))

                let savedFolder = realm.object(ofType: CPYFolder.self, forPrimaryKey: folder.identifier)
                expect(savedFolder).toNot(beNil())
                expect(savedFolder?.index).to(equal(folder.index))
                expect(savedFolder?.title).to(equal(folder.title))
                expect(savedFolder?.enable).to(equal(folder.enable))

                folder.index = 1
                folder.title = "change title"
                folder.enable = true
                folder.merge()
                expect(realm.objects(CPYFolder.self).count).to(equal(1))

                expect(savedFolder?.index).to(equal(folder.index))
                expect(savedFolder?.title).to(equal(folder.title))
                expect(savedFolder?.enable).to(equal(folder.enable))
            }

            it("Remove folder") {
                let folder = CPYFolder()
                let snippet = CPYSnippet()
                folder.snippets.append(snippet)
                let realm = try! Realm()
                realm.transaction { realm.add(folder) }

                expect(realm.objects(CPYFolder.self).count).to(equal(1))
                expect(realm.objects(CPYSnippet.self).count).to(equal(1))

                let copyFolder = folder.deepCopy()
                expect(copyFolder.realm).to(beNil())
                copyFolder.remove()

                expect(realm.objects(CPYFolder.self).count).to(equal(0))
                expect(realm.objects(CPYSnippet.self).count).to(equal(0))
            }

            afterEach {
                let realm = try! Realm()
                realm.transaction { realm.deleteAll() }
            }

        }

        describe("Rearrange Index") {

            it("Rearrange folder index") {
                let folder = CPYFolder()
                folder.index = 100
                let folder2 = CPYFolder()
                folder2.index = 10

                let folders = [folder, folder2]
                let realm = try! Realm()
                realm.transaction { realm.add(folders) }

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
                folder.snippets.append(snippet)
                folder.snippets.append(snippet2)
                let realm = try! Realm()
                realm.transaction { realm.add(folder) }

                let copyFolder = folder.deepCopy()
                copyFolder.rearrangesSnippetIndex()

                let copySnippet = copyFolder.snippets.first!
                let copySnippet2 = copyFolder.snippets[1]
                expect(copySnippet.index).to(equal(0))
                expect(copySnippet2.index).to(equal(1))
                expect(snippet.index).to(equal(0))
                expect(snippet2.index).to(equal(1))
            }

            afterEach {
                let realm = try! Realm()
                realm.transaction { realm.deleteAll() }
            }

        }

    }
}

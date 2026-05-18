import Foundation
import RealmSwift
import Testing
@testable import Clipy

@MainActor @Suite(.serialized)
final class FolderTests {
    init() {
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = UUID().uuidString
    }

    deinit {
        let realm = try! Realm()
        realm.transaction { realm.deleteAll() }
    }

    @Test
    func deepCopyObject() throws {
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

        #expect(savedFolder.realm != nil)
        #expect(savedSnippet.realm != nil)

        let folder = savedFolder.deepCopy()
        #expect(folder.realm == nil)
        #expect(folder.index == savedFolder.index)
        #expect(folder.enable == savedFolder.enable)
        #expect(folder.title == savedFolder.title)
        #expect(folder.identifier == savedFolder.identifier)
        #expect(folder.snippets.count == 1)

        let snippet = try #require(folder.snippets.first)
        #expect(snippet.realm == nil)
        #expect(snippet.index == savedSnippet.index)
        #expect(snippet.enable == savedSnippet.enable)
        #expect(snippet.title == savedSnippet.title)
        #expect(snippet.content == savedSnippet.content)
        #expect(snippet.identifier == savedSnippet.identifier)
    }

    @Test
    func createFolder() {
        let folder = CPYFolder.create()
        #expect(folder.title == "untitled folder")
        #expect(folder.index == 0)

        let realm = try! Realm()
        realm.transaction { realm.add(folder) }

        let folder2 = CPYFolder.create()
        #expect(folder2.index == 1)
    }

    @Test
    func createSnippet() {
        let folder = CPYFolder()
        let snippet = folder.createSnippet()

        #expect(snippet.title == "untitled snippet")
        #expect(snippet.index == 0)

        folder.snippets.append(snippet)

        let snippet2 = folder.createSnippet()
        #expect(snippet2.index == 1)
    }

    @Test
    func mergeSnippet() throws {
        let folder = CPYFolder()
        let realm = try! Realm()
        realm.transaction { realm.add(folder) }
        let copyFolder = folder.deepCopy()

        let snippet = CPYSnippet()
        let snippet2 = CPYSnippet()
        copyFolder.mergeSnippet(snippet)
        copyFolder.mergeSnippet(snippet2)

        #expect(snippet.realm == nil)
        #expect(snippet2.realm == nil)
        #expect(folder.snippets.count == 2)

        let savedSnippet = try #require(folder.snippets.first)
        let savedSnippet2 = folder.snippets[1]
        #expect(savedSnippet.identifier == snippet.identifier)
        #expect(savedSnippet2.identifier == snippet2.identifier)
    }

    @Test
    func insertSnippet() {
        let folder = CPYFolder()
        let realm = try! Realm()
        realm.transaction { realm.add(folder) }
        let copyFolder = folder.deepCopy()

        let snippet = CPYSnippet()
        copyFolder.insertSnippet(snippet, index: 0)
        #expect(folder.snippets.isEmpty)

        realm.transaction { realm.add(snippet) }

        copyFolder.insertSnippet(snippet, index: 0)
        #expect(folder.snippets.count == 1)
    }

    @Test
    func removeSnippet() {
        let folder = CPYFolder()
        let snippet = CPYSnippet()
        folder.snippets.append(snippet)
        let realm = try! Realm()
        realm.transaction { realm.add(folder) }

        #expect(folder.snippets.count == 1)

        let copyFolder = folder.deepCopy()
        copyFolder.removeSnippet(snippet)

        #expect(folder.snippets.isEmpty)
    }

    @Test
    func mergeFolder() throws {
        let realm = try! Realm()
        #expect(realm.objects(CPYFolder.self).isEmpty)

        let folder = CPYFolder()
        folder.index = 100
        folder.title = "title"
        folder.enable = false
        folder.merge()
        #expect(folder.realm == nil)
        #expect(realm.objects(CPYFolder.self).count == 1)

        let savedFolder = try #require(realm.object(ofType: CPYFolder.self, forPrimaryKey: folder.identifier))
        #expect(savedFolder.index == folder.index)
        #expect(savedFolder.title == folder.title)
        #expect(savedFolder.enable == folder.enable)

        folder.index = 1
        folder.title = "change title"
        folder.enable = true
        folder.merge()
        #expect(realm.objects(CPYFolder.self).count == 1)

        #expect(savedFolder.index == folder.index)
        #expect(savedFolder.title == folder.title)
        #expect(savedFolder.enable == folder.enable)
    }

    @Test
    func removeFolder() {
        let folder = CPYFolder()
        let snippet = CPYSnippet()
        folder.snippets.append(snippet)
        let realm = try! Realm()
        realm.transaction { realm.add(folder) }

        #expect(realm.objects(CPYFolder.self).count == 1)
        #expect(realm.objects(CPYSnippet.self).count == 1)

        let copyFolder = folder.deepCopy()
        #expect(copyFolder.realm == nil)
        copyFolder.remove()

        #expect(realm.objects(CPYFolder.self).isEmpty)
        #expect(realm.objects(CPYSnippet.self).isEmpty)
    }

    @Test
    func rearrangeFolderIndex() {
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

        #expect(copyFolder.index == 0)
        #expect(copyFolder2.index == 1)
        #expect(folder.index == 0)
        #expect(folder2.index == 1)
    }

    @Test
    func rearrangeSnippetIndex() throws {
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

        let copySnippet = try #require(copyFolder.snippets.first)
        let copySnippet2 = copyFolder.snippets[1]
        #expect(copySnippet.index == 0)
        #expect(copySnippet2.index == 1)
        #expect(snippet.index == 0)
        #expect(snippet2.index == 1)
    }
}

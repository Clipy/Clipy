import Foundation
import RealmSwift
import Testing
@testable import Clipy

@MainActor @Suite(.serialized)
final class SnippetTests {
    init() {
        Realm.Configuration.defaultConfiguration.inMemoryIdentifier = UUID().uuidString
    }

    deinit {
        let realm = try! Realm()
        realm.transaction { realm.deleteAll() }
    }

    @Test
    func mergeSnippet() {
        let snippet = CPYSnippet()
        let realm = try! Realm()
        realm.transaction { realm.add(snippet) }

        let snippet2 = CPYSnippet()
        snippet2.identifier = snippet.identifier
        snippet2.index = 100
        snippet2.title = "title"
        snippet2.content = "content"
        snippet2.merge()
        #expect(snippet2.realm == nil)

        #expect(snippet.index == snippet2.index)
        #expect(snippet.title == snippet2.title)
        #expect(snippet.content == snippet2.content)
    }

    @Test
    func removeSnippet() {
        let realm = try! Realm()
        #expect(realm.objects(CPYSnippet.self).isEmpty)

        let snippet = CPYSnippet()
        realm.transaction { realm.add(snippet) }

        #expect(realm.objects(CPYSnippet.self).count == 1)

        let snippet2 = CPYSnippet()
        snippet2.identifier = snippet.identifier
        snippet2.remove()

        #expect(realm.objects(CPYSnippet.self).isEmpty)
    }
}

import Quick
import Nimble
import RealmSwift
@testable import Clipy

class SnippetSpec: QuickSpec {
    override func spec() {

        beforeEach {
            Realm.Configuration.defaultConfiguration.inMemoryIdentifier = NSUUID().uuidString
        }

        describe("Sync database") {

            it("Merge snippet") {
                let snippet = CPYSnippet()
                let realm = try! Realm()
                realm.transaction { realm.add(snippet) }

                let snippet2 = CPYSnippet()
                snippet2.identifier = snippet.identifier
                snippet2.index = 100
                snippet2.title = "title"
                snippet2.content = "content"
                snippet2.merge()
                expect(snippet2.realm).to(beNil())

                expect(snippet.index) == snippet2.index
                expect(snippet.title) == snippet2.title
                expect(snippet.content) == snippet2.content
            }

            it("Remove snippet") {
                let realm = try! Realm()
                expect(realm.objects(CPYSnippet.self).count) == 0

                let snippet = CPYSnippet()
                realm.transaction { realm.add(snippet) }

                expect(realm.objects(CPYSnippet.self).count) == 1

                let snippet2 = CPYSnippet()
                snippet2.identifier = snippet.identifier
                snippet2.remove()

                expect(realm.objects(CPYSnippet.self).count) == 0
            }

            afterEach {
                let realm = try! Realm()
                realm.transaction { realm.deleteAll() }
            }

        }

    }
}

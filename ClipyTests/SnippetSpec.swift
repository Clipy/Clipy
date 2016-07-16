import Quick
import Nimble
import Realm
@testable import Clipy

class SnippetSpec: QuickSpec {
    override func spec() {

        beforeEach {
            let config = RLMRealmConfiguration.defaultConfiguration()
            config.inMemoryIdentifier = NSUUID().UUIDString
            RLMRealmConfiguration.setDefaultConfiguration(config)
        }

        describe("Sync database") { 

            it("Merge snippet") {
                let snippet = CPYSnippet()
                let realm = RLMRealm.defaultRealm()
                realm.transaction { realm.addObject(snippet) }

                let snippet2 = CPYSnippet()
                snippet2.identifier = snippet.identifier
                snippet2.index = 100
                snippet2.title = "title"
                snippet2.content = "content"
                snippet2.merge()
                expect(snippet2.realm).to(beNil())

                expect(snippet.index).to(equal(snippet2.index))
                expect(snippet.title).to(equal(snippet2.title))
                expect(snippet.content).to(equal(snippet2.content))
            }

            it("Remove snippet") {
                expect(CPYSnippet.allObjects().count).to(equal(0))

                let snippet = CPYSnippet()
                let realm = RLMRealm.defaultRealm()
                realm.transaction { realm.addObject(snippet) }

                expect(CPYSnippet.allObjects().count).to(equal(1))

                let snippet2 = CPYSnippet()
                snippet2.identifier = snippet.identifier
                snippet2.remove()

                expect(CPYSnippet.allObjects().count).to(equal(0))
            }

            afterEach {
                let realm = RLMRealm.defaultRealm()
                realm.transaction { realm.deleteAllObjects() }
            }

        }

    }
}

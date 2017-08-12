import Quick
import Nimble
import RealmSwift
import AEXML
@testable import Clipy

class ClipServiceSpec: QuickSpec {
    override func spec() {

        beforeEach {
            Realm.Configuration.defaultConfiguration.inMemoryIdentifier = NSUUID().uuidString
        }

        describe("Import and Export") {

            it("export clipboard") {
                let clipService = ClipService()
                let noItemXml = clipService.exportClipboard()
                expect(noItemXml[Constants.HistoryXml.rootElement].children.count).to(equal(0))

                (0...10).forEach { self.createClip(with: "test\($0)") }
                let exportXml = clipService.exportClipboard()
            }

            it("Import clipboard") {

            }

            afterEach {
                let realm = try! Realm()
                realm.transaction { realm.deleteAll() }
            }

        }
    }

    private func createClip(with string: String) {
        let data = CPYClipData(string: string)
        let unixTime = Int(Date().timeIntervalSince1970)
        let savedPath = CPYUtilities.applicationSupportFolder() + "/\(NSUUID().uuidString).data"

        let clip = CPYClip()
        clip.dataHash = savedPath
        clip.title = data.stringValue[0...10000]
        clip.dataHash = "\(data.hash)"
        clip.updateTime = unixTime
        clip.primaryType = data.primaryType ?? ""

        let realm = try! Realm()
        realm.transaction { realm.add(clip) }
    }
}

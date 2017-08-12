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

        describe("Export") {
            it("export clipboard") {
                let defaults = TestUserDefaults()
                defaults.set(30, forKey: Constants.UserDefaults.maxHistorySize)

                self.withEnvironment(defaults: defaults) {
                    let clipService = ClipService()
                    let noItemXml = clipService.exportClipboard()
                    expect(noItemXml[Constants.HistoryXml.rootElement].children.count).to(equal(0))

                    (0..<20).forEach { self.createClip(with: "test\($0)", index: $0) }
                    let exportXml = clipService.exportClipboard()
                    expect(exportXml[Constants.HistoryXml.rootElement].children.count).to(equal(20))
                    let firstValue = exportXml[Constants.HistoryXml.rootElement].children.first?[Constants.HistoryXml.contentElement].value
                    expect(firstValue).to(equal("test0"))
                }
            }

            it("trim max history size") {
                let defaults = TestUserDefaults()
                defaults.set(10, forKey: Constants.UserDefaults.maxHistorySize)

                self.withEnvironment(defaults: defaults) {
                    let clipService = ClipService()
                    let noItemXml = clipService.exportClipboard()
                    expect(noItemXml[Constants.HistoryXml.rootElement].children.count).to(equal(0))

                    (0..<20).forEach { self.createClip(with: "test\($0)", index: $0) }
                    let exportXml = clipService.exportClipboard()
                    expect(exportXml[Constants.HistoryXml.rootElement].children.count).to(equal(10))
                }
            }

            it("no export empty string") {
                let defaults = TestUserDefaults()
                defaults.set(30, forKey: Constants.UserDefaults.maxHistorySize)

                self.withEnvironment(defaults: defaults) {
                    let clipService = ClipService()
                    let noItemXml = clipService.exportClipboard()
                    expect(noItemXml[Constants.HistoryXml.rootElement].children.count).to(equal(0))

                    (0..<20).forEach { self.createClip(with: "test\($0)", index: $0) }
                    self.createClip(with: "")
                    let exportXml = clipService.exportClipboard()
                    expect(exportXml[Constants.HistoryXml.rootElement].children.count).to(equal(20))
                }
            }

            it("ascending") {
                let defaults = TestUserDefaults()
                defaults.set(30, forKey: Constants.UserDefaults.maxHistorySize)
                defaults.set(true, forKey: Constants.UserDefaults.reorderClipsAfterPasting)

                self.withEnvironment(defaults: defaults) {
                    let clipService = ClipService()
                    let noItemXml = clipService.exportClipboard()
                    expect(noItemXml[Constants.HistoryXml.rootElement].children.count).to(equal(0))

                    (0..<20).forEach { self.createClip(with: "test\($0)", index: $0) }
                    let exportXml = clipService.exportClipboard()
                    expect(exportXml[Constants.HistoryXml.rootElement].children.count).to(equal(20))
                    let firstValue = exportXml[Constants.HistoryXml.rootElement].children.first?[Constants.HistoryXml.contentElement].value
                    expect(firstValue).to(equal("test19"))
                }
            }
        }

        describe("Import") {
            it("Import clipboard") {
                let realm = try! Realm()
                let clips = realm.objects(CPYClip.self)
                expect(clips.count).to(equal(0))

                let clipService = ClipService()
                let xml = Fixture.Xml.exportHistories.xml
                clipService.importClipboard(with: xml)

                expect(clips.count).toEventually(equal(10))
            }
        }

        afterEach {
            let realm = try! Realm()
            realm.transaction { realm.deleteAll() }
        }
    }

    private func createClip(with string: String, index: Int = 0) {
        let data = CPYClipData(string: string)
        let unixTime = Int(Date().timeIntervalSince1970) + index
        let savedPath = CPYUtilities.applicationSupportFolder() + "/\(NSUUID().uuidString).data"

        let clip = CPYClip()
        clip.dataPath = savedPath
        clip.title = data.stringValue[0...10000]
        clip.dataHash = "\(data.hash)"
        clip.updateTime = unixTime
        clip.primaryType = data.primaryType ?? ""

        guard CPYUtilities.prepareSaveToPath(CPYUtilities.applicationSupportFolder()) else { return }
        guard NSKeyedArchiver.archiveRootObject(data, toFile: savedPath) else { return }
        let realm = try! Realm()
        realm.transaction { realm.add(clip) }
    }
}

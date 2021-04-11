import Quick
import Nimble
@testable import Clipy

final class ExcludeAppServiceSpec: QuickSpec {

    // MARK: - Properties
    private var pasteboard: NSPasteboard!

    // MARK: - Tests
    override func spec() {
        beforeEach {
            self.pasteboard = NSPasteboard(name: NSPasteboard.Name(UUID().uuidString))
        }

        describe("Special Application") {
            it("When Pasteboard and excluded applications are empty") {
                let service = ExcludeAppService(applications: [])
                expect(service.copiedProcessIsExcludedApplications(pasteboard: self.pasteboard)) == false
            }

            context("1Password") {
                it("When 1Password isn't excluded") {
                    let service = ExcludeAppService(applications: [])

                    self.pasteboard.declareTypes([.deprecatedString], owner: nil)
                    self.pasteboard.setString("string", forType: .deprecatedString)
                    expect(service.copiedProcessIsExcludedApplications(pasteboard: self.pasteboard)) == false

                    self.pasteboard.declareTypes([.deprecatedString,
                                                  NSPasteboard.PasteboardType("com.agilebits.onepassword")], owner: nil)
                    self.pasteboard.setString("string", forType: .deprecatedString)
                    expect(service.copiedProcessIsExcludedApplications(pasteboard: self.pasteboard)) == false
                }

                it("When 1Password 6 is set as an excluded application") {
                    let application1Password6 = CPYAppInfo(info: [kCFBundleIdentifierKey as String: "com.agilebits.onepassword-osx" as AnyObject,
                                                                  kCFBundleNameKey as String: "1Password" as AnyObject])!
                    let service = ExcludeAppService(applications: [application1Password6])
                    expect(service.copiedProcessIsExcludedApplications(pasteboard: self.pasteboard)) == false

                    self.pasteboard.declareTypes([.deprecatedString], owner: nil)
                    self.pasteboard.setString("string", forType: .deprecatedString)
                    expect(service.copiedProcessIsExcludedApplications(pasteboard: self.pasteboard)) == false

                    self.pasteboard.declareTypes([.deprecatedString,
                                                  NSPasteboard.PasteboardType("com.agilebits.onepassword")], owner: nil)
                    self.pasteboard.setString("string", forType: .deprecatedString)
                    expect(service.copiedProcessIsExcludedApplications(pasteboard: self.pasteboard)) == true
                }

                it("When 1Password 7 is set as an excluded application") {
                    let application1Password7 = CPYAppInfo(info: [kCFBundleIdentifierKey as String: "com.agilebits.onepassword7" as AnyObject,
                                                                  kCFBundleNameKey as String: "1Password" as AnyObject])!
                    let service = ExcludeAppService(applications: [application1Password7])
                    expect(service.copiedProcessIsExcludedApplications(pasteboard: self.pasteboard)) == false

                    self.pasteboard.declareTypes([.deprecatedString], owner: nil)
                    self.pasteboard.setString("string", forType: .deprecatedString)
                    expect(service.copiedProcessIsExcludedApplications(pasteboard: self.pasteboard)) == false

                    self.pasteboard.declareTypes([.deprecatedString,
                                                  NSPasteboard.PasteboardType("com.agilebits.onepassword")], owner: nil)
                    self.pasteboard.setString("string", forType: .deprecatedString)
                    expect(service.copiedProcessIsExcludedApplications(pasteboard: self.pasteboard)) == true
                }
            }
        }

    }

}

import Foundation
import XCTest

/// Default handler for Nimble. This assertion handler passes failures along to
/// XCTest.
public class NimbleXCTestHandler: AssertionHandler {
    public func assert(_ assertion: Bool, message: FailureMessage, location: SourceLocation) {
        if !assertion {
            recordFailure("\(message.stringValue)\n", location: location)
        }
    }
}

/// Alternative handler for Nimble. This assertion handler passes failures along
/// to XCTest by attempting to reduce the failure message size.
public class NimbleShortXCTestHandler: AssertionHandler {
    public func assert(_ assertion: Bool, message: FailureMessage, location: SourceLocation) {
        if !assertion {
            let msg: String
            if let actual = message.actualValue {
                msg = "got: \(actual) \(message.postfixActual)"
            } else {
                msg = "expected \(message.to) \(message.postfixMessage)"
            }
            recordFailure("\(msg)\n", location: location)
        }
    }
}

/// Fallback handler in case XCTest is unavailable. This assertion handler will abort
/// the program if it is invoked.
class NimbleXCTestUnavailableHandler: AssertionHandler {
    func assert(_ assertion: Bool, message: FailureMessage, location: SourceLocation) {
        fatalError("XCTest is not available and no custom assertion handler was configured. Aborting.")
    }
}

#if !SWIFT_PACKAGE
/// Helper class providing access to the currently executing XCTestCase instance, if any
@objc final internal class CurrentTestCaseTracker: NSObject, XCTestObservation {
    @objc static let sharedInstance = CurrentTestCaseTracker()

    private(set) var currentTestCase: XCTestCase?

    @objc func testCaseWillStart(_ testCase: XCTestCase) {
        currentTestCase = testCase
    }

    @objc func testCaseDidFinish(_ testCase: XCTestCase) {
        currentTestCase = nil
    }
}
#endif

func isXCTestAvailable() -> Bool {
#if _runtime(_ObjC)
    // XCTest is weakly linked and so may not be present
    return NSClassFromString("XCTestCase") != nil
#else
    return true
#endif
}

private func recordFailure(_ message: String, location: SourceLocation) {
#if SWIFT_PACKAGE
    XCTFail("\(message)", file: location.file, line: location.line)
#else
    if let testCase = CurrentTestCaseTracker.sharedInstance.currentTestCase {
        testCase.recordFailure(withDescription: message, inFile: location.file, atLine: location.line, expected: true)
    } else {
        let msg = "Attempted to report a test failure to XCTest while no test case was running. " +
        "The failure was:\n\"\(message)\"\nIt occurred at: \(location.file):\(location.line)"
        NSException(name: .internalInconsistencyException, reason: msg, userInfo: nil).raise()
    }
#endif
}

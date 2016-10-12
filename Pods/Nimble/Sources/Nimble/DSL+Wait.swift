import Foundation

#if _runtime(_ObjC)
private enum ErrorResult {
    case Exception(NSException)
    case Error(ErrorType)
    case None
}

/// Only classes, protocols, methods, properties, and subscript declarations can be
/// bridges to Objective-C via the @objc keyword. This class encapsulates callback-style
/// asynchronous waiting logic so that it may be called from Objective-C and Swift.
internal class NMBWait: NSObject {
    internal class func until(
        timeout timeout: NSTimeInterval,
        file: FileString = #file,
        line: UInt = #line,
        action: (() -> Void) -> Void) -> Void {
            return throwableUntil(timeout: timeout, file: file, line: line) { (done: () -> Void) throws -> Void in
                action() { done() }
            }
    }

    // Using a throwable closure makes this method not objc compatible.
    internal class func throwableUntil(
        timeout timeout: NSTimeInterval,
        file: FileString = #file,
        line: UInt = #line,
        action: (() -> Void) throws -> Void) -> Void {
            let awaiter = NimbleEnvironment.activeInstance.awaiter
            let leeway = timeout / 2.0
            let result = awaiter.performBlock { (done: (ErrorResult) -> Void) throws -> Void in
                dispatch_async(dispatch_get_main_queue()) {
                    let capture = NMBExceptionCapture(
                        handler: ({ exception in
                            done(.Exception(exception))
                        }),
                        finally: ({ })
                    )
                    capture.tryBlock {
                        do {
                            try action() {
                                done(.None)
                            }
                        } catch let e {
                            done(.Error(e))
                        }
                    }
                }
            }.timeout(timeout, forcefullyAbortTimeout: leeway).wait("waitUntil(...)", file: file, line: line)

            switch result {
            case .Incomplete: internalError("Reached .Incomplete state for waitUntil(...).")
            case .BlockedRunLoop:
                fail(blockedRunLoopErrorMessageFor("-waitUntil()", leeway: leeway),
                    file: file, line: line)
            case .TimedOut:
                let pluralize = (timeout == 1 ? "" : "s")
                fail("Waited more than \(timeout) second\(pluralize)", file: file, line: line)
            case let .RaisedException(exception):
                fail("Unexpected exception raised: \(exception)")
            case let .ErrorThrown(error):
                fail("Unexpected error thrown: \(error)")
            case .Completed(.Exception(let exception)):
                fail("Unexpected exception raised: \(exception)")
            case .Completed(.Error(let error)):
                fail("Unexpected error thrown: \(error)")
            case .Completed(.None): // success
                break
            }
    }

    @objc(untilFile:line:action:)
    internal class func until(file: FileString = #file, line: UInt = #line, action: (() -> Void) -> Void) -> Void {
        until(timeout: 1, file: file, line: line, action: action)
    }
}

internal func blockedRunLoopErrorMessageFor(fnName: String, leeway: NSTimeInterval) -> String {
    return "\(fnName) timed out but was unable to run the timeout handler because the main thread is unresponsive (\(leeway) seconds is allow after the wait times out). Conditions that may cause this include processing blocking IO on the main thread, calls to sleep(), deadlocks, and synchronous IPC. Nimble forcefully stopped run loop which may cause future failures in test run."
}

/// Wait asynchronously until the done closure is called or the timeout has been reached.
///
/// @discussion
/// Call the done() closure to indicate the waiting has completed.
/// 
/// This function manages the main run loop (`NSRunLoop.mainRunLoop()`) while this function
/// is executing. Any attempts to touch the run loop may cause non-deterministic behavior.
public func waitUntil(timeout timeout: NSTimeInterval = 1, file: FileString = #file, line: UInt = #line, action: (() -> Void) -> Void) -> Void {
    NMBWait.until(timeout: timeout, file: file, line: line, action: action)
}
#endif

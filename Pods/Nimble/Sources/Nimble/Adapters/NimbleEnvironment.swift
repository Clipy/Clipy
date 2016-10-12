import Foundation

/// "Global" state of Nimble is stored here. Only DSL functions should access / be aware of this
/// class' existance
internal class NimbleEnvironment {
    static var activeInstance: NimbleEnvironment {
        get {
            let env = NSThread.currentThread().threadDictionary["NimbleEnvironment"]
            if let env = env as? NimbleEnvironment {
                return env
            } else {
                let newEnv = NimbleEnvironment()
                self.activeInstance = newEnv
                return newEnv
            }
        }
        set {
            NSThread.currentThread().threadDictionary["NimbleEnvironment"] = newValue
        }
    }

    // TODO: eventually migrate the global to this environment value
    var assertionHandler: AssertionHandler {
        get { return NimbleAssertionHandler }
        set { NimbleAssertionHandler = newValue }
    }

#if _runtime(_ObjC)
    var awaiter: Awaiter

    init() {
        awaiter = Awaiter(
            waitLock: AssertionWaitLock(),
            asyncQueue: dispatch_get_main_queue(),
            timeoutQueue: dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_HIGH, 0))
    }
#endif
}

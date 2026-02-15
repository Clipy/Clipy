import ApplicationServices

let options = [kAXTrustedCheckOptionPrompt.takeUnretainedValue() as String: true] as CFDictionary
let trusted = AXIsProcessTrustedWithOptions(options)
print("AXIsProcessTrusted: \(trusted)")

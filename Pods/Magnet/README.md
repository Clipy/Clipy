# Magnet
[![Release version](https://img.shields.io/github/release/Clipy/Magnet.svg)](https://github.com/Clipy/Magnet/releases/latest)
[![License: MIT](https://img.shields.io/github/license/Clipy/Magnet.svg)](https://github.com/Clipy/Magnet/blob/master/LICENSE)
[![Carthage compatible](https://img.shields.io/badge/Carthage-compatible-4BC51D.svg?style=flat)](https://github.com/Carthage/Carthage)
[![Version](https://img.shields.io/cocoapods/v/Magnet.svg)](http://cocoadocs.org/docsets/Magnet)
[![Platform](https://img.shields.io/cocoapods/p/Magnet.svg)](http://cocoadocs.org/docsets/Magnet)

Customize global hotkeys in macOS. Supports usual hotkey and double tap hotkey like Alfred.app.

Also supports sandbox application.

## Requirements
- macOS 10.9+
- Xcode 9.4+
- Swift 4.1+

## Usage
### CocoaPods
```
pod 'Magnet'
```

### Carthage
```
github "Clipy/Magnet"
```

## Example
### Register Normal hotkey
Add `⌘ + Control + B` hotkey.

```swift
if let keyCombo = KeyCombo(keyCode: 11, carbonModifiers: 4352) {
   let hotKey = HotKey(identifier: "CommandControlB", keyCombo: keyCombo, target: self, action: #selector())
   hotKey.register() // or HotKeyCenter.shared.register(with: hotKey)
}
```

Or you can use closures.
```swift
if let keyCombo = KeyCombo(keyCode: 11, carbonModifiers: 4352) {
    let hotKey = HotKey(identifier: "CommandControlB", keyCombo: keyCombo) { hotKey in
        // Called when ⌘ + Control + B is pressed
    }
    hotKey.register()
}        
```

### Register Double tap hotkey
Add `⌘ double tap` hotkey.
```swift
if let keyCombo = KeyCombo(doubledCocoaModifiers: .command) {
   let hotKey = HotKey(identifier: "CommandDoubleTap", keyCombo: keyCombo, target: self, action: #selector())
   hotKey.register() // or HotKeyCenter.shared.register(with: hotKey)
}
```

Add `Control double tap` hotkey.
```swift
if let keyCombo = KeyCombo(doubledCarbonModifiers: controlKey) {
   let hotKey = HotKey(identifier: "ControlDoubleTap", keyCombo: keyCombo, target: self, action: #selector())
   hotKey.register() // or HotKeyCenter.shared.register(with: hotKey)
}
```

#### Support modifiers
Double tap hotkey only support following modifiers.
- Command Key
  - `NSEventModifierFlags.command` or `cmdKey`
- Shift Key
  - `NSEventModifierFlags.shift` or `shiftKey`
- Option Key
  - `NSEventModifierFlags.option` or `optionKey`
- Control Key
  - `NSEventModifierFlags.control` or `controlKey`

### Unregister hotkeys
```swift
HotKeyCenter.shared.unregisterAll()
```

or

```swift
HotKeyCenter.shared.unregister(with: "identifier")
```

or

```swift
let hotKey = HotKey(identifier: "identifier", keyCombo: KeyCombo, target: self, action: #selector())
hotKey.unregister() // or HotKeyCenter.shared.unregister(with: hotKey)
```

### Contributing
1. Fork it ( https://github.com/Clipy/Magnet/fork )
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create a new Pull Request

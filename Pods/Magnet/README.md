# Magnet
[![Release version](https://img.shields.io/github/release/Clipy/Magnet.svg)](https://github.com/Clipy/Magnet/releases/latest)
[![License: MIT](https://img.shields.io/github/license/Clipy/Magnet.svg)](https://github.com/Clipy/Magnet/blob/master/LICENSE)
[![Version](https://img.shields.io/cocoapods/v/Magnet.svg)](http://cocoadocs.org/docsets/Magnet)
[![Platform](https://img.shields.io/cocoapods/p/Magnet.svg)](http://cocoadocs.org/docsets/Magnet)

Customize global hotkeys in macOS. Supports usual hotkey and double tap hotkey like Alfred.app.

Also supports sandbox application.

## Usage
```
platform :osx, '10.9'
use_frameworks!

pod 'Magnet'
```

## Example
### Register Normal hotkey
Add `⌘ + Control + B` hotkey.

```
if let keyCombo = KeyCombo(keyCode: 11, carbonModifiers: 4352) {
   let hotKey = HotKey(identifier: "CommandControlB", keyCombo: keyCombo, target: self, action: #selector())
   hotKey.register() // or HotKeyCenter.sharedCenter.register(hotKey)
}
```

### Register Double tap hotkey
Add `⌘ double tap` hotkey.
```
if let keyCombo = KeyCombo(doubledCocoaModifiers: .CommandKeyMask) {
   let hotKey = HotKey(identifier: "CommandDoubleTap", keyCombo: keyCombo, target: self, action: #selector())
   hotKey.register() // or HotKeyCenter.sharedCenter.register(hotKey)
}
```

Add `Control double tap` hotkey.
```
if let keyCombo = KeyCombo(doubledCarbonModifiers: controlKey) {
   let hotKey = HotKey(identifier: "ControlDoubleTap", keyCombo: keyCombo, target: self, action: #selector())
   hotKey.register() // or HotKeyCenter.sharedCenter.register(hotKey)
}
```

#### Support modifiers
Double tap hotkey only support following modifiers.
- Command Key
 - `NSEventModifierFlags.CommandKeyMask` or `cmdKey`
- Shift Key
 - `NSEventModifierFlags.ShiftKeyMask` or `shiftKey`
- Option Key
 - `NSEventModifierFlags.AlternateKeyMask` or `optionKey`
- Control Key
 - `NSEventModifierFlags.ControlKeyMask` or `controlKey`

### Unregister hotkeys
```
HotKeyCenter.sharedCenter.unregisterAll()
```

or

```
HotKeyCenter.sharedCenter.unregisterHotKey("identifier")
```

or

```
let hotKey = HotKey(identifier: "identifier", keyCombo: KeyCombo, target: self, action: #selector())
hotKey.unregister() // or HotKeyCenter.sharedCenter.unregister(hotKey)
```

### Contributing
1. Fork it ( https://github.com/Clipy/Magnet/fork )
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create a new Pull Request

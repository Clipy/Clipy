# KeyHolder
[![Release version](https://img.shields.io/github/release/Clipy/KeyHolder.svg)](https://github.com/Clipy/KeyHolder/releases/latest)
[![License: MIT](https://img.shields.io/github/license/Clipy/KeyHolder.svg)](https://github.com/Clipy/KeyHolder/blob/master/LICENSE)
[![Carthage compatible](https://img.shields.io/badge/Carthage-compatible-4BC51D.svg?style=flat)](https://github.com/Carthage/Carthage)
[![Version](https://img.shields.io/cocoapods/v/KeyHolder.svg)](http://cocoadocs.org/docsets/KeyHolder)
[![Platform](https://img.shields.io/cocoapods/p/KeyHolder.svg)](http://cocoadocs.org/docsets/KeyHolder)

Record shortcuts in macOS, like Alfred App.

<img src="https://github.com/Clipy/KeyHolder/blob/master/Screenshots/double_tap_shortcut.png?raw=true" width="300">
<img src="https://github.com/Clipy/KeyHolder/blob/master/Screenshots/normal_shortcut.png?raw=true" width="300">

## Requirements
- macOS 10.10+
- Xcode 10.0+
- Swift 4.2+

## Usage
### CocoaPods
```
pod 'KeyHolder'
```

### Carthage
```
github "Clipy/KeyHolder"
github "Clipy/Magnet"
```

## Example
Set default key combo.
```swift
let recordView = RecordView(frame: CGRect.zero)
recordView.tintColor = NSColor(red: 0.164, green: 0.517, blue: 0.823, alpha: 1)
let keyCombo = KeyCombo(doubledModifiers: .command)
recordView.keyCombo = keyCombo
```

Some delegate methods
```swift
func recordViewShouldBeginRecording(_ recordView: RecordView) -> Bool
func recordView(_ recordView: RecordView, canRecordShortcut keyCombo: KeyCombo) -> Bool
func recordViewDidClearShortcut(_ recordView: RecordView)
func recordViewDidEndRecording(_ recordView: RecordView)
```

Or you can use closures.
```swift
let recordView = RecordView(frame: CGRect.zero)
recordView.didChange = { keyCombo in
    guard let keyCombo = keyCombo else { return } // Clear shortcut
    // Changed new shortcut
}
```

## Dependencies
The source code is dependent on hotkey library.
- [Magnet](https://github.com/Clipy/Magnet)

## How to Build
1. Move to the project root directory
2. Install dependency library with `carthage` or `git submodule`
3. `carthage checkout --use-submodules` or `git submodule init && git submodule update`
4. Open `KeyHolder.xcworkspace` on Xcode.
5. build.

### Contributing
1. Fork it ( https://github.com/Clipy/KeyHolder/fork )
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create a new Pull Request

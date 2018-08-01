# Sauce
[![Release version](https://img.shields.io/github/release/Clipy/Sauce.svg)](https://github.com/Clipy/Sauce/releases/latest)
[![License: MIT](https://img.shields.io/github/license/Clipy/Sauce.svg)](https://github.com/Clipy/Sauce/blob/master/LICENSE)
[![Carthage compatible](https://img.shields.io/badge/Carthage-compatible-4BC51D.svg?style=flat)](https://github.com/Carthage/Carthage)
[![Version](https://img.shields.io/cocoapods/v/Sauce.svg)](http://cocoadocs.org/docsets/Sauce)
[![Platform](https://img.shields.io/cocoapods/p/Sauce.svg)](http://cocoadocs.org/docsets/Sauce)

Mapping various keyboard layout sources and key codes in macOS. (e.g.: QWERTY, Dvorak)

## Requirements
- macOS 10.9+
- Xcode 9.0+
- Swift 4.0+

## Motivation
Only the ANSI-standard US keyboard is defined for the key code defined in Carbon.framework. Therefore, we can obtain only the key code of the QWERTY keyboard layout. (e.g.: `kVK_ANSI_V`)  
In layout other than QWERTY, (e.g. Dvorak) the virtual key code is different.

|  Keyboard Layout  |  Key  |  Key Code  | 
| :---------------: | :---: | :--------: |
|      QWERTY       |   v   |      9     |
|      Dvorak       |   v   |     47     |

This library is created with the purpose of mapping the key code of the input sources and making it possible to obtain the correct key code in various keyboard layouts.

## Usage
### CocoaPods
```
pod 'Sauce'
```

### Carthage
```
github "Clipy/Sauce"
```

## Example
### Key codes
Get the key code of the current input source

```swift
let keyCode = Sauce.shared.currentKeyCode(by: .v)
```

Get ASCII capable input source list

```swift
let sources = Sauce.shared.ASCIICapableInputSources()
```

Get key code corresponding to input source

```swift
let keyCode = Sauce.shared.keyCode(with: inputSource, key: .v)
```

### Notification
### `NSNotification.Name.SauceEnabledKeyboardInputSoucesChanged`
`SauceEnabledKeyboardInputSoucesChanged` is the same as `kTISNotifyEnabledKeyboardInputSourcesChanged` in Carbon.framework  

### `NSNotification.Name.SauceSelectedKeyboardInputSourceChanged`
`SauceSelectedKeyboardInputSourceChanged` is different from `kTISNotifySelectedKeyboardInputSourceChanged` and is notified only when the input source id has changed.  
Since it is filtered and notified, please do not use it for the same purpose as normal `kTISNotifySelectedKeyboardInputSourceChanged`.

### Contributing
1. Fork it ( https://github.com/Clipy/Sauce/fork )
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create a new Pull Request

# LoginServiceKit
[![Release version](https://img.shields.io/github/release/Clipy/LoginServiceKit.svg)]()
[![License: MIT](https://img.shields.io/github/license/Clipy/LoginServiceKit.svg)](https://github.com/Clipy/LoginServiceKit/blob/master/LICENSE)
[![Carthage compatible](https://img.shields.io/badge/Carthage-compatible-4BC51D.svg?style=flat)](https://github.com/Carthage/Carthage)

Easy manage login items in MacOSX

## Requirements
- macOS 10.9+
- Xcode 9.0+
- Swift 4.0+

## Usage
### CocoaPods
```
platform :osx, '10.9'
use_frameworks!

pod 'LoginServiceKit', :git => 'https://github.com/Clipy/LoginServiceKit.git'
```

### Carthage
```
github "Clipy/LoginServiceKit"
```

## Example
Add login item.

```swift
let appPath = NSBundle.main.bundlePath
LoginServiceKit.addLoginItems(at: appPath)
```

Remove login item.
```swift
let appPath = NSBundle.mainBundle().bundlePath
LoginServiceKit.removeLoginItems(at: appPath)
```

### Contributing
1. Fork it ( https://github.com/Clipy/LoginServiceKit/fork )
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create a new Pull Request

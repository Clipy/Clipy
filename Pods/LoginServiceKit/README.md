# LoginServiceKit
![CI](https://github.com/Clipy/LoginServiceKit/workflows/Xcode-Build/badge.svg)
[![Release version](https://img.shields.io/github/release/Clipy/LoginServiceKit.svg)]()
[![License: Apache-2.0](https://img.shields.io/github/license/Clipy/LoginServiceKit.svg)](https://github.com/Clipy/LoginServiceKit/blob/master/LICENSE)
[![Carthage compatible](https://img.shields.io/badge/Carthage-compatible-4BC51D.svg?style=flat)](https://github.com/Carthage/Carthage)
[![SPM supported](https://img.shields.io/badge/SPM-supported-DE5C43.svg?style=flat)](https://swift.org/package-manager)

Easy manage login items in macOS.

## Usage
### CocoaPods
```
pod 'LoginServiceKit', :git => 'https://github.com/Clipy/LoginServiceKit'
```

### Carthage
```
github "Clipy/LoginServiceKit"
```

## Example
#### Check exist login item
```swift
let isExistLoginItem = LoginServiceKit.isExistLoginItems() // default Bundle.main.bundlePath
```

or

```swift
let isExistLoginItem = LoginServiceKit.isExistLoginItems(at: Bundle.main.bundlePath)
```

#### Add login item
```swift
LoginServiceKit.addLoginItems() // default Bundle.main.bundlePath
```

or

```swift
LoginServiceKit.addLoginItems(at: Bundle.main.bundlePath)
```

#### Remove login item
```swift
LoginServiceKit.removeLoginItems() // default Bundle.main.bundlePath
```

or

```swift
LoginServiceKit.removeLoginItems(at: Bundle.main.bundlePath)
```

## About Deprecated APIs
LoginServiceKit uses an API that has been deprecated since macOS 10.11 El Capitan. However, there is no API migration destination that meets the current specifications.
Therefore, this library will be discontinued when the API used is discontinued.

See this [issue](https://github.com/Clipy/LoginServiceKit/issues/10) for more details.

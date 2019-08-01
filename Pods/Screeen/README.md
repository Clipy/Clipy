# Screeen
[![Release version](https://img.shields.io/github/release/Clipy/Screeen.svg)](https://github.com/Clipy/Screeen/releases/latest)
[![License: MIT](https://img.shields.io/github/license/Clipy/Screeen.svg)](https://github.com/Clipy/Screeen/blob/master/LICENSE)
[![Carthage compatible](https://img.shields.io/badge/Carthage-compatible-4BC51D.svg?style=flat)](https://github.com/Carthage/Carthage)
[![Version](https://img.shields.io/cocoapods/v/Screeen.svg)](http://cocoadocs.org/docsets/Screeen)
[![Platform](https://img.shields.io/cocoapods/p/Screeen.svg)](http://cocoadocs.org/docsets/Screeen)

Observe user screen shot event and image in macOS.

## Usage
### CocoaPods
```
pod 'Screeen'
```

### Carthage
```
github "Clipy/Screeen"
```

## Example
```swift
let observer = ScreenShotObserver()
observer.delegate = self
```

```swift
func screenShotObserver(_ observer: ScreenShotObserver, addedItem item: NSMetadataItem) {
    print("added item == \(item)")
}

func screenShotObserver(_ observer: ScreenShotObserver, updatedItem item: NSMetadataItem) {
    print("updated item == \(item)")
}

func screenShotObserver(_ observer: ScreenShotObserver, removedItem item: NSMetadataItem) {
    print("removed item == \(item)")
}
```

Change observing status
```swift
observer.isEnabled = false // Stop observing
observer.isEnabled = true  // Restart observing
```

### Contributing
1. Fork it ( https://github.com/Clipy/Screeen/fork )
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create a new Pull Request

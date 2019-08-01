# RxScreeen
[![Release version](https://img.shields.io/github/release/Clipy/RxScreeen.svg)](https://github.com/Clipy/RxScreeen/releases/latest)
[![License: MIT](https://img.shields.io/github/license/Clipy/RxScreeen.svg)](https://github.com/Clipy/RxScreeen/blob/master/LICENSE)
[![Carthage compatible](https://img.shields.io/badge/Carthage-compatible-4BC51D.svg?style=flat)](https://github.com/Carthage/Carthage)
[![Version](https://img.shields.io/cocoapods/v/RxScreeen.svg)](http://cocoadocs.org/docsets/RxScreeen)
[![Platform](https://img.shields.io/cocoapods/p/RxScreeen.svg)](http://cocoadocs.org/docsets/RxScreeen)

RxScreeen is a RxSwift wrapper for Screeen.

## Usage
### CocoaPods
```
pod 'RxScreeen'
```

### Carthage
```
github "Clipy/RxSceeen"
github "Clipy/Screeen"
github "ReactiveX/RxSwift"
```

## Example
```swift
let observer = ScreenShotObserver()
observer.rx.image
  .subscribe(onNext: { image in
    // Add / Update / Remove events images
  })

observer.rx.item
  .subscribe(onNext: { item in
    // Add / Update / Remove events NSMetadataItem
  })

observer.rx.addedImage
  .subscribe(onNext: { image in
    // Add events image
  })

observer.rx.updatedImage
  .subscribe(onNext: { image in
    // Update events image
  })

observer.rx.removedImage
  .subscribe(onNext: { image in
    // Remove events image
  })
```

## Dependencies
- [Screeen](https://github.com/Clipy/Screeen)

## How to Build
1. Move to the project root directory
2. Install dependency library with `carthage` or `git submodule`
3. `carthage checkout --use-submodules` or `git submodule init && git submodule update`
4. Open `RxScreeen.xcworkspace` on Xcode.
5. build.

### Contributing
1. Fork it ( https://github.com/Clipy/RxScreeen/fork )
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create a new Pull Request

Clipy
=============================
[![Build Status](https://www.bitrise.io/app/4660f968baa24514.svg?token=u8XErsQsveSu74-PGWRrdw&branch=master)](https://www.bitrise.io/app/4660f968baa24514)
[![Release version](https://img.shields.io/github/release/Clipy/Clipy.svg)](https://github.com/Clipy/Clipy/releases/latest)
[![License: MIT](https://img.shields.io/github/license/Clipy/Clipy.svg)](https://github.com/Clipy/Clipy/blob/master/LICENSE)


Clipy is a Clipboard extension app for macOS.

__Requirement__: OS X Mavericks or higher

__Distribution Site__ : <https://clipy-app.com>

<img src="http://clipy-app.com/img/screenshot1.png" width="400">

### Development Environment
* OS X El Capitan
* Xcode 7.3
* Swift 2.2
* swiftlint 0.10.0

### How to Build
0. Move to the project root directory
1. Install [CocoaPods](http://cocoapods.org) and [fastlane](https://github.com/fastlane/fastlane) using Bundler. Run `bundle install`
2. Run `bundle exec pod install` on your terminal.
2. Open `Clipy.xcworkspace` on Xcode.
3. build.

### Dependencies
The source code is dependent on some libraries.
* [Sparkle](https://github.com/sparkle-project/Sparkle)
* [Realm](https://realm.io/)
* [PINCache](https://github.com/pinterest/PINCache)
* [Fabric](https://fabric.io)
* [RxSwift](https://github.com/ReactiveX/RxSwift)
* [RxCocoa](https://github.com/ReactiveX/RxSwift/tree/master/RxCocoa)
* [NSObject+Rx](https://github.com/RxSwiftCommunity/NSObject-Rx)
* [RxOptional](https://github.com/RxSwiftCommunity/RxOptional)

### Contributing
1. Fork it ( https://github.com/Clipy/Clipy/fork )
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create a new Pull Request

### Distribution
If you distribute derived work, especially in the Mac App Store, I ask you to follow two rules:

1. Don't use `Clipy` and `ClipMenu` as your product name.
2. Follow the MIT license terms.

Thank you for your cooperation.

### Licence
Clipy is available under the MIT license. See the LICENSE file for more info.

Icons are copyrighted by their respective authors.

### Special Thanks
__Thank you for [@naotaka](https://github.com/naotaka) who have published [ClipMenu](https://github.com/naotaka/ClipMenu) as OSS.__

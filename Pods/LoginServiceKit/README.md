# LoginServiceKit
[![Release version](https://img.shields.io/github/release/Clipy/LoginServiceKit.svg)]()
[![License: MIT](https://img.shields.io/github/license/Clipy/LoginServiceKit.svg)](https://github.com/Clipy/LoginServiceKit/blob/master/LICENSE)

Easy manage login items in MacOSX

## Usage 
```
platform :osx, '10.9'
use_frameworks!

pod 'LoginServiceKit', :git => 'https://github.com/Clipy/LoginServiceKit.git'
```

## Example
Add login item.

```swift
let appPath = NSBundle.mainBundle().bundlePath
LoginServiceKit.addPathToLoginItems(appPath)
```

Remove login item.
```swift
let appPath = NSBundle.mainBundle().bundlePath
LoginServiceKit.removePathFromLoginItems(appPath)
```

### Contributing
1. Fork it ( https://github.com/Clipy/LoginServiceKit/fork )
2. Create your feature branch (`git checkout -b my-new-feature`)
3. Commit your changes (`git commit -am 'Add some feature'`)
4. Push to the branch (`git push origin my-new-feature`)
5. Create a new Pull Request

# PINOperation

[![CocoaPods](https://img.shields.io/cocoapods/v/PINOperation.svg)](http://cocoadocs.org/docsets/PINOperation/)
[![Carthage compatible](https://img.shields.io/badge/Carthage-compatible-4BC51D.svg?style=flat)](https://github.com/Carthage/Carthage)
[![Build status](https://github.com/pinterest/PINOperation/workflows/CI/badge.svg)](https://github.com/pinterest/PINOperation/actions?query=workflow%3ACI+branch%3Amaster)

## Fast, concurrency-limited task queue for iOS and macOS.

## Installation

### Manually

[Download the latest tag](https://github.com/pinterest/PINOperation/tags) and drag the `PINOperation` folder into your Xcode project.

Install the docs by double clicking the `.docset` file under `docs/`, or view them online at [cocoadocs.org](http://cocoadocs.org/docsets/PINOperation/)

### Git Submodule

    git submodule add https://github.com/pinterest/PINOperation.git
    git submodule update --init

### CocoaPods

Add [PINOperation](http://cocoapods.org/?q=name%3APINOperation) to your `Podfile` and run `pod install`.

### Carthage

Add the following line to your `Cartfile` and run `carthage update --platform ios`. Then follow [this instruction of Carthage](https://github.com/carthage/carthage#adding-frameworks-to-unit-tests-or-a-framework) to embed the framework.

```github "pinterest/PINOperation"```

## Requirements

__PINOperation__ requires iOS 8.0, tvOS 9.0, macOS 10.8 or watchOS 2.0 and greater.

## Contact

[Garrett Moon](mailto:garrett@pinterest.com)

## License

Copyright 2013 Tumblr, Inc.
Copyright 2015 Pinterest, Inc.

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. [See the License](LICENSE.txt) for the specific language governing permissions and limitations under the License.

# RxOptional

[![CircleCI](https://img.shields.io/circleci/project/github/RxSwiftCommunity/RxOptional/master.svg)](https://circleci.com/gh/RxSwiftCommunity/RxOptional/tree/master)
[![Version](https://img.shields.io/cocoapods/v/RxOptional.svg?style=flat)](http://cocoapods.org/pods/RxOptional)
[![License](https://img.shields.io/cocoapods/l/RxOptional.svg?style=flat)](http://cocoapods.org/pods/RxOptional)
[![Platform](https://img.shields.io/cocoapods/p/RxOptional.svg?style=flat)](http://cocoapods.org/pods/RxOptional)

RxSwift extensions for Swift optionals and "Occupiable" types.

## Usage

All operators are also available on `Driver` and `Signal`, unless otherwise noted.

### Optional Operators

##### filterNil

```swift
Observable<String?>
    .of("One", nil, "Three")
    .filterNil()
    // Type is now Observable<String>
    .subscribe { print($0) }
```

```text
next(One)
next(Three)
completed
```

##### replaceNilWith

```swift
Observable<String?>
    .of("One", nil, "Three")
    .replaceNilWith("Two")
    // Type is now Observable<String>
    .subscribe { print($0) }
```

```text
next(One)
next(Two)
next(Three)
completed
```

##### errorOnNil

Unavailable on `Driver`, because `Driver`s cannot error out.

By default errors with `RxOptionalError.foundNilWhileUnwrappingOptional`.

```swift
Observable<String?>
    .of("One", nil, "Three")
    .errorOnNil()
    // Type is now Observable<String>
    .subscribe { print($0) }
```

```text
next(One)
error(Found nil while trying to unwrap type <Optional<String>>)
```

##### catchOnNil

```swift
Observable<String?>
    .of("One", nil, "Three")
    .catchOnNil {
        return Observable<String>.just("A String from a new Observable")
    }
    // Type is now Observable<String>
    .subscribe { print($0) }
```

```text
next(One)
next(A String from a new Observable)
next(Three)
completed
```

##### distinctUntilChanged

```swift
Observable<Int?>
    .of(5, 6, 6, nil, nil, 3)
    .distinctUntilChanged()
    .subscribe { print($0) }
```

```text
next(Optional(5))
next(Optional(6))
next(nil)
next(Optional(3))
completed
```

### Occupiable Operators

Occupiables are:

- `String`
- `Array`
- `Dictionary`
- `Set`

Currently in Swift protocols cannot be extended to conform to other protocols.
For now the types listed above conform to `Occupiable`. You can also conform
custom types to `Occupiable`.

##### filterEmpty

```swift
Observable<[String]>
    .of(["Single Element"], [], ["Two", "Elements"])
    .filterEmpty()
    .subscribe { print($0) }
```

```text
next(["Single Element"])
next(["Two", "Elements"])
completed
```

##### errorOnEmpty

Unavailable on `Driver`, because `Driver`s cannot error out.

By default errors with `RxOptionalError.emptyOccupiable`.

```swift
Observable<[String]>
    .of(["Single Element"], [], ["Two", "Elements"])
    .errorOnEmpty()
    .subscribe { print($0) }
```

```text
next(["Single Element"])
error(Empty occupiable of type <Array<String>>)
```

##### catchOnEmpty

```swift
Observable<[String]>
    .of(["Single Element"], [], ["Two", "Elements"])
    .catchOnEmpty {
        return Observable<[String]>.just(["Not Empty"])
    }
    .subscribe { print($0) }
```

```text
next(["Single Element"])
next(["Not Empty"])
next(["Two", "Elements"])
completed
```

## Running Examples.playground

- Run `pod install` in Example directory
- Select RxOptional Examples target
- Build
- Show Debug Area (cmd+shift+Y)
- Click blue play button in Debug Area

## Requirements

- [RxSwift](https://github.com/ReactiveX/RxSwift)
- [RxCocoa](https://github.com/ReactiveX/RxSwift)

## Installation

### [CocoaPods](https://guides.cocoapods.org/using/using-cocoapods.html)

RxOptional is available through [CocoaPods](http://cocoapods.org). To install
it, simply add the following line to your Podfile:

```ruby
pod 'RxOptional'
```

### [Carthage](https://github.com/Carthage/Carthage)

Add this to `Cartfile`

```
github "RxSwiftCommunity/RxOptional" ~> 3.1.3
```

```
$ carthage update
```

### [Swift Package Manager](https://swift.org/package-manager)

To use RxOptional as a Swift Package Manager package just add the following in your Package.swift file.

```swift
import PackageDescription

let package = Package(
    name: "ProjectName",
    dependencies: [
        .Package(url: "https://github.com/RxSwiftCommunity/RxOptional")
    ]
)
```

## Author

Thane Gill, me@thanegill.com

## License

RxOptional is available under the MIT license. See the LICENSE file for more info.

# RxOptional

[![Build Status](https://travis-ci.org/RxSwiftCommunity/RxOptional.svg?branch=master)](https://travis-ci.org/RxSwiftCommunity/RxOptional)
[![Version](https://img.shields.io/cocoapods/v/RxOptional.svg?style=flat)](http://cocoapods.org/pods/RxOptional)
[![License](https://img.shields.io/cocoapods/l/RxOptional.svg?style=flat)](http://cocoapods.org/pods/RxOptional)
[![Platform](https://img.shields.io/cocoapods/p/RxOptional.svg?style=flat)](http://cocoapods.org/pods/RxOptional)


RxSwift extensions for Swift optionals and "Occupiable" types.

## Usage

All operators are also available on `Driver`, unless otherwise noted.

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
Next(One)
Next(Three)
Completed
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
Next(One)
Next(Two)
Next(Three)
Completed
```

##### errorOnNil
Unavailable on `Driver`, because `Driver`s cannot error out.

By default errors with `RxOptionalError.FoundNilWhileUnwrappingOptional`.
```swift
Observable<String?>
    .of("One", nil, "Three")
    .errorOnNil()
    // Type is now Observable<String>
    .subscribe { print($0) }
```
```text
Next(One)
Error(Found nil while trying to unwrap type <Optional<String>>)
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
Next(One)
Next(A String from a new Observable)
Next(Three)
Completed
```

##### distinctUntilChanged
```swift
Observable<Int?>
    .of(5, 6, 6, nil, nil, 3)
    .distinctUntilChanged()
    .subscribe { print($0) }
```
```text
Next(Optional(5))
Next(Optional(6))
Next(nil)
Next(Optional(3))
Completed
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
Next(["Single Element"])
Next(["Two", "Elements"])
Completed
```

##### errorOnEmpty
Unavailable on `Driver`, because `Driver`s cannot error out.

By default errors with `RxOptionalError.EmptyOccupiable`.
```swift
Observable<[String]>
    .of(["Single Element"], [], ["Two", "Elements"])
    .errorOnEmpty()
    .subscribe { print($0) }
```
```text
Next(["Single Element"])
Error(Empty occupiable of type <Array<String>>)
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
Next(["Single Element"])
Next(["Not Empty"])
Next(["Two", "Elements"])
Completed
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

## Author

Thane Gill, me@thanegill.com

## License

RxOptional is available under the MIT license. See the LICENSE file for more info.

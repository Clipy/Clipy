# RxRealm

[![Carthage Compatible](https://img.shields.io/badge/Carthage-compatible-4BC51D.svg?style=flat)](https://github.com/Carthage/Carthage)
[![Version](https://img.shields.io/cocoapods/v/RxRealm.svg?style=flat)](http://cocoapods.org/pods/RxRealm)
[![License](https://img.shields.io/cocoapods/l/RxRealm.svg?style=flat)](http://cocoapods.org/pods/RxRealm)
[![Platform](https://img.shields.io/cocoapods/p/RxRealm.svg?style=flat)](http://cocoapods.org/pods/RxRealm)

## Usage

This library is a thin wrapper around __RealmSwift__.

**NB**: For Swift 3 projects add this snippet to the bottom of your project's Podfile. This will update your targets to use swift3:

```
post_install do |installer|
    installer.pods_project.targets.each do |target|
        target.build_configurations.each do |config|
            config.build_settings['SWIFT_VERSION'] = '3.0'
            config.build_settings['MACOSX_DEPLOYMENT_TARGET'] = '10.10'
        end
    end
end
```

### Observing collections

RxRealm can be used to create `Observable`s from objects of type `Results`, `List`, `LinkingObjects` or `AnyRealmCollection` as follows:

#### Observable.from(_:)
Emits every time the collection changes

```swift
let realm = try! Realm()
let laps = realm.objects(Lap.self)

Observable.from(laps)
  .map { 
    laps in "\(laps.count) laps"
  }.subscribe(onNext: { text  in
    print(text)
  })
```

#### Observable.arrayFrom(_:)
Fetches the a snapshot of a Realm collection and converts it to an array value (for example if you want to use array methods on the collection)

```swift
let realm = try! Realm()
let laps = realm.objects(Lap.self)

Observable.arrayFrom(laps)
  .map { array in
    return array.prefix(3) //slice of first 3 items
  }.subscribe(onNext: { text  in
    print(text)
  })
```

#### Observable.changesetFrom(_:)
Emits every time the collection changes and provides the exact indexes that has been deleted, inserted or updated

```swift
let realm = try! Realm()
let laps = realm.objects(Lap.self))

Observable.changesetFrom(laps)
  .subscribe(onNext: { results, changes in
    if let changes = changes {
    // it's an update
    print(results)
    print("deleted: \(changes.deleted) inserted: \(changes.inserted) updated: \(changes.updated)")
  } else {
    // it's the initial data
    print(results)
  }
  })
```

#### Observable.changesetArrayFrom(_:)
Combines the result of `Observable.arrayFrom(_:)` and `Observable.changesetFrom(_:)` returning an `Observable<Array<T>, RealmChangeset?>`

```swift
let realm = try! Realm()
let laps = realm.objects(Lap.self))

Observable.changesetArrayFrom(laps)
  .subscribe(onNext: { array, changes in
    if let changes = changes {
    // it's an update
    print(array.first)
    print("deleted: \(changes.deleted) inserted: \(changes.inserted) updated: \(changes.updated)")
  } else {
    // it's the initial data
    print(array)
  }
  })
```

### Observing a single object

There's a separate API to make it easier to observe single object (it creates `Results` behind the scenes):

```swift
Observable.from(ticker)
    .map({ (ticker) -> String in
        return "\(ticker.ticks) ticks"
    })
    .bindTo(footer.rx.text)
```

This API uses the primary key of the object to query the database for it and observe for change notifications. Observing objects without a primary key does not work.

### Performing transactions

#### rx.add()
##### **Writing to an existing Realm reference**

You can add newly created objects to a Realm that you already have initialized:

```swift
let realm = try! Realm()
let messages = [Message("hello"), Message("world")]

Observable.from(messages)
  .subscribe(realm.rx.add())
```

Be careful, this will retain your Realm until the `Observable` completes or errors out.

##### **Writing to the default Realm**
You can leave it to RxRealm to grab the default Realm on any thread your subscribe and write objects to it:

```swift
let messages = [Message("hello"), Message("world")]

Observable.from(messages)
  .subscribe(Realm.rx.add())
```

##### **Writing to a specific Realm**
If you want to switch threads and not use the default Realm, provide a `Realm.Configuration`:

```swift
var config = Realm.Configuration()
/* custom configuration settings */

let messages = [Message("hello"), Message("world")]
Observable.from(messages)
  .observeOn( /* you can switch threads if you want to */ )     
  .subscribe(Realm.rx.add(configuration: config))
```

If you want to create a Realm on a different thread manually, allowing you to handle errors, you can do that too:

```swift
let messages = [Message("hello"), Message("world")]

Observable.from(messages)
  .observeOn( /* you can switch threads if you want to */ )
  .subscribe(onNext: {messages in
    let realm = try! Realm()
    try! realm.write {
      realm.add(messages)
    }
  })
```

#### rx.delete()

#####**Deleting from an existing realm reference**
```swift
let realm = try! Realm()
let messages = realm.objects(Message.self)
Observable.from(messages)
  .subscribe(realm.rx.delete())
```

Be careful, this will retain your realm until the `Observable` completes or errors out.

#####**Deleting from the object's realm automatically**
You can leave it to RxRealm to grab the Realm from the first object and use it:

```swift
Observable.from(someCollectionOfPersistedObjects)
  .subscribe(Realm.rx.delete())
```


## Example app

To run the example project, clone the repo, and run `pod install` from the Example directory first. The app uses RxSwift, RxCocoa using RealmSwift, RxRealm to observe Results from Realm.

Further you're welcome to peak into the __RxRealmTests__ folder of the example app, which features the library's unit tests.

## Installation

This library depends on both __RxSwift__ and __RealmSwift__ 1.0+.

#### CocoaPods
RxRealm is available through [CocoaPods](http://cocoapods.org). To install it, simply add the following line to your Podfile:

```ruby
pod "RxRealm"
```

#### Carthage

RxRealm is available through [Carthage](https://github.com/Carthage/Carthage). You can install Carthage with [Homebrew](http://brew.sh/) using the following command:

```bash
$ brew update
$ brew install carthage
```

To integrate RxRealm into your Xcode project using Carthage, specify it in your `Cartfile`:

```ogdl
github "RxSwiftCommunity/RxRealm" ~> 1.0
```

Run `carthage update` to build the framework and drag the built `RxRealm.framework` into your Xcode project.

#### As Source

You can grab the files in `Pod/Classes` from this repo and include them in your project.

## TODO

* Test add platforms and add compatibility for the pod

## License

This library belongs to _RxSwiftCommunity_.

RxRealm is available under the MIT license. See the LICENSE file for more info.

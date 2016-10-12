[![Build Status](https://travis-ci.org/RxSwiftCommunity/NSObject-Rx.svg)](https://travis-ci.org/RxSwiftCommunity/NSObject-Rx)

NSObject+Rx
===========

If you're using [RxSwift](https://github.com/ReactiveX/RxSwift), you've probably encountered the following code more than a few times.

```swift
class MyObject: Whatever {
	let disposeBag = DisposeBag()

	...
}
```

You're actually not the only one; it has been typed many, many times.

[![Search screenshot showing many, many results.](assets/screenshot.png)](https://github.com/search?q=let+disposeBag+%3D+DisposeBag%28%29&type=Code&utf8=✓)

Instead of adding a new property to every object, use this library to add it for you, to any subclass of `NSObject`.

```swift
thing
  .bindTo(otherThing)
  .addDisposableTo(rx_disposeBag)
```

Sweet.

It'll work just like a property: when the instance is deinit'd, the `DisposeBag` gets disposed. It's also a read/write property, so you can use your own, too.

Installing
----------

####CocoaPods

This works with RxSwift version 2, which is still prerelease, so you've gotta be fancy with your podfile.

```ruby
pod 'NSObject+Rx'
```

And that'll be 👌

####Carthage

Add to `Cartfile`:
```
github "RxSwiftCommunity/NSObject-Rx" ~> 1.3.0
```
Add frameworks to your project (no need to "copy items if needed")

Run `carthage update` or `carthage update --platform ios` if you target iOS only

Add run script build phase `/usr/local/bin/carthage copy-frameworks`
with input files being:

```
$(SRCROOT)/Carthage/Build/iOS/RxSwift.framework
$(SRCROOT)/Carthage/Build/iOS/NSObject_Rx.framework
```

And rule ✌️

License
-------

MIT obvs.

![Tim Cook dancing to the sound of a permissive license.](http://i.imgur.com/mONiWzj.gif)

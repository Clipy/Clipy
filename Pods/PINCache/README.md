# PINCache

[![CocoaPods](https://img.shields.io/cocoapods/v/PINCache.svg)](http://cocoadocs.org/docsets/PINCache/)
[![Carthage compatible](https://img.shields.io/badge/Carthage-compatible-4BC51D.svg?style=flat)](https://github.com/Carthage/Carthage)
[![Tavis CI build](https://img.shields.io/travis/pinterest/PINCache.svg?style=flat)](https://travis-ci.org/pinterest/PINCache)

## Fast, non-deadlocking parallel object cache for iOS and OS X.

[PINCache](PINCache/PINCache.h) is a fork of [TMCache](https://github.com/tumblr/TMCache) re-architected to fix issues with deadlocking caused by heavy use. It is a key/value store designed for persisting temporary objects that are expensive to reproduce, such as downloaded data or the results of slow processing. It is comprised of two self-similar stores, one in memory ([PINMemoryCache](PINCache/PINMemoryCache.h)) and one on disk ([PINDiskCache](PINCache/PINDiskCache.h)), all backed by GCD and safe to access from multiple threads simultaneously. On iOS, `PINMemoryCache` will clear itself when the app receives a memory warning or goes into the background. Objects stored in `PINDiskCache` remain until you trim the cache yourself, either manually or by setting a byte or age limit.

`PINCache` and `PINDiskCache` accept any object conforming to [NSCoding](https://developer.apple.com/library/ios/#documentation/Cocoa/Reference/Foundation/Protocols/NSCoding_Protocol/Reference/Reference.html). Put things in like this:

```objective-c
UIImage *img = [[UIImage alloc] initWithData:data scale:[[UIScreen mainScreen] scale]];
[[PINCache sharedCache] setObject:img forKey:@"image" block:nil]; // returns immediately
```

Get them back out like this:

```objective-c
[[PINCache sharedCache] objectForKey:@"image"
                              block:^(PINCache *cache, NSString *key, id object) {
                                  UIImage *image = (UIImage *)object;
                                  NSLog(@"image scale: %f", image.scale);
                              }];
```

Both `PINMemoryCache` and PINDiskCache use locks to protect reads and writes. `PINCache` coordinates them so that objects added to memory are available immediately to other threads while being written to disk safely in the background. Both caches are public properties of `PINCache`, so it's easy to manipulate one or the other separately if necessary.

Collections work too. Thanks to the magic of `NSKeyedArchiver`, objects repeated in a collection only occupy the space of one on disk:

```objective-c
NSArray *images = @[ image, image, image ];
[[PINCache sharedCache] setObject:images forKey:@"images"];
NSLog(@"3 for the price of 1: %d", [[[PINCache sharedCache] diskCache] byteCount]);
```

## Installation

### Manually

[Download the latest tag](https://github.com/pinterest/PINCache/tags) and drag the `PINCache` folder into your Xcode project.

Install the docs by double clicking the `.docset` file under `docs/`, or view them online at [cocoadocs.org](http://cocoadocs.org/docsets/PINCache/)

### Git Submodule

    git submodule add https://github.com/pinterest/PINCache.git
    git submodule update --init

### CocoaPods

Add [PINCache](http://cocoapods.org/?q=name%3APINCache) to your `Podfile` and run `pod install`.

### Carthage

Add the following line to your `Cartfile` and run `carthage update --platform ios`. Then follow [this instruction of Carthage](https://github.com/carthage/carthage#adding-frameworks-to-unit-tests-or-a-framework) to embed the framework.

```github "pinterest/PINCache"```

## Requirements

__PINCache__ requires iOS 5.0 or OS X 10.7 and greater.

## Contact

[Garrett Moon](mailto:garrett@pinterest.com)

## License

Copyright 2013 Tumblr, Inc.
Copyright 2015 Pinterest, Inc.

Licensed under the Apache License, Version 2.0 (the "License"); you may not use this file except in compliance with the License. You may obtain a copy of the License at http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software distributed under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied. [See the License](LICENSE.txt) for the specific language governing permissions and limitations under the License.

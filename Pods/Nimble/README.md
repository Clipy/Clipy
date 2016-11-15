# Nimble

Use Nimble to express the expected outcomes of Swift
or Objective-C expressions. Inspired by
[Cedar](https://github.com/pivotal/cedar).

```swift
// Swift
expect(1 + 1).to(equal(2))
expect(1.2).to(beCloseTo(1.1, within: 0.1))
expect(3) > 2
expect("seahorse").to(contain("sea"))
expect(["Atlantic", "Pacific"]).toNot(contain("Mississippi"))
expect(ocean.isClean).toEventually(beTruthy())
```

# How to Use Nimble

<!-- START doctoc generated TOC please keep comment here to allow auto update -->
<!-- DON'T EDIT THIS SECTION, INSTEAD RE-RUN doctoc TO UPDATE -->
**Table of Contents**  *generated with [DocToc](https://github.com/thlorenz/doctoc)*

- [Some Background: Expressing Outcomes Using Assertions in XCTest](#some-background-expressing-outcomes-using-assertions-in-xctest)
- [Nimble: Expectations Using `expect(...).to`](#nimble-expectations-using-expectto)
  - [Custom Failure Messages](#custom-failure-messages)
  - [Type Checking](#type-checking)
  - [Operator Overloads](#operator-overloads)
  - [Lazily Computed Values](#lazily-computed-values)
  - [C Primitives](#c-primitives)
  - [Asynchronous Expectations](#asynchronous-expectations)
  - [Objective-C Support](#objective-c-support)
  - [Disabling Objective-C Shorthand](#disabling-objective-c-shorthand)
- [Built-in Matcher Functions](#built-in-matcher-functions)
  - [Equivalence](#equivalence)
  - [Identity](#identity)
  - [Comparisons](#comparisons)
  - [Types/Classes](#typesclasses)
  - [Truthiness](#truthiness)
  - [Swift Assertions](#swift-assertions)
  - [Swift Error Handling](#swift-error-handling)
  - [Exceptions](#exceptions)
  - [Collection Membership](#collection-membership)
  - [Strings](#strings)
  - [Checking if all elements of a collection pass a condition](#checking-if-all-elements-of-a-collection-pass-a-condition)
  - [Verify collection count](#verify-collection-count)
  - [Verify a notification was posted](#verifying-a-notification-was-posted)
  - [Matching a value to any of a group of matchers](#matching-a-value-to-any-of-a-group-of-matchers)
- [Writing Your Own Matchers](#writing-your-own-matchers)
  - [Lazy Evaluation](#lazy-evaluation)
  - [Type Checking via Swift Generics](#type-checking-via-swift-generics)
  - [Customizing Failure Messages](#customizing-failure-messages)
  - [Supporting Objective-C](#supporting-objective-c)
    - [Properly Handling `nil` in Objective-C Matchers](#properly-handling-nil-in-objective-c-matchers)
- [Installing Nimble](#installing-nimble)
  - [Installing Nimble as a Submodule](#installing-nimble-as-a-submodule)
  - [Installing Nimble via CocoaPods](#installing-nimble-via-cocoapods)
  - [Using Nimble without XCTest](#using-nimble-without-xctest)

<!-- END doctoc generated TOC please keep comment here to allow auto update -->

# Some Background: Expressing Outcomes Using Assertions in XCTest

Apple's Xcode includes the XCTest framework, which provides
assertion macros to test whether code behaves properly.
For example, to assert that `1 + 1 = 2`, XCTest has you write:

```swift
// Swift

XCTAssertEqual(1 + 1, 2, "expected one plus one to equal two")
```

Or, in Objective-C:

```objc
// Objective-C

XCTAssertEqual(1 + 1, 2, @"expected one plus one to equal two");
```

XCTest assertions have a couple of drawbacks:

1. **Not enough macros.** There's no easy way to assert that a string
   contains a particular substring, or that a number is less than or
   equal to another.
2. **It's hard to write asynchronous tests.** XCTest forces you to write
   a lot of boilerplate code.

Nimble addresses these concerns.

# Nimble: Expectations Using `expect(...).to`

Nimble allows you to express expectations using a natural,
easily understood language:

```swift
// Swift

import Nimble

expect(seagull.squawk).to(equal("Squee!"))
```

```objc
// Objective-C

@import Nimble;

expect(seagull.squawk).to(equal(@"Squee!"));
```

> The `expect` function autocompletes to include `file:` and `line:`,
  but these parameters are optional. Use the default values to have
  Xcode highlight the correct line when an expectation is not met.

To perform the opposite expectation--to assert something is *not*
equal--use `toNot` or `notTo`:

```swift
// Swift

import Nimble

expect(seagull.squawk).toNot(equal("Oh, hello there!"))
expect(seagull.squawk).notTo(equal("Oh, hello there!"))
```

```objc
// Objective-C

@import Nimble;

expect(seagull.squawk).toNot(equal(@"Oh, hello there!"));
expect(seagull.squawk).notTo(equal(@"Oh, hello there!"));
```

## Custom Failure Messages

Would you like to add more information to the test's failure messages? Use the `description` optional argument to add your own text:

```swift
// Swift

expect(1 + 1).to(equal(3))
// failed - expected to equal <3>, got <2>

expect(1 + 1).to(equal(3), description: "Make sure libKindergartenMath is loaded")
// failed - Make sure libKindergartenMath is loaded
// expected to equal <3>, got <2>
```

Or the *WithDescription version in Objective-C:

```objc
// Objective-C

@import Nimble;

expect(@(1+1)).to(equal(@3));
// failed - expected to equal <3.0000>, got <2.0000>

expect(@(1+1)).toWithDescription(equal(@3), @"Make sure libKindergartenMath is loaded");
// failed - Make sure libKindergartenMath is loaded
// expected to equal <3.0000>, got <2.0000>
```

## Type Checking

Nimble makes sure you don't compare two types that don't match:

```swift
// Swift

// Does not compile:
expect(1 + 1).to(equal("Squee!"))
```

> Nimble uses generics--only available in Swift--to ensure
  type correctness. That means type checking is
  not available when using Nimble in Objective-C. :sob:

## Operator Overloads

Tired of so much typing? With Nimble, you can use overloaded operators
like `==` for equivalence, or `>` for comparisons:

```swift
// Swift

// Passes if squawk does not equal "Hi!":
expect(seagull.squawk) != "Hi!"

// Passes if 10 is greater than 2:
expect(10) > 2
```

> Operator overloads are only available in Swift, so you won't be able
  to use this syntax in Objective-C. :broken_heart:

## Lazily Computed Values

The `expect` function doesn't evaluate the value it's given until it's
time to match. So Nimble can test whether an expression raises an
exception once evaluated:

```swift
// Swift

// Note: Swift currently doesn't have exceptions.
//       Only Objective-C code can raise exceptions
//       that Nimble will catch.
//       (see https://github.com/Quick/Nimble/issues/220#issuecomment-172667064)
let exception = NSException(
  name: NSInternalInconsistencyException,
  reason: "Not enough fish in the sea.",
  userInfo: ["something": "is fishy"])
expect { exception.raise() }.to(raiseException())

// Also, you can customize raiseException to be more specific
expect { exception.raise() }.to(raiseException(named: NSInternalInconsistencyException))
expect { exception.raise() }.to(raiseException(
    named: NSInternalInconsistencyException,
    reason: "Not enough fish in the sea"))
expect { exception.raise() }.to(raiseException(
    named: NSInternalInconsistencyException,
    reason: "Not enough fish in the sea",
    userInfo: ["something": "is fishy"]))
```

Objective-C works the same way, but you must use the `expectAction`
macro when making an expectation on an expression that has no return
value:

```objc
// Objective-C

NSException *exception = [NSException exceptionWithName:NSInternalInconsistencyException
                                                 reason:@"Not enough fish in the sea."
                                               userInfo:nil];
expectAction(^{ [exception raise]; }).to(raiseException());

// Use the property-block syntax to be more specific.
expectAction(^{ [exception raise]; }).to(raiseException().named(NSInternalInconsistencyException));
expectAction(^{ [exception raise]; }).to(raiseException().
    named(NSInternalInconsistencyException).
    reason("Not enough fish in the sea"));
expectAction(^{ [exception raise]; }).to(raiseException().
    named(NSInternalInconsistencyException).
    reason("Not enough fish in the sea").
    userInfo(@{@"something": @"is fishy"}));

// You can also pass a block for custom matching of the raised exception
expectAction(exception.raise()).to(raiseException().satisfyingBlock(^(NSException *exception) {
    expect(exception.name).to(beginWith(NSInternalInconsistencyException));
}));
```

## C Primitives

Some testing frameworks make it hard to test primitive C values.
In Nimble, it just works:

```swift
// Swift

let actual: CInt = 1
let expectedValue: CInt = 1
expect(actual).to(equal(expectedValue))
```

In fact, Nimble uses type inference, so you can write the above
without explicitly specifying both types:

```swift
// Swift

expect(1 as CInt).to(equal(1))
```

> In Objective-C, Nimble only supports Objective-C objects. To
  make expectations on primitive C values, wrap then in an object
  literal:

  ```objc
  expect(@(1 + 1)).to(equal(@2));
  ```

## Asynchronous Expectations

In Nimble, it's easy to make expectations on values that are updated
asynchronously. Just use `toEventually` or `toEventuallyNot`:

```swift
// Swift

dispatch_async(dispatch_get_main_queue()) {
  ocean.add("dolphins")
  ocean.add("whales")
}
expect(ocean).toEventually(contain("dolphins", "whales"))
```


```objc
// Objective-C
dispatch_async(dispatch_get_main_queue(), ^{
  [ocean add:@"dolphins"];
  [ocean add:@"whales"];
});
expect(ocean).toEventually(contain(@"dolphins", @"whales"));
```

Note: toEventually triggers its polls on the main thread. Blocking the main
thread will cause Nimble to stop the run loop. This can cause test pollution
for whatever incomplete code that was running on the main thread.  Blocking the
main thread can be caused by blocking IO, calls to sleep(), deadlocks, and
synchronous IPC.

In the above example, `ocean` is constantly re-evaluated. If it ever
contains dolphins and whales, the expectation passes. If `ocean` still
doesn't contain them, even after being continuously re-evaluated for one
whole second, the expectation fails.

Sometimes it takes more than a second for a value to update. In those
cases, use the `timeout` parameter:

```swift
// Swift

// Waits three seconds for ocean to contain "starfish":
expect(ocean).toEventually(contain("starfish"), timeout: 3)

// Evaluate someValue every 0.2 seconds repeatedly until it equals 100, or fails if it timeouts after 5.5 seconds.
expect(someValue).toEventually(equal(100), timeout: 5.5, pollInterval: 0.2)
```

```objc
// Objective-C

// Waits three seconds for ocean to contain "starfish":
expect(ocean).withTimeout(3).toEventually(contain(@"starfish"));
```

You can also provide a callback by using the `waitUntil` function:

```swift
// Swift

waitUntil { done in
  // do some stuff that takes a while...
  NSThread.sleepForTimeInterval(0.5)
  done()
}
```

```objc
// Objective-C

waitUntil(^(void (^done)(void)){
  // do some stuff that takes a while...
  [NSThread sleepForTimeInterval:0.5];
  done();
});
```

`waitUntil` also optionally takes a timeout parameter:

```swift
// Swift

waitUntil(timeout: 10) { done in
  // do some stuff that takes a while...
  NSThread.sleepForTimeInterval(1)
  done()
}
```

```objc
// Objective-C

waitUntilTimeout(10, ^(void (^done)(void)){
  // do some stuff that takes a while...
  [NSThread sleepForTimeInterval:1];
  done();
});
```

Note: waitUntil triggers its timeout code on the main thread. Blocking the main
thread will cause Nimble to stop the run loop to continue. This can cause test
pollution for whatever incomplete code that was running on the main thread.
Blocking the main thread can be caused by blocking IO, calls to sleep(),
deadlocks, and synchronous IPC.

In some cases (e.g. when running on slower machines) it can be useful to modify
the default timeout and poll interval values. This can be done as follows:

```swift
// Swift

// Increase the global timeout to 5 seconds:
Nimble.AsyncDefaults.Timeout = 5

// Slow the polling interval to 0.1 seconds:
Nimble.AsyncDefaults.PollInterval = 0.1
```

## Objective-C Support

Nimble has full support for Objective-C. However, there are two things
to keep in mind when using Nimble in Objective-C:

1. All parameters passed to the `expect` function, as well as matcher
   functions like `equal`, must be Objective-C objects or can be converted into
   an `NSObject` equivalent:

   ```objc
   // Objective-C

   @import Nimble;

   expect(@(1 + 1)).to(equal(@2));
   expect(@"Hello world").to(contain(@"world"));

   // Boxed as NSNumber *
   expect(2).to(equal(2));
   expect(1.2).to(beLessThan(2.0));
   expect(true).to(beTruthy());

   // Boxed as NSString *
   expect("Hello world").to(equal("Hello world"));

   // Boxed as NSRange
   expect(NSMakeRange(1, 10)).to(equal(NSMakeRange(1, 10)));
   ```

2. To make an expectation on an expression that does not return a value,
   such as `-[NSException raise]`, use `expectAction` instead of
   `expect`:

   ```objc
   // Objective-C

   expectAction(^{ [exception raise]; }).to(raiseException());
   ```

The following types are currently converted to an `NSObject` type:

 - **C Numeric types** are converted to `NSNumber *`
 - `NSRange` is converted to `NSValue *`
 - `char *` is converted to `NSString *`

For the following matchers:

- `equal`
- `beGreaterThan`
- `beGreaterThanOrEqual`
- `beLessThan`
- `beLessThanOrEqual`
- `beCloseTo`
- `beTrue`
- `beFalse`
- `beTruthy`
- `beFalsy`
- `haveCount`

If you would like to see more, [file an issue](https://github.com/Quick/Nimble/issues).

## Disabling Objective-C Shorthand

Nimble provides a shorthand for expressing expectations using the
`expect` function. To disable this shorthand in Objective-C, define the
`NIMBLE_DISABLE_SHORT_SYNTAX` macro somewhere in your code before
importing Nimble:

```objc
#define NIMBLE_DISABLE_SHORT_SYNTAX 1

@import Nimble;

NMB_expect(^{ return seagull.squawk; }, __FILE__, __LINE__).to(NMB_equal(@"Squee!"));
```

> Disabling the shorthand is useful if you're testing functions with
  names that conflict with Nimble functions, such as `expect` or
  `equal`. If that's not the case, there's no point in disabling the
  shorthand.

# Built-in Matcher Functions

Nimble includes a wide variety of matcher functions.

## Equivalence

```swift
// Swift

// Passes if actual is equivalent to expected:
expect(actual).to(equal(expected))
expect(actual) == expected

// Passes if actual is not equivalent to expected:
expect(actual).toNot(equal(expected))
expect(actual) != expected
```

```objc
// Objective-C

// Passes if actual is equivalent to expected:
expect(actual).to(equal(expected))

// Passes if actual is not equivalent to expected:
expect(actual).toNot(equal(expected))
```

Values must be `Equatable`, `Comparable`, or subclasses of `NSObject`.
`equal` will always fail when used to compare one or more `nil` values.

## Identity

```swift
// Swift

// Passes if actual has the same pointer address as expected:
expect(actual).to(beIdenticalTo(expected))
expect(actual) === expected

// Passes if actual does not have the same pointer address as expected:
expect(actual).toNot(beIdenticalTo(expected))
expect(actual) !== expected
```

Its important to remember that `beIdenticalTo` only makes sense when comparing types with reference semantics, which have a notion of identity. In Swift, that means a `class`. This matcher will not work with types with value semantics such as `struct` or `enum`. If you need to compare two value types, you can either compare individual properties or if it makes sense to do so, make your type implement `Equatable` and use Nimble's equivalence matchers instead.


```objc
// Objective-C

// Passes if actual has the same pointer address as expected:
expect(actual).to(beIdenticalTo(expected));

// Passes if actual does not have the same pointer address as expected:
expect(actual).toNot(beIdenticalTo(expected));
```

## Comparisons

```swift
// Swift

expect(actual).to(beLessThan(expected))
expect(actual) < expected

expect(actual).to(beLessThanOrEqualTo(expected))
expect(actual) <= expected

expect(actual).to(beGreaterThan(expected))
expect(actual) > expected

expect(actual).to(beGreaterThanOrEqualTo(expected))
expect(actual) >= expected
```

```objc
// Objective-C

expect(actual).to(beLessThan(expected));
expect(actual).to(beLessThanOrEqualTo(expected));
expect(actual).to(beGreaterThan(expected));
expect(actual).to(beGreaterThanOrEqualTo(expected));
```

> Values given to the comparison matchers above must implement
  `Comparable`.

Because of how computers represent floating point numbers, assertions
that two floating point numbers be equal will sometimes fail. To express
that two numbers should be close to one another within a certain margin
of error, use `beCloseTo`:

```swift
// Swift

expect(actual).to(beCloseTo(expected, within: delta))
```

```objc
// Objective-C

expect(actual).to(beCloseTo(expected).within(delta));
```

For example, to assert that `10.01` is close to `10`, you can write:

```swift
// Swift

expect(10.01).to(beCloseTo(10, within: 0.1))
```

```objc
// Objective-C

expect(@(10.01)).to(beCloseTo(@10).within(0.1));
```

There is also an operator shortcut available in Swift:

```swift
// Swift

expect(actual) ≈ expected
expect(actual) ≈ (expected, delta)

```
(Type Option-x to get ≈ on a U.S. keyboard)

The former version uses the default delta of 0.0001. Here is yet another way to do this:

```swift
// Swift

expect(actual) ≈ expected ± delta
expect(actual) == expected ± delta

```
(Type Option-Shift-= to get ± on a U.S. keyboard)

If you are comparing arrays of floating point numbers, you'll find the following useful:

```swift
// Swift

expect([0.0, 2.0]) ≈ [0.0001, 2.0001]
expect([0.0, 2.0]).to(beCloseTo([0.1, 2.1], within: 0.1))

```

> Values given to the `beCloseTo` matcher must be coercable into a
  `Double`.

## Types/Classes

```swift
// Swift

// Passes if instance is an instance of aClass:
expect(instance).to(beAnInstanceOf(aClass))

// Passes if instance is an instance of aClass or any of its subclasses:
expect(instance).to(beAKindOf(aClass))
```

```objc
// Objective-C

// Passes if instance is an instance of aClass:
expect(instance).to(beAnInstanceOf(aClass));

// Passes if instance is an instance of aClass or any of its subclasses:
expect(instance).to(beAKindOf(aClass));
```

> Instances must be Objective-C objects: subclasses of `NSObject`,
  or Swift objects bridged to Objective-C with the `@objc` prefix.

For example, to assert that `dolphin` is a kind of `Mammal`:

```swift
// Swift

expect(dolphin).to(beAKindOf(Mammal))
```

```objc
// Objective-C

expect(dolphin).to(beAKindOf([Mammal class]));
```

> `beAnInstanceOf` uses the `-[NSObject isMemberOfClass:]` method to
  test membership. `beAKindOf` uses `-[NSObject isKindOfClass:]`.

## Truthiness

```swift
// Passes if actual is not nil, true, or an object with a boolean value of true:
expect(actual).to(beTruthy())

// Passes if actual is only true (not nil or an object conforming to Boolean true):
expect(actual).to(beTrue())

// Passes if actual is nil, false, or an object with a boolean value of false:
expect(actual).to(beFalsy())

// Passes if actual is only false (not nil or an object conforming to Boolean false):
expect(actual).to(beFalse())

// Passes if actual is nil:
expect(actual).to(beNil())
```

```objc
// Objective-C

// Passes if actual is not nil, true, or an object with a boolean value of true:
expect(actual).to(beTruthy());

// Passes if actual is only true (not nil or an object conforming to Boolean true):
expect(actual).to(beTrue());

// Passes if actual is nil, false, or an object with a boolean value of false:
expect(actual).to(beFalsy());

// Passes if actual is only false (not nil or an object conforming to Boolean false):
expect(actual).to(beFalse());

// Passes if actual is nil:
expect(actual).to(beNil());
```

## Swift Assertions

If you're using Swift, you can use the `throwAssertion` matcher to check if an assertion is thrown (e.g. `fatalError()`). This is made possible by [@mattgallagher](https://github.com/mattgallagher)'s [CwlPreconditionTesting](https://github.com/mattgallagher/CwlPreconditionTesting) library.

```swift
// Swift

// Passes if somethingThatThrows() throws an assertion, such as calling fatalError() or precondition fails:
expect { () -> Void in fatalError() }.to(throwAssertion())
expect { precondition(false) }.to(throwAssertion())

// Passes if throwing a NSError is not equal to throwing an assertion:
expect { throw NSError(domain: "test", code: 0, userInfo: nil) }.toNot(throwAssertion())

// Passes if the post assertion code is not run:
var reachedPoint1 = false
var reachedPoint2 = false
expect {
    reachedPoint1 = true
    precondition(false, "condition message")
    reachedPoint2 = true
}.to(throwAssertion())

expect(reachedPoint1) == true
expect(reachedPoint2) == false
```

Notes:

* This feature is only available in Swift.
* It is only supported for `x86_64` binaries, meaning _you cannot run this matcher on iOS devices, only simulators_.
* The tvOS simulator is supported, but using a different mechanism, requiring you to turn off the `Debug executable` scheme setting for your tvOS scheme's Test configuration.

## Swift Error Handling

If you're using Swift 2.0+, you can use the `throwError` matcher to check if an error is thrown.

```swift
// Swift

// Passes if somethingThatThrows() throws an ErrorProtocol:
expect{ try somethingThatThrows() }.to(throwError())

// Passes if somethingThatThrows() throws an error with a given domain:
expect{ try somethingThatThrows() }.to(throwError { (error: ErrorProtocol) in
    expect(error._domain).to(equal(NSCocoaErrorDomain))
})

// Passes if somethingThatThrows() throws an error with a given case:
expect{ try somethingThatThrows() }.to(throwError(NSCocoaError.PropertyListReadCorruptError))

// Passes if somethingThatThrows() throws an error with a given type:
expect{ try somethingThatThrows() }.to(throwError(errorType: NimbleError.self))
```

If you are working directly with `ErrorProtocol` values, as is sometimes the case when using `Result` or `Promise` types, you can use the `matchError` matcher to check if the error is the same error is is supposed to be, without requiring explicit casting.

```swift
// Swift

let actual: ErrorProtocol = …

// Passes if actual contains any error value from the NimbleErrorEnum type:
expect(actual).to(matchError(NimbleErrorEnum))

// Passes if actual contains the Timeout value from the NimbleErrorEnum type:
expect(actual).to(matchError(NimbleErrorEnum.Timeout))

// Passes if actual contains an NSError equal to the given one:
expect(actual).to(matchError(NSError(domain: "err", code: 123, userInfo: nil)))
```

Note: This feature is only available in Swift.

## Exceptions

```swift
// Swift

// Passes if actual, when evaluated, raises an exception:
expect(actual).to(raiseException())

// Passes if actual raises an exception with the given name:
expect(actual).to(raiseException(named: name))

// Passes if actual raises an exception with the given name and reason:
expect(actual).to(raiseException(named: name, reason: reason))

// Passes if actual raises an exception and it passes expectations in the block
// (in this case, if name begins with 'a r')
expect { exception.raise() }.to(raiseException { (exception: NSException) in
    expect(exception.name).to(beginWith("a r"))
})
```

```objc
// Objective-C

// Passes if actual, when evaluated, raises an exception:
expect(actual).to(raiseException())

// Passes if actual raises an exception with the given name
expect(actual).to(raiseException().named(name))

// Passes if actual raises an exception with the given name and reason:
expect(actual).to(raiseException().named(name).reason(reason))

// Passes if actual raises an exception and it passes expectations in the block
// (in this case, if name begins with 'a r')
expect(actual).to(raiseException().satisfyingBlock(^(NSException *exception) {
    expect(exception.name).to(beginWith(@"a r"));
}));
```

Note: Swift currently doesn't have exceptions (see [#220](https://github.com/Quick/Nimble/issues/220#issuecomment-172667064)). Only Objective-C code can raise
exceptions that Nimble will catch.

## Collection Membership

```swift
// Swift

// Passes if all of the expected values are members of actual:
expect(actual).to(contain(expected...))

// Passes if actual is an empty collection (it contains no elements):
expect(actual).to(beEmpty())
```

```objc
// Objective-C

// Passes if expected is a member of actual:
expect(actual).to(contain(expected));

// Passes if actual is an empty collection (it contains no elements):
expect(actual).to(beEmpty());
```

> In Swift `contain` takes any number of arguments. The expectation
  passes if all of them are members of the collection. In Objective-C,
  `contain` only takes one argument [for now](https://github.com/Quick/Nimble/issues/27).

For example, to assert that a list of sea creature names contains
"dolphin" and "starfish":

```swift
// Swift

expect(["whale", "dolphin", "starfish"]).to(contain("dolphin", "starfish"))
```

```objc
// Objective-C

expect(@[@"whale", @"dolphin", @"starfish"]).to(contain(@"dolphin"));
expect(@[@"whale", @"dolphin", @"starfish"]).to(contain(@"starfish"));
```

> `contain` and `beEmpty` expect collections to be instances of
  `NSArray`, `NSSet`, or a Swift collection composed of `Equatable` elements.

To test whether a set of elements is present at the beginning or end of
an ordered collection, use `beginWith` and `endWith`:

```swift
// Swift

// Passes if the elements in expected appear at the beginning of actual:
expect(actual).to(beginWith(expected...))

// Passes if the the elements in expected come at the end of actual:
expect(actual).to(endWith(expected...))
```

```objc
// Objective-C

// Passes if the elements in expected appear at the beginning of actual:
expect(actual).to(beginWith(expected));

// Passes if the the elements in expected come at the end of actual:
expect(actual).to(endWith(expected));
```

> `beginWith` and `endWith` expect collections to be instances of
  `NSArray`, or ordered Swift collections composed of `Equatable`
  elements.

  Like `contain`, in Objective-C `beginWith` and `endWith` only support
  a single argument [for now](https://github.com/Quick/Nimble/issues/27).

## Strings

```swift
// Swift

// Passes if actual contains substring expected:
expect(actual).to(contain(expected))

// Passes if actual begins with substring:
expect(actual).to(beginWith(expected))

// Passes if actual ends with substring:
expect(actual).to(endWith(expected))

// Passes if actual is an empty string, "":
expect(actual).to(beEmpty())

// Passes if actual matches the regular expression defined in expected:
expect(actual).to(match(expected))
```

```objc
// Objective-C

// Passes if actual contains substring expected:
expect(actual).to(contain(expected));

// Passes if actual begins with substring:
expect(actual).to(beginWith(expected));

// Passes if actual ends with substring:
expect(actual).to(endWith(expected));

// Passes if actual is an empty string, "":
expect(actual).to(beEmpty());

// Passes if actual matches the regular expression defined in expected:
expect(actual).to(match(expected))
```

## Checking if all elements of a collection pass a condition

```swift
// Swift

// with a custom function:
expect([1,2,3,4]).to(allPass({$0 < 5}))

// with another matcher:
expect([1,2,3,4]).to(allPass(beLessThan(5)))
```

```objc
// Objective-C

expect(@[@1, @2, @3,@4]).to(allPass(beLessThan(@5)));
```

For Swift the actual value has to be a Sequence, e.g. an array, a set or a custom seqence type.

For Objective-C the actual value has to be a NSFastEnumeration, e.g. NSArray and NSSet, of NSObjects and only the variant which
uses another matcher is available here.

## Verify collection count

```swift
// Swift

// passes if actual collection's count is equal to expected
expect(actual).to(haveCount(expected))

// passes if actual collection's count is not equal to expected
expect(actual).notTo(haveCount(expected))
```

```objc
// Objective-C

// passes if actual collection's count is equal to expected
expect(actual).to(haveCount(expected))

// passes if actual collection's count is not equal to expected
expect(actual).notTo(haveCount(expected))
```

For Swift the actual value must be a `Collection` such as array, dictionary or set.

For Objective-C the actual value has to be one of the following classes `NSArray`, `NSDictionary`, `NSSet`, `NSHashTable` or one of their subclasses.

## Foundation

### Verifying a Notification was posted

```swift
// Swift
let testNotification = Notification(name: "Foo", object: nil)

// passes if the closure in expect { ... } posts a notification to the default
// notification center.
expect {
    NotificationCenter.default.postNotification(testNotification)
}.to(postNotifications(equal([testNotification]))

// passes if the closure in expect { ... } posts a notification to a given
// notification center
let notificationCenter = NotificationCenter()
expect {
    notificationCenter.postNotification(testNotification)
}.to(postNotifications(equal([testNotification]), fromNotificationCenter: notificationCenter))
```

> This matcher is only available in Swift.

## Matching a value to any of a group of matchers

```swift
// passes if actual is either less than 10 or greater than 20
expect(actual).to(satisfyAnyOf(beLessThan(10), beGreaterThan(20)))

// can include any number of matchers -- the following will pass
// **be careful** -- too many matchers can be the sign of an unfocused test
expect(6).to(satisfyAnyOf(equal(2), equal(3), equal(4), equal(5), equal(6), equal(7)))

// in Swift you also have the option to use the || operator to achieve a similar function
expect(82).to(beLessThan(50) || beGreaterThan(80))
```

```objc
// passes if actual is either less than 10 or greater than 20
expect(actual).to(satisfyAnyOf(beLessThan(@10), beGreaterThan(@20)))

// can include any number of matchers -- the following will pass
// **be careful** -- too many matchers can be the sign of an unfocused test
expect(@6).to(satisfyAnyOf(equal(@2), equal(@3), equal(@4), equal(@5), equal(@6), equal(@7)))
```

Note: This matcher allows you to chain any number of matchers together. This provides flexibility,
      but if you find yourself chaining many matchers together in one test, consider whether you
      could instead refactor that single test into multiple, more precisely focused tests for
      better coverage.

# Writing Your Own Matchers

In Nimble, matchers are Swift functions that take an expected
value and return a `MatcherFunc` closure. Take `equal`, for example:

```swift
// Swift

public func equal<T: Equatable>(expectedValue: T?) -> MatcherFunc<T?> {
  return MatcherFunc { actualExpression, failureMessage in
    failureMessage.postfixMessage = "equal <\(expectedValue)>"
    if let actualValue = try actualExpression.evaluate() {
    	return actualValue == expectedValue
    } else {
    	return false
    }
  }
}
```

The return value of a `MatcherFunc` closure is a `Bool` that indicates
whether the actual value matches the expectation: `true` if it does, or
`false` if it doesn't.

> The actual `equal` matcher function does not match when either
  `actual` or `expected` are nil; the example above has been edited for
  brevity.

Since matchers are just Swift functions, you can define them anywhere:
at the top of your test file, in a file shared by all of your tests, or
in an Xcode project you distribute to others.

> If you write a matcher you think everyone can use, consider adding it
  to Nimble's built-in set of matchers by sending a pull request! Or
  distribute it yourself via GitHub.

For examples of how to write your own matchers, just check out the
[`Matchers` directory](https://github.com/Quick/Nimble/tree/master/Sources/Nimble/Matchers)
to see how Nimble's built-in set of matchers are implemented. You can
also check out the tips below.

## Lazy Evaluation

`actualExpression` is a lazy, memoized closure around the value provided to the
`expect` function. The expression can either be a closure or a value directly
passed to `expect(...)`. In order to determine whether that value matches,
custom matchers should call `actualExpression.evaluate()`:

```swift
// Swift

public func beNil<T>() -> MatcherFunc<T?> {
  return MatcherFunc { actualExpression, failureMessage in
    failureMessage.postfixMessage = "be nil"
    return actualExpression.evaluate() == nil
  }
}
```

In the above example, `actualExpression` is not `nil`--it is a closure
that returns a value. The value it returns, which is accessed via the
`evaluate()` method, may be `nil`. If that value is `nil`, the `beNil`
matcher function returns `true`, indicating that the expectation passed.

Use `expression.isClosure` to determine if the expression will be invoking
a closure to produce its value.

## Type Checking via Swift Generics

Using Swift's generics, matchers can constrain the type of the actual value
passed to the `expect` function by modifying the return type.

For example, the following matcher, `haveDescription`, only accepts actual
values that implement the `Printable` protocol. It checks their `description`
against the one provided to the matcher function, and passes if they are the same:

```swift
// Swift

public func haveDescription(description: String) -> MatcherFunc<Printable?> {
  return MatcherFunc { actual, failureMessage in
    return actual.evaluate().description == description
  }
}
```

## Customizing Failure Messages

By default, Nimble outputs the following failure message when an
expectation fails:

```
expected to match, got <\(actual)>
```

You can customize this message by modifying the `failureMessage` struct
from within your `MatcherFunc` closure. To change the verb "match" to
something else, update the `postfixMessage` property:

```swift
// Swift

// Outputs: expected to be under the sea, got <\(actual)>
failureMessage.postfixMessage = "be under the sea"
```

You can change how the `actual` value is displayed by updating
`failureMessage.actualValue`. Or, to remove it altogether, set it to
`nil`:

```swift
// Swift

// Outputs: expected to be under the sea
failureMessage.actualValue = nil
failureMessage.postfixMessage = "be under the sea"
```

## Supporting Objective-C

To use a custom matcher written in Swift from Objective-C, you'll have
to extend the `NMBObjCMatcher` class, adding a new class method for your
custom matcher. The example below defines the class method
`+[NMBObjCMatcher beNilMatcher]`:

```swift
// Swift

extension NMBObjCMatcher {
  public class func beNilMatcher() -> NMBObjCMatcher {
    return NMBObjCMatcher { actualBlock, failureMessage, location in
      let block = ({ actualBlock() as NSObject? })
      let expr = Expression(expression: block, location: location)
      return beNil().matches(expr, failureMessage: failureMessage)
    }
  }
}
```

The above allows you to use the matcher from Objective-C:

```objc
// Objective-C

expect(actual).to([NMBObjCMatcher beNilMatcher]());
```

To make the syntax easier to use, define a C function that calls the
class method:

```objc
// Objective-C

FOUNDATION_EXPORT id<NMBMatcher> beNil() {
  return [NMBObjCMatcher beNilMatcher];
}
```

### Properly Handling `nil` in Objective-C Matchers

When supporting Objective-C, make sure you handle `nil` appropriately.
Like [Cedar](https://github.com/pivotal/cedar/issues/100),
**most matchers do not match with nil**. This is to bring prevent test
writers from being surprised by `nil` values where they did not expect
them.

Nimble provides the `beNil` matcher function for test writer that want
to make expectations on `nil` objects:

```objc
// Objective-C

expect(nil).to(equal(nil)); // fails
expect(nil).to(beNil());    // passes
```

If your matcher does not want to match with nil, you use `NonNilMatcherFunc`
and the `canMatchNil` constructor on `NMBObjCMatcher`. Using both types will
automatically generate expected value failure messages when they're nil.

```swift

public func beginWith<S: Sequence, T: Equatable where S.Iterator.Element == T>(startingElement: T) -> NonNilMatcherFunc<S> {
    return NonNilMatcherFunc { actualExpression, failureMessage in
        failureMessage.postfixMessage = "begin with <\(startingElement)>"
        if let actualValue = actualExpression.evaluate() {
            var actualGenerator = actualValue.makeIterator()
            return actualGenerator.next() == startingElement
        }
        return false
    }
}

extension NMBObjCMatcher {
    public class func beginWithMatcher(expected: AnyObject) -> NMBObjCMatcher {
        return NMBObjCMatcher(canMatchNil: false) { actualExpression, failureMessage in
            let actual = actualExpression.evaluate()
            let expr = actualExpression.cast { $0 as? NMBOrderedCollection }
            return beginWith(expected).matches(expr, failureMessage: failureMessage)
        }
    }
}
```

# Installing Nimble

> Nimble can be used on its own, or in conjunction with its sister
  project, [Quick](https://github.com/Quick/Quick). To install both
  Quick and Nimble, follow [the installation instructions in the Quick
  Documentation](https://github.com/Quick/Quick/blob/master/Documentation/en-us/InstallingQuick.md).

Nimble can currently be installed in one of two ways: using CocoaPods, or with
git submodules.

## Installing Nimble as a Submodule

To use Nimble as a submodule to test your macOS, iOS or tvOS applications, follow
these 4 easy steps:

1. Clone the Nimble repository
2. Add Nimble.xcodeproj to the Xcode workspace for your project
3. Link Nimble.framework to your test target
4. Start writing expectations!

For more detailed instructions on each of these steps,
read [How to Install Quick](https://github.com/Quick/Quick#how-to-install-quick).
Ignore the steps involving adding Quick to your project in order to
install just Nimble.

## Installing Nimble via CocoaPods

To use Nimble in CocoaPods to test your macOS, iOS or tvOS applications, add
Nimble to your podfile and add the ```use_frameworks!``` line to enable Swift
support for CocoaPods.

```ruby
platform :ios, '8.0'

source 'https://github.com/CocoaPods/Specs.git'

# Whatever pods you need for your app go here

target 'YOUR_APP_NAME_HERE_Tests', :exclusive => true do
  use_frameworks!
  pod 'Nimble', '~> 5.0.0'
end
```

Finally run `pod install`.

## Using Nimble without XCTest

Nimble is integrated with XCTest to allow it work well when used in Xcode test
bundles, however it can also be used in a standalone app. After installing
Nimble using one of the above methods, there are two additional steps required
to make this work.

1. Create a custom assertion handler and assign an instance of it to the
   global `NimbleAssertionHandler` variable. For example:

```swift
class MyAssertionHandler : AssertionHandler {
    func assert(assertion: Bool, message: FailureMessage, location: SourceLocation) {
        if (!assertion) {
            print("Expectation failed: \(message.stringValue)")
        }
    }
}
```
```swift
// Somewhere before you use any assertions
NimbleAssertionHandler = MyAssertionHandler()
```

2. Add a post-build action to fix an issue with the Swift XCTest support
   library being unnecessarily copied into your app
  * Edit your scheme in Xcode, and navigate to Build -> Post-actions
  * Click the "+" icon and select "New Run Script Action"
  * Open the "Provide build settings from" dropdown and select your target
  * Enter the following script contents:
```
rm "${SWIFT_STDLIB_TOOL_DESTINATION_DIR}/libswiftXCTest.dylib"
```

You can now use Nimble assertions in your code and handle failures as you see
fit.

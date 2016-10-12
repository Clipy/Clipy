//  PINCache is a modified version of TMCache
//  Modifications by Garrett Moon
//  Copyright (c) 2015 Pinterest. All rights reserved.

#import <Foundation/Foundation.h>

#import "PINDiskCache.h"
#import "PINMemoryCache.h"

NS_ASSUME_NONNULL_BEGIN

@class PINCache;

/**
 A callback block which provides only the cache as an argument
 */

typedef void (^PINCacheBlock)(PINCache *cache);

/**
 A callback block which provides the cache, key and object as arguments
 */

typedef void (^PINCacheObjectBlock)(PINCache *cache, NSString *key, id __nullable object);

/**
 `PINCache` is a thread safe key/value store designed for persisting temporary objects that are expensive to
 reproduce, such as downloaded data or the results of slow processing. It is comprised of two self-similar
 stores, one in memory (<PINMemoryCache>) and one on disk (<PINDiskCache>).
 
 `PINCache` itself actually does very little; its main function is providing a front end for a common use case:
 a small, fast memory cache that asynchronously persists itself to a large, slow disk cache. When objects are
 removed from the memory cache in response to an "apocalyptic" event they remain in the disk cache and are
 repopulated in memory the next time they are accessed. `PINCache` also does the tedious work of creating a
 dispatch group to wait for both caches to finish their operations without blocking each other.
 
 The parallel caches are accessible as public properties (<memoryCache> and <diskCache>) and can be manipulated
 separately if necessary. See the docs for <PINMemoryCache> and <PINDiskCache> for more details.

 @warning when using in extension or watch extension, define PIN_APP_EXTENSIONS=1
 */

@interface PINCache : NSObject

#pragma mark -
/// @name Core

/**
 The name of this cache, used to create the <diskCache> and also appearing in stack traces.
 */
@property (readonly) NSString *name;

/**
 A concurrent queue on which blocks passed to the asynchronous access methods are run.
 */
@property (readonly) dispatch_queue_t concurrentQueue;

/**
 Synchronously retrieves the total byte count of the <diskCache> on the shared disk queue.
 */
@property (readonly) NSUInteger diskByteCount;

/**
 The underlying disk cache, see <PINDiskCache> for additional configuration and trimming options.
 */
@property (readonly) PINDiskCache *diskCache;

/**
 The underlying memory cache, see <PINMemoryCache> for additional configuration and trimming options.
 */
@property (readonly) PINMemoryCache *memoryCache;

#pragma mark -
/// @name Initialization

/**
 A shared cache.
 
 @result The shared singleton cache instance.
 */
+ (instancetype)sharedCache;

- (instancetype)init NS_UNAVAILABLE;

/**
 Multiple instances with the same name are allowed and can safely access
 the same data on disk thanks to the magic of seriality. Also used to create the <diskCache>.
 
 @see name
 @param name The name of the cache.
 @result A new cache with the specified name.
 */
- (instancetype)initWithName:(NSString *)name;

/**
 Multiple instances with the same name are allowed and can safely access
 the same data on disk thanks to the magic of seriality. Also used to create the <diskCache>.
 
 @see name
 @param name The name of the cache.
 @param rootPath The path of the cache on disk.
 @result A new cache with the specified name.
 */
- (instancetype)initWithName:(NSString *)name rootPath:(NSString *)rootPath NS_DESIGNATED_INITIALIZER;

#pragma mark -
/// @name Asynchronous Methods

/**
 Retrieves the object for the specified key. This method returns immediately and executes the passed
 block after the object is available, potentially in parallel with other blocks on the <concurrentQueue>.
 
 @param key The key associated with the requested object.
 @param block A block to be executed concurrently when the object is available.
 */
- (void)objectForKey:(NSString *)key block:(PINCacheObjectBlock)block;

/**
 Stores an object in the cache for the specified key. This method returns immediately and executes the
 passed block after the object has been stored, potentially in parallel with other blocks on the <concurrentQueue>.
 
 @param object An object to store in the cache.
 @param key A key to associate with the object. This string will be copied.
 @param block A block to be executed concurrently after the object has been stored, or nil.
 */
- (void)setObject:(id <NSCoding>)object forKey:(NSString *)key block:(nullable PINCacheObjectBlock)block;

/**
 Removes the object for the specified key. This method returns immediately and executes the passed
 block after the object has been removed, potentially in parallel with other blocks on the <concurrentQueue>.
 
 @param key The key associated with the object to be removed.
 @param block A block to be executed concurrently after the object has been removed, or nil.
 */
- (void)removeObjectForKey:(NSString *)key block:(nullable PINCacheObjectBlock)block;

/**
 Removes all objects from the cache that have not been used since the specified date. This method returns immediately and
 executes the passed block after the cache has been trimmed, potentially in parallel with other blocks on the <concurrentQueue>.
 
 @param date Objects that haven't been accessed since this date are removed from the cache.
 @param block A block to be executed concurrently after the cache has been trimmed, or nil.
 */
- (void)trimToDate:(NSDate *)date block:(nullable PINCacheBlock)block;

/**
 Removes all objects from the cache.This method returns immediately and executes the passed block after the
 cache has been cleared, potentially in parallel with other blocks on the <concurrentQueue>.
 
 @param block A block to be executed concurrently after the cache has been cleared, or nil.
 */
- (void)removeAllObjects:(nullable PINCacheBlock)block;

#pragma mark -
/// @name Synchronous Methods

/**
 Retrieves the object for the specified key. This method blocks the calling thread until the object is available.
 Uses a semaphore to achieve synchronicity on the disk cache.
 
 @see objectForKey:block:
 @param key The key associated with the object.
 @result The object for the specified key.
 */
- (__nullable id)objectForKey:(NSString *)key;

/**
 Stores an object in the cache for the specified key. This method blocks the calling thread until the object has been set.
 Uses a semaphore to achieve synchronicity on the disk cache.
 
 @see setObject:forKey:block:
 @param object An object to store in the cache.
 @param key A key to associate with the object. This string will be copied.
 */
- (void)setObject:(id <NSCoding>)object forKey:(NSString *)key;

/**
 Removes the object for the specified key. This method blocks the calling thread until the object
 has been removed.
 Uses a semaphore to achieve synchronicity on the disk cache.
 
 @param key The key associated with the object to be removed.
 */
- (void)removeObjectForKey:(NSString *)key;

/**
 Removes all objects from the cache that have not been used since the specified date.
 This method blocks the calling thread until the cache has been trimmed.
 Uses a semaphore to achieve synchronicity on the disk cache.
 
 @param date Objects that haven't been accessed since this date are removed from the cache.
 */
- (void)trimToDate:(NSDate *)date;

/**
 Removes all objects from the cache. This method blocks the calling thread until the cache has been cleared.
 Uses a semaphore to achieve synchronicity on the disk cache.
 */
- (void)removeAllObjects;

@end

NS_ASSUME_NONNULL_END

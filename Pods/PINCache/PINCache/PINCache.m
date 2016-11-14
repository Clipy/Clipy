//  PINCache is a modified version of PINCache
//  Modifications by Garrett Moon
//  Copyright (c) 2015 Pinterest. All rights reserved.

#import "PINCache.h"

static NSString * const PINCachePrefix = @"com.pinterest.PINCache";
static NSString * const PINCacheSharedName = @"PINCacheShared";

@interface PINCache ()
#if OS_OBJECT_USE_OBJC
@property (strong, nonatomic) dispatch_queue_t concurrentQueue;
#else
@property (assign, nonatomic) dispatch_queue_t concurrentQueue;
#endif
@end

@implementation PINCache

#pragma mark - Initialization -

#if !OS_OBJECT_USE_OBJC
- (void)dealloc
{
    dispatch_release(_concurrentQueue);
    _concurrentQueue = nil;
}
#endif

- (instancetype)init
{
    @throw [NSException exceptionWithName:@"Must initialize with a name" reason:@"PINCache must be initialized with a name. Call initWithName: instead." userInfo:nil];
    return [self initWithName:@""];
}

- (instancetype)initWithName:(NSString *)name
{
    return [self initWithName:name rootPath:[NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES) firstObject]];
}

- (instancetype)initWithName:(NSString *)name rootPath:(NSString *)rootPath
{
    if (!name)
        return nil;
    
    if (self = [super init]) {
        _name = [name copy];
        
        NSString *queueName = [[NSString alloc] initWithFormat:@"%@.%p", PINCachePrefix, (void *)self];
        _concurrentQueue = dispatch_queue_create([[NSString stringWithFormat:@"%@ Asynchronous Queue", queueName] UTF8String], DISPATCH_QUEUE_CONCURRENT);
        
        _diskCache = [[PINDiskCache alloc] initWithName:_name rootPath:rootPath];
        _memoryCache = [[PINMemoryCache alloc] init];
    }
    return self;
}

- (NSString *)description
{
    return [[NSString alloc] initWithFormat:@"%@.%@.%p", PINCachePrefix, _name, (void *)self];
}

+ (instancetype)sharedCache
{
    static id cache;
    static dispatch_once_t predicate;
    
    dispatch_once(&predicate, ^{
        cache = [[self alloc] initWithName:PINCacheSharedName];
    });
    
    return cache;
}

#pragma mark - Public Asynchronous Methods -

- (void)containsObjectForKey:(NSString *)key block:(PINCacheObjectContainmentBlock)block
{
    if (!key || !block) {
        return;
    }
    
    __weak PINCache *weakSelf = self;
    
    dispatch_async(_concurrentQueue, ^{
        PINCache *strongSelf = weakSelf;
        
        BOOL containsObject = [strongSelf containsObjectForKey:key];
        block(containsObject);
    });
}

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wshadow"

- (void)objectForKey:(NSString *)key block:(PINCacheObjectBlock)block
{
    if (!key || !block)
        return;
    
    __weak PINCache *weakSelf = self;
    
    dispatch_async(_concurrentQueue, ^{
        PINCache *strongSelf = weakSelf;
        if (!strongSelf)
            return;
        [strongSelf->_memoryCache objectForKey:key block:^(PINMemoryCache *memoryCache, NSString *memoryCacheKey, id memoryCacheObject) {
            PINCache *strongSelf = weakSelf;
            if (!strongSelf)
                return;
            
            if (memoryCacheObject) {
                [strongSelf->_diskCache fileURLForKey:memoryCacheKey block:^(PINDiskCache *diskCache, NSString *diskCacheKey, id <NSCoding> diskCacheObject, NSURL *fileURL) {
                    // update the access time on disk
                }];
                dispatch_async(strongSelf->_concurrentQueue, ^{
                    PINCache *strongSelf = weakSelf;
                    if (strongSelf)
                        block(strongSelf, memoryCacheKey, memoryCacheObject);
                });
            } else {
                [strongSelf->_diskCache objectForKey:memoryCacheKey block:^(PINDiskCache *diskCache, NSString *diskCacheKey, id <NSCoding> diskCacheObject, NSURL *fileURL) {
                    PINCache *strongSelf = weakSelf;
                    if (!strongSelf)
                        return;
                    
                    [strongSelf->_memoryCache setObject:diskCacheObject forKey:diskCacheKey block:nil];
                    

                    dispatch_async(strongSelf->_concurrentQueue, ^{
                        PINCache *strongSelf = weakSelf;
                        if (strongSelf)
                            block(strongSelf, diskCacheKey, diskCacheObject);
                    });
                }];
            }
        }];
    });
}

#pragma clang diagnostic pop

- (void)setObject:(id <NSCoding>)object forKey:(NSString *)key block:(PINCacheObjectBlock)block
{
    if (!key || !object)
        return;
    
    dispatch_group_t group = nil;
    PINMemoryCacheObjectBlock memBlock = nil;
    PINDiskCacheObjectBlock diskBlock = nil;
    
    if (block) {
        group = dispatch_group_create();
        dispatch_group_enter(group);
        dispatch_group_enter(group);
        
        memBlock = ^(PINMemoryCache *memoryCache, NSString *memoryCacheKey, id memoryCacheObject) {
            dispatch_group_leave(group);
        };
        
        diskBlock = ^(PINDiskCache *diskCache, NSString *diskCacheKey, id <NSCoding> memoryCacheObject, NSURL *memoryCacheFileURL) {
            dispatch_group_leave(group);
        };
    }
    
    [_memoryCache setObject:object forKey:key block:memBlock];
    [_diskCache setObject:object forKey:key block:diskBlock];
    
    if (group) {
        __weak PINCache *weakSelf = self;
        dispatch_group_notify(group, _concurrentQueue, ^{
            PINCache *strongSelf = weakSelf;
            if (strongSelf)
                block(strongSelf, key, object);
        });
        
#if !OS_OBJECT_USE_OBJC
        dispatch_release(group);
#endif
    }
}

- (void)removeObjectForKey:(NSString *)key block:(PINCacheObjectBlock)block
{
    if (!key)
        return;
    
    dispatch_group_t group = nil;
    PINMemoryCacheObjectBlock memBlock = nil;
    PINDiskCacheObjectBlock diskBlock = nil;
    
    if (block) {
        group = dispatch_group_create();
        dispatch_group_enter(group);
        dispatch_group_enter(group);
        
        memBlock = ^(PINMemoryCache *memoryCache, NSString *memoryCacheKey, id memoryCacheObject) {
            dispatch_group_leave(group);
        };
        
        diskBlock = ^(PINDiskCache *diskCache, NSString *diskCacheKey, id <NSCoding> memoryCacheObject, NSURL *memoryCacheFileURL) {
            dispatch_group_leave(group);
        };
    }
    
    [_memoryCache removeObjectForKey:key block:memBlock];
    [_diskCache removeObjectForKey:key block:diskBlock];
    
    if (group) {
        __weak PINCache *weakSelf = self;
        dispatch_group_notify(group, _concurrentQueue, ^{
            PINCache *strongSelf = weakSelf;
            if (strongSelf)
                block(strongSelf, key, nil);
        });
        
#if !OS_OBJECT_USE_OBJC
        dispatch_release(group);
#endif
    }
}

- (void)removeAllObjects:(PINCacheBlock)block
{
    dispatch_group_t group = nil;
    PINMemoryCacheBlock memBlock = nil;
    PINDiskCacheBlock diskBlock = nil;
    
    if (block) {
        group = dispatch_group_create();
        dispatch_group_enter(group);
        dispatch_group_enter(group);
        
        memBlock = ^(PINMemoryCache *cache) {
            dispatch_group_leave(group);
        };
        
        diskBlock = ^(PINDiskCache *cache) {
            dispatch_group_leave(group);
        };
    }
    
    [_memoryCache removeAllObjects:memBlock];
    [_diskCache removeAllObjects:diskBlock];
    
    if (group) {
        __weak PINCache *weakSelf = self;
        dispatch_group_notify(group, _concurrentQueue, ^{
            PINCache *strongSelf = weakSelf;
            if (strongSelf)
                block(strongSelf);
        });
        
#if !OS_OBJECT_USE_OBJC
        dispatch_release(group);
#endif
    }
}

- (void)trimToDate:(NSDate *)date block:(PINCacheBlock)block
{
    if (!date)
        return;
    
    dispatch_group_t group = nil;
    PINMemoryCacheBlock memBlock = nil;
    PINDiskCacheBlock diskBlock = nil;
    
    if (block) {
        group = dispatch_group_create();
        dispatch_group_enter(group);
        dispatch_group_enter(group);
        
        memBlock = ^(PINMemoryCache *cache) {
            dispatch_group_leave(group);
        };
        
        diskBlock = ^(PINDiskCache *cache) {
            dispatch_group_leave(group);
        };
    }
    
    [_memoryCache trimToDate:date block:memBlock];
    [_diskCache trimToDate:date block:diskBlock];
    
    if (group) {
        __weak PINCache *weakSelf = self;
        dispatch_group_notify(group, _concurrentQueue, ^{
            PINCache *strongSelf = weakSelf;
            if (strongSelf)
                block(strongSelf);
        });
        
#if !OS_OBJECT_USE_OBJC
        dispatch_release(group);
#endif
    }
}

#pragma mark - Public Synchronous Accessors -

- (NSUInteger)diskByteCount
{
    __block NSUInteger byteCount = 0;
    
    [_diskCache synchronouslyLockFileAccessWhileExecutingBlock:^(PINDiskCache *diskCache) {
        byteCount = diskCache.byteCount;
    }];
    
    return byteCount;
}

- (BOOL)containsObjectForKey:(NSString *)key
{
    if (!key)
        return NO;
    
    return [_memoryCache containsObjectForKey:key] || [_diskCache containsObjectForKey:key];
}

- (__nullable id)objectForKey:(NSString *)key
{
    if (!key)
        return nil;
    
    __block id object = nil;

    object = [_memoryCache objectForKey:key];
    
    if (object) {
        // update the access time on disk
        [_diskCache fileURLForKey:key block:NULL];
    } else {
        object = [_diskCache objectForKey:key];
        [_memoryCache setObject:object forKey:key];
    }
    
    return object;
}

- (void)setObject:(id <NSCoding>)object forKey:(NSString *)key
{
    if (!key || !object)
        return;
    
    [_memoryCache setObject:object forKey:key];
    [_diskCache setObject:object forKey:key];
}

- (id)objectForKeyedSubscript:(NSString *)key
{
    return [self objectForKey:key];
}

- (void)setObject:(id)obj forKeyedSubscript:(NSString *)key
{
    [self setObject:obj forKey:key];
}

- (void)removeObjectForKey:(NSString *)key
{
    if (!key)
        return;
    
    [_memoryCache removeObjectForKey:key];
    [_diskCache removeObjectForKey:key];
}

- (void)trimToDate:(NSDate *)date
{
    if (!date)
        return;
    
    [_memoryCache trimToDate:date];
    [_diskCache trimToDate:date];
}

- (void)removeAllObjects
{
    [_memoryCache removeAllObjects];
    [_diskCache removeAllObjects];
}

@end

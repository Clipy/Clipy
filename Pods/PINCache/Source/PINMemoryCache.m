//  PINCache is a modified version of TMCache
//  Modifications by Garrett Moon
//  Copyright (c) 2015 Pinterest. All rights reserved.

#import "PINMemoryCache.h"

#import <pthread.h>

#if SWIFT_PACKAGE
@import PINOperation;
#else
#import <PINOperation/PINOperation.h>
#endif

#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_4_0
#import <UIKit/UIKit.h>
#endif

static NSString * const PINMemoryCachePrefix = @"com.pinterest.PINMemoryCache";
static NSString * const PINMemoryCacheSharedName = @"PINMemoryCacheSharedName";

@interface PINMemoryCache ()
@property (copy, nonatomic) NSString *name;
@property (strong, nonatomic) PINOperationQueue *operationQueue;
@property (assign, nonatomic) pthread_mutex_t mutex;
@property (strong, nonatomic) NSMutableDictionary *dictionary;
@property (strong, nonatomic) NSMutableDictionary *createdDates;
@property (strong, nonatomic) NSMutableDictionary *accessDates;
@property (strong, nonatomic) NSMutableDictionary *costs;
@property (strong, nonatomic) NSMutableDictionary *ageLimits;
@end

@implementation PINMemoryCache

@synthesize name = _name;
@synthesize ageLimit = _ageLimit;
@synthesize costLimit = _costLimit;
@synthesize totalCost = _totalCost;
@synthesize ttlCache = _ttlCache;
@synthesize willAddObjectBlock = _willAddObjectBlock;
@synthesize willRemoveObjectBlock = _willRemoveObjectBlock;
@synthesize willRemoveAllObjectsBlock = _willRemoveAllObjectsBlock;
@synthesize didAddObjectBlock = _didAddObjectBlock;
@synthesize didRemoveObjectBlock = _didRemoveObjectBlock;
@synthesize didRemoveAllObjectsBlock = _didRemoveAllObjectsBlock;
@synthesize didReceiveMemoryWarningBlock = _didReceiveMemoryWarningBlock;
@synthesize didEnterBackgroundBlock = _didEnterBackgroundBlock;

#pragma mark - Initialization -

- (void)dealloc
{
    [[NSNotificationCenter defaultCenter] removeObserver:self];

    __unused int result = pthread_mutex_destroy(&_mutex);
    NSCAssert(result == 0, @"Failed to destroy lock in PINMemoryCache %p. Code: %d", (void *)self, result);
}

- (instancetype)init
{
    return [self initWithOperationQueue:[PINOperationQueue sharedOperationQueue]];
}

- (instancetype)initWithOperationQueue:(PINOperationQueue *)operationQueue
{
    return [self initWithName:PINMemoryCacheSharedName operationQueue:operationQueue];
}

- (instancetype)initWithName:(NSString *)name operationQueue:(PINOperationQueue *)operationQueue
{
    return [self initWithName:name operationQueue:operationQueue ttlCache:NO];
}

- (instancetype)initWithName:(NSString *)name operationQueue:(PINOperationQueue *)operationQueue ttlCache:(BOOL)ttlCache
{
    if (self = [super init]) {
        __unused int result = pthread_mutex_init(&_mutex, NULL);
        NSAssert(result == 0, @"Failed to init lock in PINMemoryCache %@. Code: %d", self, result);
        
        _name = [name copy];
        _operationQueue = operationQueue;
        _ttlCache = ttlCache;
        
        _dictionary = [[NSMutableDictionary alloc] init];
        _createdDates = [[NSMutableDictionary alloc] init];
        _accessDates = [[NSMutableDictionary alloc] init];
        _costs = [[NSMutableDictionary alloc] init];
        _ageLimits = [[NSMutableDictionary alloc] init];
        
        _willAddObjectBlock = nil;
        _willRemoveObjectBlock = nil;
        _willRemoveAllObjectsBlock = nil;
        
        _didAddObjectBlock = nil;
        _didRemoveObjectBlock = nil;
        _didRemoveAllObjectsBlock = nil;
        
        _didReceiveMemoryWarningBlock = nil;
        _didEnterBackgroundBlock = nil;
        
        _ageLimit = 0.0;
        _costLimit = 0;
        _totalCost = 0;
        
        _removeAllObjectsOnMemoryWarning = YES;
        _removeAllObjectsOnEnteringBackground = YES;
        
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_4_0 && !TARGET_OS_WATCH
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(didReceiveEnterBackgroundNotification:)
                                                     name:UIApplicationDidEnterBackgroundNotification
                                                   object:nil];
        [[NSNotificationCenter defaultCenter] addObserver:self
                                                 selector:@selector(didReceiveMemoryWarningNotification:)
                                                     name:UIApplicationDidReceiveMemoryWarningNotification
                                                   object:nil];
        
#endif
    }
    return self;
}

+ (PINMemoryCache *)sharedCache
{
    static PINMemoryCache *cache;
    static dispatch_once_t predicate;

    dispatch_once(&predicate, ^{
        cache = [[PINMemoryCache alloc] init];
    });

    return cache;
}

#pragma mark - Private Methods -

- (void)didReceiveMemoryWarningNotification:(NSNotification *)notification {
    if (self.removeAllObjectsOnMemoryWarning) {
        [self removeAllObjectsAsync:nil];
    } else {
        [self removeExpiredObjects];
    }

    [self.operationQueue scheduleOperation:^{
        [self lock];
            PINCacheBlock didReceiveMemoryWarningBlock = self->_didReceiveMemoryWarningBlock;
        [self unlock];
        
        if (didReceiveMemoryWarningBlock)
            didReceiveMemoryWarningBlock(self);
    } withPriority:PINOperationQueuePriorityHigh];
}

- (void)didReceiveEnterBackgroundNotification:(NSNotification *)notification
{
    if (self.removeAllObjectsOnEnteringBackground)
        [self removeAllObjectsAsync:nil];

    [self.operationQueue scheduleOperation:^{
        [self lock];
            PINCacheBlock didEnterBackgroundBlock = self->_didEnterBackgroundBlock;
        [self unlock];

        if (didEnterBackgroundBlock)
            didEnterBackgroundBlock(self);
    } withPriority:PINOperationQueuePriorityHigh];
}

- (void)removeObjectAndExecuteBlocksForKey:(NSString *)key
{
    [self lock];
        id object = _dictionary[key];
        NSNumber *cost = _costs[key];
        PINCacheObjectBlock willRemoveObjectBlock = _willRemoveObjectBlock;
        PINCacheObjectBlock didRemoveObjectBlock = _didRemoveObjectBlock;
    [self unlock];

    if (willRemoveObjectBlock)
        willRemoveObjectBlock(self, key, object);

    [self lock];
        if (cost)
            _totalCost -= [cost unsignedIntegerValue];

        [_dictionary removeObjectForKey:key];
        [_createdDates removeObjectForKey:key];
        [_accessDates removeObjectForKey:key];
        [_costs removeObjectForKey:key];
        [_ageLimits removeObjectForKey:key];
    [self unlock];
    
    if (didRemoveObjectBlock)
        didRemoveObjectBlock(self, key, nil);
}

- (void)trimMemoryToDate:(NSDate *)trimDate
{
    [self lock];
        NSDictionary *createdDates = [_createdDates copy];
        NSDictionary *ageLimits = [_ageLimits copy];
    [self unlock];
    
    [createdDates enumerateKeysAndObjectsUsingBlock:^(NSString * _Nonnull key, NSDate * _Nonnull createdDate, BOOL * _Nonnull stop) {
        NSTimeInterval ageLimit = [ageLimits[key] doubleValue];
        if (!createdDate || ageLimit > 0.0) {
            return;
        }
        if ([createdDate compare:trimDate] == NSOrderedAscending) { // older than trim date
            [self removeObjectAndExecuteBlocksForKey:key];
        }
    }];
}

- (void)removeExpiredObjects
{
    [self lock];
        NSDictionary<NSString *, NSDate *> *createdDates = [_createdDates copy];
        NSDictionary<NSString *, NSNumber *> *ageLimits = [_ageLimits copy];
        NSTimeInterval globalAgeLimit = self->_ageLimit;
    [self unlock];

    NSDate *now = [NSDate date];
    for (NSString *key in ageLimits) {
        NSDate *createdDate = createdDates[key];
        NSTimeInterval ageLimit = [ageLimits[key] doubleValue] ?: globalAgeLimit;
        if (!createdDate)
            continue;

        NSDate *expirationDate = [createdDate dateByAddingTimeInterval:ageLimit];
        if ([expirationDate compare:now] == NSOrderedAscending) { // Expiration date has passed
            [self removeObjectAndExecuteBlocksForKey:key];
        }
    }
}

- (void)trimToCostLimit:(NSUInteger)limit
{
    NSUInteger totalCost = 0;
    
    [self lock];
        totalCost = _totalCost;
        NSArray *keysSortedByCost = [_costs keysSortedByValueUsingSelector:@selector(compare:)];
    [self unlock];
    
    if (totalCost <= limit) {
        return;
    }

    for (NSString *key in [keysSortedByCost reverseObjectEnumerator]) { // costliest objects first
        [self removeObjectAndExecuteBlocksForKey:key];

        [self lock];
            totalCost = _totalCost;
        [self unlock];
        
        if (totalCost <= limit)
            break;
    }
}

- (void)trimToCostLimitByDate:(NSUInteger)limit
{
    if (self.isTTLCache) {
        [self removeExpiredObjects];
    }

    NSUInteger totalCost = 0;
    
    [self lock];
        totalCost = _totalCost;
        NSArray *keysSortedByAccessDate = [_accessDates keysSortedByValueUsingSelector:@selector(compare:)];
    [self unlock];
    
    if (totalCost <= limit)
        return;

    for (NSString *key in keysSortedByAccessDate) { // oldest objects first
        [self removeObjectAndExecuteBlocksForKey:key];

        [self lock];
            totalCost = _totalCost;
        [self unlock];
        if (totalCost <= limit)
            break;
    }
}

- (void)trimToAgeLimitRecursively
{
    [self lock];
        NSTimeInterval ageLimit = _ageLimit;
    [self unlock];
    
    if (ageLimit == 0.0)
        return;

    NSDate *date = [[NSDate alloc] initWithTimeIntervalSinceNow:-ageLimit];
    
    [self trimMemoryToDate:date];
    
    dispatch_time_t time = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(ageLimit * NSEC_PER_SEC));
    dispatch_after(time, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(void){
        // Ensure that ageLimit is the same as when we were scheduled, otherwise, we've been
        // rescheduled (another dispatch_after was issued) and should cancel.
        BOOL shouldReschedule = YES;
        [self lock];
            if (ageLimit != self->_ageLimit) {
                shouldReschedule = NO;
            }
        [self unlock];
        
        if (shouldReschedule) {
            [self.operationQueue scheduleOperation:^{
                [self trimToAgeLimitRecursively];
            } withPriority:PINOperationQueuePriorityLow];
        }
    });
}

#pragma mark - Public Asynchronous Methods -

- (void)containsObjectForKeyAsync:(NSString *)key completion:(PINCacheObjectContainmentBlock)block
{
    if (!key || !block)
        return;
    
    [self.operationQueue scheduleOperation:^{
        BOOL containsObject = [self containsObjectForKey:key];
        
        block(containsObject);
    } withPriority:PINOperationQueuePriorityHigh];
}

- (void)objectForKeyAsync:(NSString *)key completion:(PINCacheObjectBlock)block
{
    if (block == nil) {
      return;
    }
    
    [self.operationQueue scheduleOperation:^{
        id object = [self objectForKey:key];
        
        block(self, key, object);
    } withPriority:PINOperationQueuePriorityHigh];
}

- (void)setObjectAsync:(id)object forKey:(NSString *)key completion:(PINCacheObjectBlock)block
{
    [self setObjectAsync:object forKey:key withCost:0 completion:block];
}

- (void)setObjectAsync:(id)object forKey:(NSString *)key withAgeLimit:(NSTimeInterval)ageLimit completion:(PINCacheObjectBlock)block
{
    [self setObjectAsync:object forKey:key withCost:0 ageLimit:ageLimit completion:block];
}

- (void)setObjectAsync:(id)object forKey:(NSString *)key withCost:(NSUInteger)cost completion:(PINCacheObjectBlock)block
{
    [self setObjectAsync:object forKey:key withCost:cost ageLimit:0.0 completion:block];
}

- (void)setObjectAsync:(id)object forKey:(NSString *)key withCost:(NSUInteger)cost ageLimit:(NSTimeInterval)ageLimit completion:(PINCacheObjectBlock)block
{
    [self.operationQueue scheduleOperation:^{
        [self setObject:object forKey:key withCost:cost ageLimit:ageLimit];
        
        if (block)
            block(self, key, object);
    } withPriority:PINOperationQueuePriorityHigh];
}

- (void)removeObjectForKeyAsync:(NSString *)key completion:(PINCacheObjectBlock)block
{
    [self.operationQueue scheduleOperation:^{
        [self removeObjectForKey:key];
        
        if (block)
            block(self, key, nil);
    } withPriority:PINOperationQueuePriorityLow];
}

- (void)trimToDateAsync:(NSDate *)trimDate completion:(PINCacheBlock)block
{
    [self.operationQueue scheduleOperation:^{
        [self trimToDate:trimDate];
        
        if (block)
            block(self);
    } withPriority:PINOperationQueuePriorityLow];
}

- (void)trimToCostAsync:(NSUInteger)cost completion:(PINCacheBlock)block
{
    [self.operationQueue scheduleOperation:^{
        [self trimToCost:cost];
        
        if (block)
            block(self);
    } withPriority:PINOperationQueuePriorityLow];
}

- (void)trimToCostByDateAsync:(NSUInteger)cost completion:(PINCacheBlock)block
{
    [self.operationQueue scheduleOperation:^{
        [self trimToCostByDate:cost];
        
        if (block)
            block(self);
    } withPriority:PINOperationQueuePriorityLow];
}

- (void)removeExpiredObjectsAsync:(PINCacheBlock)block
{
    [self.operationQueue scheduleOperation:^{
        [self removeExpiredObjects];

        if (block)
            block(self);
    } withPriority:PINOperationQueuePriorityLow];
}

- (void)removeAllObjectsAsync:(PINCacheBlock)block
{
    [self.operationQueue scheduleOperation:^{
        [self removeAllObjects];
        
        if (block)
            block(self);
    } withPriority:PINOperationQueuePriorityLow];
}

- (void)enumerateObjectsWithBlockAsync:(PINCacheObjectEnumerationBlock)block completionBlock:(PINCacheBlock)completionBlock
{
    [self.operationQueue scheduleOperation:^{
        [self enumerateObjectsWithBlock:block];
        
        if (completionBlock)
            completionBlock(self);
    } withPriority:PINOperationQueuePriorityLow];
}

#pragma mark - Public Synchronous Methods -

- (BOOL)containsObjectForKey:(NSString *)key
{
    if (!key)
        return NO;
    
    [self lock];
        BOOL containsObject = (_dictionary[key] != nil);
    [self unlock];
    return containsObject;
}

- (nullable id)objectForKey:(NSString *)key
{
    if (!key)
        return nil;
    
    NSDate *now = [NSDate date];
    [self lock];
        id object = nil;
        // If the cache should behave like a TTL cache, then only fetch the object if there's a valid ageLimit and  the object is still alive
        NSTimeInterval ageLimit = [_ageLimits[key] doubleValue] ?: self->_ageLimit;
        if (!self->_ttlCache || ageLimit <= 0 || fabs([[_createdDates objectForKey:key] timeIntervalSinceDate:now]) < ageLimit) {
            object = _dictionary[key];
        }
    [self unlock];
        
    if (object) {
        [self lock];
            _accessDates[key] = now;
        [self unlock];
    }

    return object;
}

- (id)objectForKeyedSubscript:(NSString *)key
{
    return [self objectForKey:key];
}

- (void)setObject:(id)object forKey:(NSString *)key
{
    [self setObject:object forKey:key withCost:0];
}

- (void)setObject:(id)object forKey:(NSString *)key withAgeLimit:(NSTimeInterval)ageLimit
{
    [self setObject:object forKey:key withCost:0 ageLimit:ageLimit];
}

- (void)setObject:(id)object forKeyedSubscript:(NSString *)key
{
    if (object == nil) {
        [self removeObjectForKey:key];
    } else {
        [self setObject:object forKey:key];
    }
}

- (void)setObject:(id)object forKey:(NSString *)key withCost:(NSUInteger)cost
{
    [self setObject:object forKey:key withCost:cost ageLimit:0.0];
}

- (void)setObject:(id)object forKey:(NSString *)key withCost:(NSUInteger)cost ageLimit:(NSTimeInterval)ageLimit
{
    NSAssert(ageLimit <= 0.0 || (ageLimit > 0.0 && _ttlCache), @"ttlCache must be set to YES if setting an object-level age limit.");

    if (!key || !object)
        return;
    
    [self lock];
        PINCacheObjectBlock willAddObjectBlock = _willAddObjectBlock;
        PINCacheObjectBlock didAddObjectBlock = _didAddObjectBlock;
        NSUInteger costLimit = _costLimit;
    [self unlock];
    
    if (willAddObjectBlock)
        willAddObjectBlock(self, key, object);
    
    [self lock];
        NSNumber* oldCost = _costs[key];
        if (oldCost)
            _totalCost -= [oldCost unsignedIntegerValue];

        NSDate *now = [NSDate date];
        _dictionary[key] = object;
        _createdDates[key] = now;
        _accessDates[key] = now;
        _costs[key] = @(cost);

        if (ageLimit > 0.0) {
            _ageLimits[key] = @(ageLimit);
        } else {
            [_ageLimits removeObjectForKey:key];
        }

        _totalCost += cost;
    [self unlock];
    
    if (didAddObjectBlock)
        didAddObjectBlock(self, key, object);
    
    if (costLimit > 0)
        [self trimToCostByDate:costLimit];
}

- (void)removeObjectForKey:(NSString *)key
{
    if (!key)
        return;
    
    [self removeObjectAndExecuteBlocksForKey:key];
}

- (void)trimToDate:(NSDate *)trimDate
{
    if (!trimDate)
        return;
    
    if ([trimDate isEqualToDate:[NSDate distantPast]]) {
        [self removeAllObjects];
        return;
    }
    
    [self trimMemoryToDate:trimDate];
}

- (void)trimToCost:(NSUInteger)cost
{
    [self trimToCostLimit:cost];
}

- (void)trimToCostByDate:(NSUInteger)cost
{
    [self trimToCostLimitByDate:cost];
}

- (void)removeAllObjects
{
    [self lock];
        PINCacheBlock willRemoveAllObjectsBlock = _willRemoveAllObjectsBlock;
        PINCacheBlock didRemoveAllObjectsBlock = _didRemoveAllObjectsBlock;
    [self unlock];
    
    if (willRemoveAllObjectsBlock)
        willRemoveAllObjectsBlock(self);
    
    [self lock];
        [_dictionary removeAllObjects];
        [_createdDates removeAllObjects];
        [_accessDates removeAllObjects];
        [_costs removeAllObjects];
        [_ageLimits removeAllObjects];
    
        _totalCost = 0;
    [self unlock];
    
    if (didRemoveAllObjectsBlock)
        didRemoveAllObjectsBlock(self);
    
}

- (void)enumerateObjectsWithBlock:(PIN_NOESCAPE PINCacheObjectEnumerationBlock)block
{
    if (!block)
        return;
    
    [self lock];
        NSDate *now = [NSDate date];
        NSArray *keysSortedByCreatedDate = [_createdDates keysSortedByValueUsingSelector:@selector(compare:)];
        
        for (NSString *key in keysSortedByCreatedDate) {
            // If the cache should behave like a TTL cache, then only fetch the object if there's a valid ageLimit and  the object is still alive
            NSTimeInterval ageLimit = [_ageLimits[key] doubleValue] ?: self->_ageLimit;
            if (!self->_ttlCache || ageLimit <= 0 || fabs([[_createdDates objectForKey:key] timeIntervalSinceDate:now]) < ageLimit) {
                BOOL stop = NO;
                block(self, key, _dictionary[key], &stop);
                if (stop)
                    break;
            }
        }
    [self unlock];
}

#pragma mark - Public Thread Safe Accessors -

- (PINCacheObjectBlock)willAddObjectBlock
{
    [self lock];
        PINCacheObjectBlock block = _willAddObjectBlock;
    [self unlock];

    return block;
}

- (void)setWillAddObjectBlock:(PINCacheObjectBlock)block
{
    [self lock];
        _willAddObjectBlock = [block copy];
    [self unlock];
}

- (PINCacheObjectBlock)willRemoveObjectBlock
{
    [self lock];
        PINCacheObjectBlock block = _willRemoveObjectBlock;
    [self unlock];

    return block;
}

- (void)setWillRemoveObjectBlock:(PINCacheObjectBlock)block
{
    [self lock];
        _willRemoveObjectBlock = [block copy];
    [self unlock];
}

- (PINCacheBlock)willRemoveAllObjectsBlock
{
    [self lock];
        PINCacheBlock block = _willRemoveAllObjectsBlock;
    [self unlock];

    return block;
}

- (void)setWillRemoveAllObjectsBlock:(PINCacheBlock)block
{
    [self lock];
        _willRemoveAllObjectsBlock = [block copy];
    [self unlock];
}

- (PINCacheObjectBlock)didAddObjectBlock
{
    [self lock];
        PINCacheObjectBlock block = _didAddObjectBlock;
    [self unlock];

    return block;
}

- (void)setDidAddObjectBlock:(PINCacheObjectBlock)block
{
    [self lock];
        _didAddObjectBlock = [block copy];
    [self unlock];
}

- (PINCacheObjectBlock)didRemoveObjectBlock
{
    [self lock];
        PINCacheObjectBlock block = _didRemoveObjectBlock;
    [self unlock];

    return block;
}

- (void)setDidRemoveObjectBlock:(PINCacheObjectBlock)block
{
    [self lock];
        _didRemoveObjectBlock = [block copy];
    [self unlock];
}

- (PINCacheBlock)didRemoveAllObjectsBlock
{
    [self lock];
        PINCacheBlock block = _didRemoveAllObjectsBlock;
    [self unlock];

    return block;
}

- (void)setDidRemoveAllObjectsBlock:(PINCacheBlock)block
{
    [self lock];
        _didRemoveAllObjectsBlock = [block copy];
    [self unlock];
}

- (PINCacheBlock)didReceiveMemoryWarningBlock
{
    [self lock];
        PINCacheBlock block = _didReceiveMemoryWarningBlock;
    [self unlock];

    return block;
}

- (void)setDidReceiveMemoryWarningBlock:(PINCacheBlock)block
{
    [self lock];
        _didReceiveMemoryWarningBlock = [block copy];
    [self unlock];
}

- (PINCacheBlock)didEnterBackgroundBlock
{
    [self lock];
        PINCacheBlock block = _didEnterBackgroundBlock;
    [self unlock];

    return block;
}

- (void)setDidEnterBackgroundBlock:(PINCacheBlock)block
{
    [self lock];
        _didEnterBackgroundBlock = [block copy];
    [self unlock];
}

- (NSTimeInterval)ageLimit
{
    [self lock];
        NSTimeInterval ageLimit = _ageLimit;
    [self unlock];
    
    return ageLimit;
}

- (void)setAgeLimit:(NSTimeInterval)ageLimit
{
    [self lock];
        _ageLimit = ageLimit;
    [self unlock];
    
    [self trimToAgeLimitRecursively];
}

- (NSUInteger)costLimit
{
    [self lock];
        NSUInteger costLimit = _costLimit;
    [self unlock];

    return costLimit;
}

- (void)setCostLimit:(NSUInteger)costLimit
{
    [self lock];
        _costLimit = costLimit;
    [self unlock];

    if (costLimit > 0)
        [self trimToCostLimitByDate:costLimit];
}

- (NSUInteger)totalCost
{
    [self lock];
        NSUInteger cost = _totalCost;
    [self unlock];
    
    return cost;
}

- (BOOL)isTTLCache {
    BOOL isTTLCache;
    
    [self lock];
        isTTLCache = _ttlCache;
    [self unlock];
    
    return isTTLCache;
}

- (void)lock
{
    __unused int result = pthread_mutex_lock(&_mutex);
    NSAssert(result == 0, @"Failed to lock PINMemoryCache %@. Code: %d", self, result);
}

- (void)unlock
{
    __unused int result = pthread_mutex_unlock(&_mutex);
    NSAssert(result == 0, @"Failed to unlock PINMemoryCache %@. Code: %d", self, result);
}

@end


#pragma mark - Deprecated

@implementation PINMemoryCache (Deprecated)

- (void)containsObjectForKey:(NSString *)key block:(PINMemoryCacheContainmentBlock)block
{
    [self containsObjectForKeyAsync:key completion:block];
}

- (void)objectForKey:(NSString *)key block:(nullable PINMemoryCacheObjectBlock)block
{
    [self objectForKeyAsync:key completion:^(id<PINCaching> memoryCache, NSString *memoryCacheKey, id memoryCacheObject) {
        if (block) {
            block((PINMemoryCache *)memoryCache, memoryCacheKey, memoryCacheObject);
        }
    }];
}

- (void)setObject:(id)object forKey:(NSString *)key block:(nullable PINMemoryCacheObjectBlock)block
{
    [self setObjectAsync:object forKey:key completion:^(id<PINCaching> memoryCache, NSString *memoryCacheKey, id memoryCacheObject) {
        if (block) {
            block((PINMemoryCache *)memoryCache, memoryCacheKey, memoryCacheObject);
        }
    }];
}

- (void)setObject:(id)object forKey:(NSString *)key withCost:(NSUInteger)cost block:(nullable PINMemoryCacheObjectBlock)block
{
    [self setObjectAsync:object forKey:key withCost:cost completion:^(id<PINCaching> memoryCache, NSString *memoryCacheKey, id memoryCacheObject) {
        if (block) {
            block((PINMemoryCache *)memoryCache, memoryCacheKey, memoryCacheObject);
        }
    }];
}

- (void)removeObjectForKey:(NSString *)key block:(nullable PINMemoryCacheObjectBlock)block
{
    [self removeObjectForKeyAsync:key completion:^(id<PINCaching> memoryCache, NSString *memoryCacheKey, id memoryCacheObject) {
        if (block) {
            block((PINMemoryCache *)memoryCache, memoryCacheKey, memoryCacheObject);
        }
    }];
}

- (void)trimToDate:(NSDate *)date block:(nullable PINMemoryCacheBlock)block
{
    [self trimToDateAsync:date completion:^(id<PINCaching> memoryCache) {
        if (block) {
            block((PINMemoryCache *)memoryCache);
        }
    }];
}

- (void)trimToCost:(NSUInteger)cost block:(nullable PINMemoryCacheBlock)block
{
    [self trimToCostAsync:cost completion:^(id<PINCaching> memoryCache) {
        if (block) {
            block((PINMemoryCache *)memoryCache);
        }
    }];
}

- (void)trimToCostByDate:(NSUInteger)cost block:(nullable PINMemoryCacheBlock)block
{
    [self trimToCostByDateAsync:cost completion:^(id<PINCaching> memoryCache) {
        if (block) {
            block((PINMemoryCache *)memoryCache);
        }
    }];
}

- (void)removeAllObjects:(nullable PINMemoryCacheBlock)block
{
    [self removeAllObjectsAsync:^(id<PINCaching> memoryCache) {
        if (block) {
            block((PINMemoryCache *)memoryCache);
        }
    }];
}

- (void)enumerateObjectsWithBlock:(PINMemoryCacheObjectBlock)block completionBlock:(nullable PINMemoryCacheBlock)completionBlock
{
    [self enumerateObjectsWithBlockAsync:^(id<PINCaching> _Nonnull cache, NSString * _Nonnull key, id _Nullable object, BOOL * _Nonnull stop) {
        if ([cache isKindOfClass:[PINMemoryCache class]]) {
            PINMemoryCache *memoryCache = (PINMemoryCache *)cache;
            block(memoryCache, key, object);
        }
    } completionBlock:^(id<PINCaching> memoryCache) {
        if (completionBlock) {
            completionBlock((PINMemoryCache *)memoryCache);
        }
    }];
}

- (void)setTtlCache:(BOOL)ttlCache
{
    [self lock];
        _ttlCache = ttlCache;
    [self unlock];
}

@end

//  PINCache is a modified version of TMCache
//  Modifications by Garrett Moon
//  Copyright (c) 2015 Pinterest. All rights reserved.

#import "PINDiskCache.h"

#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_4_0
#import <UIKit/UIKit.h>
#endif

#import <pthread.h>

#define PINDiskCacheError(error) if (error) { NSLog(@"%@ (%d) ERROR: %@", \
[[NSString stringWithUTF8String:__FILE__] lastPathComponent], \
__LINE__, [error localizedDescription]); }

static NSString * const PINDiskCachePrefix = @"com.pinterest.PINDiskCache";
static NSString * const PINDiskCacheSharedName = @"PINDiskCacheShared";

@interface PINBackgroundTask : NSObject
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_4_0 && !TARGET_OS_WATCH
@property (atomic, assign) UIBackgroundTaskIdentifier taskID;
#endif
+ (instancetype)start;
- (void)end;
@end

typedef NS_ENUM(NSUInteger, PINDiskCacheCondition) {
    PINDiskCacheConditionNotReady = 0,
    PINDiskCacheConditionReady = 1,
};

@interface PINDiskCache () {
    NSConditionLock *_instanceLock;
}

@property (assign) NSUInteger byteCount;
@property (strong, nonatomic) NSURL *cacheURL;
#if OS_OBJECT_USE_OBJC
@property (strong, nonatomic) dispatch_queue_t asyncQueue;
#else
@property (assign, nonatomic) dispatch_queue_t asyncQueue;
#endif
@property (strong, nonatomic) NSMutableDictionary *dates;
@property (strong, nonatomic) NSMutableDictionary *sizes;
@end

@implementation PINDiskCache

@synthesize willAddObjectBlock = _willAddObjectBlock;
@synthesize willRemoveObjectBlock = _willRemoveObjectBlock;
@synthesize willRemoveAllObjectsBlock = _willRemoveAllObjectsBlock;
@synthesize didAddObjectBlock = _didAddObjectBlock;
@synthesize didRemoveObjectBlock = _didRemoveObjectBlock;
@synthesize didRemoveAllObjectsBlock = _didRemoveAllObjectsBlock;
@synthesize byteLimit = _byteLimit;
@synthesize ageLimit = _ageLimit;
@synthesize ttlCache = _ttlCache;

#if TARGET_OS_IPHONE
@synthesize writingProtectionOption = _writingProtectionOption;
#endif

#pragma mark - Initialization -

- (void)dealloc
{
#if !OS_OBJECT_USE_OBJC
    dispatch_release(_asyncQueue);
    _asyncQueue = nil;
#endif
}

- (instancetype)init
{
    @throw [NSException exceptionWithName:@"Must initialize with a name" reason:@"PINDiskCache must be initialized with a name. Call initWithName: instead." userInfo:nil];
    return [self initWithName:@""];
}

- (instancetype)initWithName:(NSString *)name
{
    return [self initWithName:name rootPath:[NSSearchPathForDirectoriesInDomains(NSCachesDirectory, NSUserDomainMask, YES) objectAtIndex:0]];
}

- (instancetype)initWithName:(NSString *)name rootPath:(NSString *)rootPath
{
    if (!name)
        return nil;
    
    if (self = [super init]) {
        _name = [name copy];
        _asyncQueue = dispatch_queue_create([[NSString stringWithFormat:@"%@ Asynchronous Queue", PINDiskCachePrefix] UTF8String], DISPATCH_QUEUE_CONCURRENT);
        _instanceLock = [[NSConditionLock alloc] initWithCondition:PINDiskCacheConditionNotReady];
        _willAddObjectBlock = nil;
        _willRemoveObjectBlock = nil;
        _willRemoveAllObjectsBlock = nil;
        _didAddObjectBlock = nil;
        _didRemoveObjectBlock = nil;
        _didRemoveAllObjectsBlock = nil;
        
        _byteCount = 0;
        _byteLimit = 0;
        _ageLimit = 0.0;
      
        #if TARGET_OS_IPHONE
          _writingProtectionOption = NSDataWritingFileProtectionNone;
        #endif
      
        _dates = [[NSMutableDictionary alloc] init];
        _sizes = [[NSMutableDictionary alloc] init];
        
        NSString *pathComponent = [[NSString alloc] initWithFormat:@"%@.%@", PINDiskCachePrefix, _name];
        _cacheURL = [NSURL fileURLWithPathComponents:@[ rootPath, pathComponent ]];
        
        //we don't want to do anything without setting up the disk cache, but we also don't want to block init, it can take a while to initialize
        dispatch_async(_asyncQueue, ^{
            //should always be able to aquire the lock unless the below code is running.
            [_instanceLock lockWhenCondition:PINDiskCacheConditionNotReady];
                [self createCacheDirectory];
                [self initializeDiskProperties];
            [_instanceLock unlockWithCondition:PINDiskCacheConditionReady];
        });
    }
    return self;
}

- (NSString *)description
{
    return [[NSString alloc] initWithFormat:@"%@.%@.%p", PINDiskCachePrefix, _name, (void *)self];
}

+ (instancetype)sharedCache
{
    static id cache;
    static dispatch_once_t predicate;
    
    dispatch_once(&predicate, ^{
        cache = [[self alloc] initWithName:PINDiskCacheSharedName];
    });
    
    return cache;
}

#pragma mark - Private Methods -

- (NSURL *)encodedFileURLForKey:(NSString *)key
{
    if (![key length])
        return nil;
    
    return [_cacheURL URLByAppendingPathComponent:[self encodedString:key]];
}

- (NSString *)keyForEncodedFileURL:(NSURL *)url
{
    NSString *fileName = [url lastPathComponent];
    if (!fileName)
        return nil;
    
    return [self decodedString:fileName];
}

- (NSString *)encodedString:(NSString *)string
{
    if (![string length]) {
        return @"";
    }
    
    if ([string respondsToSelector:@selector(stringByAddingPercentEncodingWithAllowedCharacters:)]) {
        return [string stringByAddingPercentEncodingWithAllowedCharacters:[[NSCharacterSet characterSetWithCharactersInString:@".:/%"] invertedSet]];
    }
    else {
        CFStringRef static const charsToEscape = CFSTR(".:/%");
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        CFStringRef escapedString = CFURLCreateStringByAddingPercentEscapes(kCFAllocatorDefault,
                                                                            (__bridge CFStringRef)string,
                                                                            NULL,
                                                                            charsToEscape,
                                                                            kCFStringEncodingUTF8);
#pragma clang diagnostic pop
        return (__bridge_transfer NSString *)escapedString;
    }
}

- (NSString *)decodedString:(NSString *)string
{
    if (![string length]) {
        return @"";
    }
    
    if ([string respondsToSelector:@selector(stringByRemovingPercentEncoding)]) {
        return [string stringByRemovingPercentEncoding];
    }
    else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
        CFStringRef unescapedString = CFURLCreateStringByReplacingPercentEscapesUsingEncoding(kCFAllocatorDefault,
                                                                                              (__bridge CFStringRef)string,
                                                                                              CFSTR(""),
                                                                                              kCFStringEncodingUTF8);
#pragma clang diagnostic pop
        return (__bridge_transfer NSString *)unescapedString;
    }
}

#pragma mark - Private Trash Methods -

+ (dispatch_queue_t)sharedTrashQueue
{
    static dispatch_queue_t trashQueue;
    static dispatch_once_t predicate;
    
    dispatch_once(&predicate, ^{
        NSString *queueName = [[NSString alloc] initWithFormat:@"%@.trash", PINDiskCachePrefix];
        trashQueue = dispatch_queue_create([queueName UTF8String], DISPATCH_QUEUE_SERIAL);
        dispatch_set_target_queue(trashQueue, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_BACKGROUND, 0));
    });
    
    return trashQueue;
}

+ (NSURL *)sharedTrashURL
{
    static NSURL *sharedTrashURL;
    static dispatch_once_t predicate;
    
    dispatch_once(&predicate, ^{
        sharedTrashURL = [[[NSURL alloc] initFileURLWithPath:NSTemporaryDirectory()] URLByAppendingPathComponent:PINDiskCachePrefix isDirectory:YES];
        
        if (![[NSFileManager defaultManager] fileExistsAtPath:[sharedTrashURL path]]) {
            NSError *error = nil;
            [[NSFileManager defaultManager] createDirectoryAtURL:sharedTrashURL
                                     withIntermediateDirectories:YES
                                                      attributes:nil
                                                           error:&error];
            PINDiskCacheError(error);
        }
    });
    
    return sharedTrashURL;
}

+(BOOL)moveItemAtURLToTrash:(NSURL *)itemURL
{
    if (![[NSFileManager defaultManager] fileExistsAtPath:[itemURL path]])
        return NO;
    
    NSError *error = nil;
    NSString *uniqueString = [[NSProcessInfo processInfo] globallyUniqueString];
    NSURL *uniqueTrashURL = [[PINDiskCache sharedTrashURL] URLByAppendingPathComponent:uniqueString];
    BOOL moved = [[NSFileManager defaultManager] moveItemAtURL:itemURL toURL:uniqueTrashURL error:&error];
    PINDiskCacheError(error);
    return moved;
}

+ (void)emptyTrash
{
    dispatch_async([self sharedTrashQueue], ^{
        PINBackgroundTask *task = [PINBackgroundTask start];
        
        NSError *searchTrashedItemsError = nil;
        NSArray *trashedItems = [[NSFileManager defaultManager] contentsOfDirectoryAtURL:[self sharedTrashURL]
                                                              includingPropertiesForKeys:nil
                                                                                 options:0
                                                                                   error:&searchTrashedItemsError];
        PINDiskCacheError(searchTrashedItemsError);
        
        for (NSURL *trashedItemURL in trashedItems) {
            NSError *removeTrashedItemError = nil;
            [[NSFileManager defaultManager] removeItemAtURL:trashedItemURL error:&removeTrashedItemError];
            PINDiskCacheError(removeTrashedItemError);
        }
        
        [task end];
    });
}

#pragma mark - Private Queue Methods -

- (BOOL)createCacheDirectory
{
    if ([[NSFileManager defaultManager] fileExistsAtPath:[_cacheURL path]])
        return NO;
    
    NSError *error = nil;
    BOOL success = [[NSFileManager defaultManager] createDirectoryAtURL:_cacheURL
                                            withIntermediateDirectories:YES
                                                             attributes:nil
                                                                  error:&error];
    PINDiskCacheError(error);
    
    return success;
}

- (void)initializeDiskProperties
{
    NSUInteger byteCount = 0;
    NSArray *keys = @[ NSURLContentModificationDateKey, NSURLTotalFileAllocatedSizeKey ];
    
    NSError *error = nil;
    NSArray *files = [[NSFileManager defaultManager] contentsOfDirectoryAtURL:_cacheURL
                                                   includingPropertiesForKeys:keys
                                                                      options:NSDirectoryEnumerationSkipsHiddenFiles
                                                                        error:&error];
    PINDiskCacheError(error);
    
    for (NSURL *fileURL in files) {
        NSString *key = [self keyForEncodedFileURL:fileURL];
        
        error = nil;
        NSDictionary *dictionary = [fileURL resourceValuesForKeys:keys error:&error];
        PINDiskCacheError(error);
        
        NSDate *date = [dictionary objectForKey:NSURLContentModificationDateKey];
        if (date && key)
            [_dates setObject:date forKey:key];
        
        NSNumber *fileSize = [dictionary objectForKey:NSURLTotalFileAllocatedSizeKey];
        if (fileSize) {
            [_sizes setObject:fileSize forKey:key];
            byteCount += [fileSize unsignedIntegerValue];
        }
    }
    
    if (byteCount > 0)
        self.byteCount = byteCount; // atomic
}

- (BOOL)setFileModificationDate:(NSDate *)date forURL:(NSURL *)fileURL
{
    if (!date || !fileURL) {
        return NO;
    }
    
    NSError *error = nil;
    BOOL success = [[NSFileManager defaultManager] setAttributes:@{ NSFileModificationDate: date }
                                                    ofItemAtPath:[fileURL path]
                                                           error:&error];
    PINDiskCacheError(error);
    
    if (success) {
        NSString *key = [self keyForEncodedFileURL:fileURL];
        if (key) {
            [_dates setObject:date forKey:key];
        }
    }
    
    return success;
}

- (BOOL)removeFileAndExecuteBlocksForKey:(NSString *)key
{
    NSURL *fileURL = [self encodedFileURLForKey:key];
    if (!fileURL || ![[NSFileManager defaultManager] fileExistsAtPath:[fileURL path]])
        return NO;
    
    if (_willRemoveObjectBlock)
        _willRemoveObjectBlock(self, key, nil, fileURL);
    
    BOOL trashed = [PINDiskCache moveItemAtURLToTrash:fileURL];
    if (!trashed)
        return NO;
    
    [PINDiskCache emptyTrash];
    
    NSNumber *byteSize = [_sizes objectForKey:key];
    if (byteSize)
        self.byteCount = _byteCount - [byteSize unsignedIntegerValue]; // atomic
    
    [_sizes removeObjectForKey:key];
    [_dates removeObjectForKey:key];
    
    if (_didRemoveObjectBlock)
        _didRemoveObjectBlock(self, key, nil, fileURL);
    
    return YES;
}

- (void)trimDiskToSize:(NSUInteger)trimByteCount
{
    if (_byteCount <= trimByteCount)
        return;
    
    NSArray *keysSortedBySize = [_sizes keysSortedByValueUsingSelector:@selector(compare:)];
    
    for (NSString *key in [keysSortedBySize reverseObjectEnumerator]) { // largest objects first
        [self removeFileAndExecuteBlocksForKey:key];
        
        if (_byteCount <= trimByteCount)
            break;
    }
}

- (void)trimDiskToSizeByDate:(NSUInteger)trimByteCount
{
    if (_byteCount <= trimByteCount)
        return;
    
    NSArray *keysSortedByDate = [_dates keysSortedByValueUsingSelector:@selector(compare:)];
    
    for (NSString *key in keysSortedByDate) { // oldest objects first
        [self removeFileAndExecuteBlocksForKey:key];
        
        if (_byteCount <= trimByteCount)
            break;
    }
}

- (void)trimDiskToDate:(NSDate *)trimDate
{
    NSArray *keysSortedByDate = [_dates keysSortedByValueUsingSelector:@selector(compare:)];
    
    for (NSString *key in keysSortedByDate) { // oldest files first
        NSDate *accessDate = [_dates objectForKey:key];
        if (!accessDate)
            continue;
        
        if ([accessDate compare:trimDate] == NSOrderedAscending) { // older than trim date
            [self removeFileAndExecuteBlocksForKey:key];
        } else {
            break;
        }
    }
}

- (void)trimToAgeLimitRecursively
{
    [self lock];
        NSTimeInterval ageLimit = _ageLimit;
    [self unlock];
    if (ageLimit == 0.0)
        return;
    
    [self lock];
        NSDate *date = [[NSDate alloc] initWithTimeIntervalSinceNow:-ageLimit];
        [self trimDiskToDate:date];
    [self unlock];
    
    __weak PINDiskCache *weakSelf = self;
    
    dispatch_time_t time = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(_ageLimit * NSEC_PER_SEC));
    dispatch_after(time, _asyncQueue, ^(void) {
        PINDiskCache *strongSelf = weakSelf;
        [strongSelf trimToAgeLimitRecursively];
    });
}

#pragma mark - Public Asynchronous Methods -

- (void)lockFileAccessWhileExecutingBlock:(void(^)(PINDiskCache *diskCache))block
{
    __weak PINDiskCache *weakSelf = self;
    
    dispatch_async(_asyncQueue, ^{
        PINDiskCache *strongSelf = weakSelf;
        if (block) {
            [strongSelf lock];
                block(strongSelf);
            [strongSelf unlock];
        }
    });
}

- (void)containsObjectForKey:(NSString *)key block:(PINDiskCacheContainsBlock)block
{
    if (!key || !block)
        return;
    
    __weak PINDiskCache *weakSelf = self;
    
    dispatch_async(_asyncQueue, ^{
        PINDiskCache *strongSelf = weakSelf;
        block([strongSelf containsObjectForKey:key]);
    });
}

- (void)objectForKey:(NSString *)key block:(PINDiskCacheObjectBlock)block
{
    __weak PINDiskCache *weakSelf = self;
    
    dispatch_async(_asyncQueue, ^{
        PINDiskCache *strongSelf = weakSelf;
        NSURL *fileURL = nil;
        id <NSCoding> object = [strongSelf objectForKey:key fileURL:&fileURL];
        
        if (block) {
            [strongSelf lock];
                block(strongSelf, key, object, fileURL);
            [strongSelf unlock];
        }
    });
}

- (void)fileURLForKey:(NSString *)key block:(PINDiskCacheObjectBlock)block
{
    __weak PINDiskCache *weakSelf = self;
    
    dispatch_async(_asyncQueue, ^{
        PINDiskCache *strongSelf = weakSelf;
        NSURL *fileURL = [strongSelf fileURLForKey:key];
        
        if (block) {
            [strongSelf lock];
                block(strongSelf, key, nil, fileURL);
            [strongSelf unlock];
        }
    });
}

- (void)setObject:(id <NSCoding>)object forKey:(NSString *)key block:(PINDiskCacheObjectBlock)block
{
    __weak PINDiskCache *weakSelf = self;
    
    dispatch_async(_asyncQueue, ^{
        PINDiskCache *strongSelf = weakSelf;
        NSURL *fileURL = nil;
        [strongSelf setObject:object forKey:key fileURL:&fileURL];
        
        if (block) {
            [strongSelf lock];
                block(strongSelf, key, object, fileURL);
            [strongSelf unlock];
        }
    });
}

- (void)removeObjectForKey:(NSString *)key block:(PINDiskCacheObjectBlock)block
{
    __weak PINDiskCache *weakSelf = self;
    
    dispatch_async(_asyncQueue, ^{
        PINDiskCache *strongSelf = weakSelf;
        NSURL *fileURL = nil;
        [strongSelf removeObjectForKey:key fileURL:&fileURL];
        
        if (block) {
            [strongSelf lock];
                block(strongSelf, key, nil, fileURL);
            [strongSelf unlock];
        }
    });
}

- (void)trimToSize:(NSUInteger)trimByteCount block:(PINDiskCacheBlock)block
{
    __weak PINDiskCache *weakSelf = self;
    
    dispatch_async(_asyncQueue, ^{
        PINDiskCache *strongSelf = weakSelf;
        [strongSelf trimToSize:trimByteCount];
        
        if (block) {
            [strongSelf lock];
                block(strongSelf);
            [strongSelf unlock];
        }
    });
}

- (void)trimToDate:(NSDate *)trimDate block:(PINDiskCacheBlock)block
{
    __weak PINDiskCache *weakSelf = self;
    
    dispatch_async(_asyncQueue, ^{
        PINDiskCache *strongSelf = weakSelf;
        [strongSelf trimToDate:trimDate];
        
        if (block) {
            [strongSelf lock];
                block(strongSelf);
            [strongSelf unlock];
        }
    });
}

- (void)trimToSizeByDate:(NSUInteger)trimByteCount block:(PINDiskCacheBlock)block
{
    __weak PINDiskCache *weakSelf = self;
    
    dispatch_async(_asyncQueue, ^{
        PINDiskCache *strongSelf = weakSelf;
        [strongSelf trimToSizeByDate:trimByteCount];
        
        if (block) {
            [strongSelf lock];
                block(strongSelf);
            [strongSelf unlock];
        }
    });
}

- (void)removeAllObjects:(PINDiskCacheBlock)block
{
    __weak PINDiskCache *weakSelf = self;
    
    dispatch_async(_asyncQueue, ^{
        PINDiskCache *strongSelf = weakSelf;
        [strongSelf removeAllObjects];
        
        if (block) {
            [strongSelf lock];
                block(strongSelf);
            [strongSelf unlock];
        }
    });
}

- (void)enumerateObjectsWithBlock:(PINDiskCacheObjectBlock)block completionBlock:(PINDiskCacheBlock)completionBlock
{
    __weak PINDiskCache *weakSelf = self;
    
    dispatch_async(_asyncQueue, ^{
        PINDiskCache *strongSelf = weakSelf;
        [strongSelf enumerateObjectsWithBlock:block];
        
        if (completionBlock) {
            [self lock];
                completionBlock(strongSelf);
            [self unlock];
        }
    });
}

#pragma mark - Public Synchronous Methods -

- (void)synchronouslyLockFileAccessWhileExecutingBlock:(void(^)(PINDiskCache *diskCache))block
{
    if (block) {
        [self lock];
        block(self);
        [self unlock];
    }
}

- (BOOL)containsObjectForKey:(NSString *)key
{
    return ([self fileURLForKey:key updateFileModificationDate:NO] != nil);
}

- (__nullable id<NSCoding>)objectForKey:(NSString *)key
{
    return [self objectForKey:key fileURL:nil];
}

- (id)objectForKeyedSubscript:(NSString *)key
{
    return [self objectForKey:key];
}

- (__nullable id <NSCoding>)objectForKey:(NSString *)key fileURL:(NSURL **)outFileURL
{
    NSDate *now = [[NSDate alloc] init];
    
    if (!key)
        return nil;
    
    id <NSCoding> object = nil;
    NSURL *fileURL = nil;
    
    [self lock];
        fileURL = [self encodedFileURLForKey:key];
        object = nil;
        
        if ([[NSFileManager defaultManager] fileExistsAtPath:[fileURL path]] &&
            // If the cache should behave like a TTL cache, then only fetch the object if there's a valid ageLimit and  the object is still alive
            (!self->_ttlCache || self->_ageLimit <= 0 || fabs([[_dates objectForKey:key] timeIntervalSinceDate:now]) < self->_ageLimit)) {
            @try {
                object = [NSKeyedUnarchiver unarchiveObjectWithFile:[fileURL path]];
            }
            @catch (NSException *exception) {
                NSError *error = nil;
                [[NSFileManager defaultManager] removeItemAtPath:[fileURL path] error:&error];
                PINDiskCacheError(error);
            }
          if (!self->_ttlCache) {
            [self setFileModificationDate:now forURL:fileURL];
          }
        }
    [self unlock];
    
    if (outFileURL) {
        *outFileURL = fileURL;
    }
    
    return object;
}

/// Helper function to call fileURLForKey:updateFileModificationDate:
- (NSURL *)fileURLForKey:(NSString *)key
{
    // Don't update the file modification time, if self is a ttlCache
    return [self fileURLForKey:key updateFileModificationDate:!self->_ttlCache];
}

- (NSURL *)fileURLForKey:(NSString *)key updateFileModificationDate:(BOOL)updateFileModificationDate
{
    if (!key) {
        return nil;
    }
    
    NSDate *now = [[NSDate alloc] init];
    NSURL *fileURL = nil;
    
    [self lock];
        fileURL = [self encodedFileURLForKey:key];
        
        if ([[NSFileManager defaultManager] fileExistsAtPath:[fileURL path]]) {
            if (updateFileModificationDate) {
                [self setFileModificationDate:now forURL:fileURL];
            }
        } else {
            fileURL = nil;
        }
    [self unlock];
    return fileURL;
}

- (void)setObject:(id <NSCoding>)object forKey:(NSString *)key
{
    [self setObject:object forKey:key fileURL:nil];
}

- (void)setObject:(id)object forKeyedSubscript:(NSString *)key
{
    [self setObject:object forKey:key];
}

- (void)setObject:(id <NSCoding>)object forKey:(NSString *)key fileURL:(NSURL **)outFileURL
{
    NSDate *now = [[NSDate alloc] init];
    
    if (!key || !object)
        return;
    
    PINBackgroundTask *task = [PINBackgroundTask start];
    
    #if TARGET_OS_IPHONE
      NSDataWritingOptions writeOptions = NSDataWritingAtomic | self.writingProtectionOption;
    #else
      NSDataWritingOptions writeOptions = NSDataWritingAtomic;
    #endif
  
    NSURL *fileURL = nil;
    
    [self lock];
        fileURL = [self encodedFileURLForKey:key];
        
        if (self->_willAddObjectBlock)
            self->_willAddObjectBlock(self, key, object, fileURL);
  
        NSData *data = [NSKeyedArchiver archivedDataWithRootObject:object];
        NSError *writeError = nil;
  
        BOOL written = [data writeToURL:fileURL options:writeOptions error:&writeError];
        PINDiskCacheError(writeError);
        
        if (written) {
            [self setFileModificationDate:now forURL:fileURL];
            
            NSError *error = nil;
            NSDictionary *values = [fileURL resourceValuesForKeys:@[ NSURLTotalFileAllocatedSizeKey ] error:&error];
            PINDiskCacheError(error);
            
            NSNumber *diskFileSize = [values objectForKey:NSURLTotalFileAllocatedSizeKey];
            if (diskFileSize) {
                NSNumber *prevDiskFileSize = [self->_sizes objectForKey:key];
                if (prevDiskFileSize) {
                    self.byteCount = self->_byteCount - [prevDiskFileSize unsignedIntegerValue];
                }
                [self->_sizes setObject:diskFileSize forKey:key];
                self.byteCount = self->_byteCount + [diskFileSize unsignedIntegerValue]; // atomic
            }
            
            if (self->_byteLimit > 0 && self->_byteCount > self->_byteLimit)
                [self trimToSizeByDate:self->_byteLimit block:nil];
        } else {
            fileURL = nil;
        }
        
        if (self->_didAddObjectBlock)
            self->_didAddObjectBlock(self, key, object, written ? fileURL : nil);
    [self unlock];
    
    if (outFileURL) {
        *outFileURL = fileURL;
    }
    
    [task end];
}

- (void)removeObjectForKey:(NSString *)key
{
    [self removeObjectForKey:key fileURL:nil];
}

- (void)removeObjectForKey:(NSString *)key fileURL:(NSURL **)outFileURL
{
    if (!key)
        return;
    
    PINBackgroundTask *task = [PINBackgroundTask start];
    
    NSURL *fileURL = nil;
    
    [self lock];
        fileURL = [self encodedFileURLForKey:key];
        [self removeFileAndExecuteBlocksForKey:key];
    [self unlock];
    
    [task end];
    
    if (outFileURL) {
        *outFileURL = fileURL;
    }
}

- (void)trimToSize:(NSUInteger)trimByteCount
{
    if (trimByteCount == 0) {
        [self removeAllObjects];
        return;
    }
    
    PINBackgroundTask *task = [PINBackgroundTask start];
    
    [self lock];
        [self trimDiskToSize:trimByteCount];
    [self unlock];
    
    [task end];
}

- (void)trimToDate:(NSDate *)trimDate
{
    if (!trimDate)
        return;
    
    if ([trimDate isEqualToDate:[NSDate distantPast]]) {
        [self removeAllObjects];
        return;
    }
    
    PINBackgroundTask *task = [PINBackgroundTask start];
    
    [self lock];
        [self trimDiskToDate:trimDate];
    [self unlock];
    
    [task end];
}

- (void)trimToSizeByDate:(NSUInteger)trimByteCount
{
    if (trimByteCount == 0) {
        [self removeAllObjects];
        return;
    }
    
    PINBackgroundTask *task = [PINBackgroundTask start];
    
    [self lock];
        [self trimDiskToSizeByDate:trimByteCount];
    [self unlock];
    
    [task end];
}

- (void)removeAllObjects
{
    PINBackgroundTask *task = [PINBackgroundTask start];
    
    [self lock];
        if (self->_willRemoveAllObjectsBlock)
            self->_willRemoveAllObjectsBlock(self);
        
        [PINDiskCache moveItemAtURLToTrash:self->_cacheURL];
        [PINDiskCache emptyTrash];
        
        [self createCacheDirectory];
        
        [self->_dates removeAllObjects];
        [self->_sizes removeAllObjects];
        self.byteCount = 0; // atomic
        
        if (self->_didRemoveAllObjectsBlock)
            self->_didRemoveAllObjectsBlock(self);
    [self unlock];
    
    [task end];
}

- (void)enumerateObjectsWithBlock:(PINDiskCacheObjectBlock)block
{
    if (!block)
        return;
    
    PINBackgroundTask *task = [PINBackgroundTask start];
    
    [self lock];
        NSDate *now = [NSDate date];
        NSArray *keysSortedByDate = [self->_dates keysSortedByValueUsingSelector:@selector(compare:)];
        
        for (NSString *key in keysSortedByDate) {
            NSURL *fileURL = [self encodedFileURLForKey:key];
            // If the cache should behave like a TTL cache, then only fetch the object if there's a valid ageLimit and  the object is still alive
            if (!self->_ttlCache || self->_ageLimit <= 0 || fabs([[_dates objectForKey:key] timeIntervalSinceDate:now]) < self->_ageLimit) {
                block(self, key, nil, fileURL);
            }
        }
    [self unlock];
    
    [task end];
}

#pragma mark - Public Thread Safe Accessors -

- (PINDiskCacheObjectBlock)willAddObjectBlock
{
    PINDiskCacheObjectBlock block = nil;
    
    [self lock];
        block = _willAddObjectBlock;
    [self unlock];
    
    return block;
}

- (void)setWillAddObjectBlock:(PINDiskCacheObjectBlock)block
{
    __weak PINDiskCache *weakSelf = self;
    
    dispatch_async(_asyncQueue, ^{
        PINDiskCache *strongSelf = weakSelf;
        if (!strongSelf)
            return;
        [strongSelf lock];
            strongSelf->_willAddObjectBlock = [block copy];
        [strongSelf unlock];
    });
}

- (PINDiskCacheObjectBlock)willRemoveObjectBlock
{
    PINDiskCacheObjectBlock block = nil;
    
    [self lock];
        block = _willRemoveObjectBlock;
    [self unlock];
    
    return block;
}

- (void)setWillRemoveObjectBlock:(PINDiskCacheObjectBlock)block
{
    __weak PINDiskCache *weakSelf = self;
    
    dispatch_async(_asyncQueue, ^{
        PINDiskCache *strongSelf = weakSelf;
        if (!strongSelf)
            return;
        
        [strongSelf lock];
            strongSelf->_willRemoveObjectBlock = [block copy];
        [strongSelf unlock];
    });
}

- (PINDiskCacheBlock)willRemoveAllObjectsBlock
{
    PINDiskCacheBlock block = nil;
    
    [self lock];
        block = _willRemoveAllObjectsBlock;
    [self unlock];
    
    return block;
}

- (void)setWillRemoveAllObjectsBlock:(PINDiskCacheBlock)block
{
    __weak PINDiskCache *weakSelf = self;
    
    dispatch_async(_asyncQueue, ^{
        PINDiskCache *strongSelf = weakSelf;
        if (!strongSelf)
            return;
        
        [strongSelf lock];
            strongSelf->_willRemoveAllObjectsBlock = [block copy];
        [strongSelf unlock];
    });
}

- (PINDiskCacheObjectBlock)didAddObjectBlock
{
    PINDiskCacheObjectBlock block = nil;
    
    [self lock];
        block = _didAddObjectBlock;
    [self unlock];
    
    return block;
}

- (void)setDidAddObjectBlock:(PINDiskCacheObjectBlock)block
{
    __weak PINDiskCache *weakSelf = self;
    
    dispatch_async(_asyncQueue, ^{
        PINDiskCache *strongSelf = weakSelf;
        if (!strongSelf)
            return;
        
        [strongSelf lock];
            strongSelf->_didAddObjectBlock = [block copy];
        [strongSelf unlock];
    });
}

- (PINDiskCacheObjectBlock)didRemoveObjectBlock
{
    PINDiskCacheObjectBlock block = nil;
    
    [self lock];
        block = _didRemoveObjectBlock;
    [self unlock];
    
    return block;
}

- (void)setDidRemoveObjectBlock:(PINDiskCacheObjectBlock)block
{
    __weak PINDiskCache *weakSelf = self;
    
    dispatch_async(_asyncQueue, ^{
        PINDiskCache *strongSelf = weakSelf;
        if (!strongSelf)
            return;
        
        [strongSelf lock];
            strongSelf->_didRemoveObjectBlock = [block copy];
        [strongSelf unlock];
    });
}

- (PINDiskCacheBlock)didRemoveAllObjectsBlock
{
    PINDiskCacheBlock block = nil;
    
    [self lock];
        block = _didRemoveAllObjectsBlock;
    [self unlock];
    
    return block;
}

- (void)setDidRemoveAllObjectsBlock:(PINDiskCacheBlock)block
{
    __weak PINDiskCache *weakSelf = self;
    
    dispatch_async(_asyncQueue, ^{
        PINDiskCache *strongSelf = weakSelf;
        if (!strongSelf)
            return;
        
        [strongSelf lock];
            strongSelf->_didRemoveAllObjectsBlock = [block copy];
        [strongSelf unlock];
    });
}

- (NSUInteger)byteLimit
{
    NSUInteger byteLimit;
    
    [self lock];
        byteLimit = _byteLimit;
    [self unlock];
    
    return byteLimit;
}

- (void)setByteLimit:(NSUInteger)byteLimit
{
    __weak PINDiskCache *weakSelf = self;
    
    dispatch_async(_asyncQueue, ^{
        PINDiskCache *strongSelf = weakSelf;
        if (!strongSelf)
            return;
        
        [strongSelf lock];
        strongSelf->_byteLimit = byteLimit;
        
        if (byteLimit > 0)
            [strongSelf trimDiskToSizeByDate:byteLimit];
        [strongSelf unlock];
    });
}

- (NSTimeInterval)ageLimit
{
    NSTimeInterval ageLimit;
    
    [self lock];
        ageLimit = _ageLimit;
    [self unlock];
    
    return ageLimit;
}

- (void)setAgeLimit:(NSTimeInterval)ageLimit
{
    __weak PINDiskCache *weakSelf = self;
    
    dispatch_async(_asyncQueue, ^{
        PINDiskCache *strongSelf = weakSelf;
        if (!strongSelf)
            return;
        
        [strongSelf lock];
            strongSelf->_ageLimit = ageLimit;
        [strongSelf unlock];
        
        [strongSelf trimToAgeLimitRecursively];
    });
}

- (BOOL)isTTLCache {
    BOOL isTTLCache;
    
    [self lock];
        isTTLCache = _ttlCache;
    [self unlock];
  
    return isTTLCache;
}

- (void)setTtlCache:(BOOL)ttlCache {
    __weak PINDiskCache *weakSelf = self;

    dispatch_async(_asyncQueue, ^{
        PINDiskCache *strongSelf = weakSelf;
        if (!strongSelf)
            return;

        [strongSelf lock];
            strongSelf->_ttlCache = ttlCache;
        [strongSelf unlock];
    });
}

#if TARGET_OS_IPHONE
- (NSDataWritingOptions)writingProtectionOption {
    NSDataWritingOptions option;
  
    [self lock];
        option = _writingProtectionOption;
    [self unlock];
  
    return option;
}

- (void)setWritingProtectionOption:(NSDataWritingOptions)writingProtectionOption {
  __weak PINDiskCache *weakSelf = self;
  
  dispatch_async(_asyncQueue, ^{
    PINDiskCache *strongSelf = weakSelf;
    if (!strongSelf)
      return;
    
    NSDataWritingOptions option = NSDataWritingFileProtectionMask & writingProtectionOption;
    
    [strongSelf lock];
    strongSelf->_writingProtectionOption = option;
    [strongSelf unlock];
  });
}
#endif

- (void)lock
{
    [_instanceLock lockWhenCondition:PINDiskCacheConditionReady];
}

- (void)unlock
{
    [_instanceLock unlockWithCondition:PINDiskCacheConditionReady];
}

@end

@implementation PINBackgroundTask

+ (BOOL)isAppExtension {

    static BOOL isExtension;
    static dispatch_once_t onceToken;

    dispatch_once(&onceToken, ^{
        NSDictionary *extensionDictionary = [[NSBundle mainBundle] infoDictionary][@"NSExtension"];
        isExtension = [extensionDictionary isKindOfClass:[NSDictionary class]];
    });

    return isExtension;
}

- (instancetype)init
{
    if (self = [super init]) {
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_4_0 && !TARGET_OS_WATCH
        _taskID = UIBackgroundTaskInvalid;
#endif
    }
    return self;
}

+ (instancetype)start
{
    PINBackgroundTask *task = nil;
    
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_4_0 && !TARGET_OS_WATCH
    if ([self.class isAppExtension]) {
        return task;
    }
    
    task = [[self alloc] init];
    
    UIApplication *sharedApplication = [UIApplication performSelector:@selector(sharedApplication)];
    task.taskID = [sharedApplication beginBackgroundTaskWithExpirationHandler:^{
        UIBackgroundTaskIdentifier taskID = task.taskID;
        task.taskID = UIBackgroundTaskInvalid;
        [sharedApplication endBackgroundTask:taskID];
    }];
#endif
    
    return task;
}

- (void)end
{
#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_4_0 && !TARGET_OS_WATCH
    if ([self.class isAppExtension]) {
        return;
    }
    
    UIBackgroundTaskIdentifier taskID = self.taskID;
    self.taskID = UIBackgroundTaskInvalid;
    
    UIApplication *sharedApplication = [UIApplication performSelector:@selector(sharedApplication)];
    [sharedApplication endBackgroundTask:taskID];
#endif
}

@end

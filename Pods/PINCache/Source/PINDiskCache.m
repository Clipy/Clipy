//  PINCache is a modified version of TMCache
//  Modifications by Garrett Moon
//  Copyright (c) 2015 Pinterest. All rights reserved.

#import "PINDiskCache.h"

#if __IPHONE_OS_VERSION_MIN_REQUIRED >= __IPHONE_4_0
#import <UIKit/UIKit.h>
#endif

#import <pthread.h>
#import <sys/xattr.h>

#if SWIFT_PACKAGE
@import PINOperation;
#else
#import <PINOperation/PINOperation.h>
#endif

#define PINDiskCacheError(error) if (error) { NSLog(@"%@ (%d) ERROR: %@", \
[[NSString stringWithUTF8String:__FILE__] lastPathComponent], \
__LINE__, [error localizedDescription]); }

#define PINDiskCacheException(exception) if (exception) { NSAssert(NO, [exception reason]); }

const char * PINDiskCacheAgeLimitAttributeName = "com.pinterest.PINDiskCache.ageLimit";
NSString * const PINDiskCacheErrorDomain = @"com.pinterest.PINDiskCache";
NSErrorUserInfoKey const PINDiskCacheErrorReadFailureCodeKey = @"PINDiskCacheErrorReadFailureCodeKey";
NSErrorUserInfoKey const PINDiskCacheErrorWriteFailureCodeKey = @"PINDiskCacheErrorWriteFailureCodeKey";
NSString * const PINDiskCachePrefix = @"com.pinterest.PINDiskCache";
static NSString * const PINDiskCacheSharedName = @"PINDiskCacheShared";

static NSString * const PINDiskCacheOperationIdentifierTrimToDate = @"PINDiskCacheOperationIdentifierTrimToDate";
static NSString * const PINDiskCacheOperationIdentifierTrimToSize = @"PINDiskCacheOperationIdentifierTrimToSize";
static NSString * const PINDiskCacheOperationIdentifierTrimToSizeByDate = @"PINDiskCacheOperationIdentifierTrimToSizeByDate";

typedef NS_ENUM(NSUInteger, PINDiskCacheCondition) {
    PINDiskCacheConditionNotReady = 0,
    PINDiskCacheConditionReady = 1,
};

static PINOperationDataCoalescingBlock PINDiskTrimmingSizeCoalescingBlock = ^id(NSNumber *existingSize, NSNumber *newSize) {
    NSComparisonResult result = [existingSize compare:newSize];
    return (result == NSOrderedDescending) ? newSize : existingSize;
};

static PINOperationDataCoalescingBlock PINDiskTrimmingDateCoalescingBlock = ^id(NSDate *existingDate, NSDate *newDate) {
    NSComparisonResult result = [existingDate compare:newDate];
    return (result == NSOrderedDescending) ? newDate : existingDate;
};

const char * PINDiskCacheFileSystemRepresentation(NSURL *url)
{
#ifdef __MAC_10_13 // Xcode >= 9
    // -fileSystemRepresentation is available on macOS >= 10.9
    if (@available(macOS 10.9, iOS 7.0, watchOS 2.0, tvOS 9.0, *)) {
      return url.fileSystemRepresentation;
    }
#endif
    return [url.path cStringUsingEncoding:NSUTF8StringEncoding];
}

@interface PINDiskCacheMetadata : NSObject
// When the object was added to the disk cache
@property (nonatomic, strong) NSDate *createdDate;
// Last time the object was accessed
@property (nonatomic, strong) NSDate *lastModifiedDate;
@property (nonatomic, strong) NSNumber *size;
// Age limit is used in conjuction with ttl
@property (nonatomic) NSTimeInterval ageLimit;
@end

@interface PINDiskCache () {
    PINDiskCacheSerializerBlock _serializer;
    PINDiskCacheDeserializerBlock _deserializer;
    
    PINDiskCacheKeyEncoderBlock _keyEncoder;
    PINDiskCacheKeyDecoderBlock _keyDecoder;
}

@property (assign, nonatomic) pthread_mutex_t mutex;
@property (copy, nonatomic) NSString *name;
@property (assign) NSUInteger byteCount;
@property (strong, nonatomic) NSURL *cacheURL;
@property (strong, nonatomic) PINOperationQueue *operationQueue;
@property (strong, nonatomic) NSMutableDictionary <NSString *, PINDiskCacheMetadata *> *metadata;
@property (assign, nonatomic) pthread_cond_t diskWritableCondition;
@property (assign, nonatomic) BOOL diskWritable;
@property (assign, nonatomic) pthread_cond_t diskStateKnownCondition;
@property (assign, nonatomic) BOOL diskStateKnown;
@property (assign, nonatomic) BOOL writingProtectionOptionSet;
@end

@implementation PINDiskCache

static NSURL *_sharedTrashURL;

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
@synthesize writingProtectionOptionSet = _writingProtectionOptionSet;
#endif

#pragma mark - Initialization -

- (void)dealloc
{
    __unused int result = pthread_mutex_destroy(&_mutex);
    NSCAssert(result == 0, @"Failed to destroy lock in PINDiskCache %p. Code: %d", (void *)self, result);
    pthread_cond_destroy(&_diskWritableCondition);
    pthread_cond_destroy(&_diskStateKnownCondition);
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
    return [self initWithName:name rootPath:rootPath serializer:nil deserializer:nil];
}

- (instancetype)initWithName:(NSString *)name rootPath:(NSString *)rootPath serializer:(PINDiskCacheSerializerBlock)serializer deserializer:(PINDiskCacheDeserializerBlock)deserializer
{
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
    return [self initWithName:name rootPath:rootPath serializer:serializer deserializer:deserializer operationQueue:[PINOperationQueue sharedOperationQueue]];
#pragma clang diagnostic pop
}

- (instancetype)initWithName:(NSString *)name
                    rootPath:(NSString *)rootPath
                  serializer:(PINDiskCacheSerializerBlock)serializer
                deserializer:(PINDiskCacheDeserializerBlock)deserializer
              operationQueue:(PINOperationQueue *)operationQueue
{
  return [self initWithName:name
                     prefix:PINDiskCachePrefix
                   rootPath:rootPath
                 serializer:serializer
               deserializer:deserializer
                 keyEncoder:nil
                 keyDecoder:nil
             operationQueue:operationQueue];
}

- (instancetype)initWithName:(NSString *)name
                      prefix:(NSString *)prefix
                    rootPath:(NSString *)rootPath
                  serializer:(PINDiskCacheSerializerBlock)serializer
                deserializer:(PINDiskCacheDeserializerBlock)deserializer
                  keyEncoder:(PINDiskCacheKeyEncoderBlock)keyEncoder
                  keyDecoder:(PINDiskCacheKeyDecoderBlock)keyDecoder
              operationQueue:(PINOperationQueue *)operationQueue
{
    return [self initWithName:name prefix:prefix
                     rootPath:rootPath
                   serializer:serializer
                 deserializer:deserializer
                   keyEncoder:keyEncoder
                   keyDecoder:keyDecoder
               operationQueue:operationQueue
                     ttlCache:NO];
}

- (instancetype)initWithName:(NSString *)name
                      prefix:(NSString *)prefix
                    rootPath:(NSString *)rootPath
                  serializer:(PINDiskCacheSerializerBlock)serializer
                deserializer:(PINDiskCacheDeserializerBlock)deserializer
                  keyEncoder:(PINDiskCacheKeyEncoderBlock)keyEncoder
                  keyDecoder:(PINDiskCacheKeyDecoderBlock)keyDecoder
              operationQueue:(PINOperationQueue *)operationQueue
                    ttlCache:(BOOL)ttlCache
{
    if (!name) {
        return nil;
    }

    NSAssert(((!serializer && !deserializer) || (serializer && deserializer)),
             @"PINDiskCache must be initialized with a serializer AND deserializer.");
    
    NSAssert(((!keyEncoder && !keyDecoder) || (keyEncoder && keyDecoder)),
              @"PINDiskCache must be initialized with an encoder AND decoder.");
    
    if (self = [super init]) {
        __unused int result = pthread_mutex_init(&_mutex, NULL);
        NSAssert(result == 0, @"Failed to init lock in PINDiskCache %@. Code: %d", self, result);
        
        _name = [name copy];
        _prefix = [prefix copy];
        _operationQueue = operationQueue;
        _ttlCache = ttlCache;
        _willAddObjectBlock = nil;
        _willRemoveObjectBlock = nil;
        _willRemoveAllObjectsBlock = nil;
        _didAddObjectBlock = nil;
        _didRemoveObjectBlock = nil;
        _didRemoveAllObjectsBlock = nil;
        
        _byteCount = 0;
        
        // 50 MB by default
        _byteLimit = 50 * 1024 * 1024;
        // 30 days by default
        _ageLimit = 60 * 60 * 24 * 30;
        
#if TARGET_OS_IPHONE
        _writingProtectionOptionSet = NO;
        // This is currently the default for files, but we'd rather not write it if it's unspecified.
        _writingProtectionOption = NSDataWritingFileProtectionCompleteUntilFirstUserAuthentication;
#endif
        
        _metadata = [[NSMutableDictionary alloc] init];
        _diskStateKnown = NO;
      
        _cacheURL = [[self class] cacheURLWithRootPath:rootPath prefix:_prefix name:_name];
        
        //setup serializers
        if(serializer) {
            _serializer = [serializer copy];
        } else {
            _serializer = self.defaultSerializer;
        }

        if(deserializer) {
            _deserializer = [deserializer copy];
        } else {
            _deserializer = self.defaultDeserializer;
        }
        
        //setup key encoder/decoder
        if(keyEncoder) {
            _keyEncoder = [keyEncoder copy];
        } else {
            _keyEncoder = self.defaultKeyEncoder;
        }
        
        if(keyDecoder) {
            _keyDecoder = [keyDecoder copy];
        } else {
            _keyDecoder = self.defaultKeyDecoder;
        }
        
        pthread_cond_init(&_diskWritableCondition, NULL);
        pthread_cond_init(&_diskStateKnownCondition, NULL);

        //we don't want to do anything without setting up the disk cache, but we also don't want to block init, it can take a while to initialize. This must *not* be done on _operationQueue because other operations added may hold the lock and fill up the queue.
        dispatch_async(dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^{
            [self lock];
                [self _locked_createCacheDirectory];
            [self unlock];
            [self initializeDiskProperties];
        });
    }
    return self;
}

- (NSString *)description
{
    return [[NSString alloc] initWithFormat:@"%@.%@.%p", PINDiskCachePrefix, _name, (__bridge void *)self];
}

+ (PINDiskCache *)sharedCache
{
    static PINDiskCache *cache;
    static dispatch_once_t predicate;
    
    dispatch_once(&predicate, ^{
        cache = [[PINDiskCache alloc] initWithName:PINDiskCacheSharedName];
    });
    
    return cache;
}

+ (NSURL *)cacheURLWithRootPath:(NSString *)rootPath prefix:(NSString *)prefix name:(NSString *)name
{
    NSString *pathComponent = [[NSString alloc] initWithFormat:@"%@.%@", prefix, name];
    return [NSURL fileURLWithPathComponents:@[ rootPath, pathComponent ]];
}

#pragma mark - Private Methods -

- (NSURL *)encodedFileURLForKey:(NSString *)key
{
    if (![key length])
        return nil;
    
    //Significantly improve performance by indicating that the URL will *not* result in a directory.
    //Also note that accessing _cacheURL is safe without the lock because it is only set on init.
    return [_cacheURL URLByAppendingPathComponent:[self encodedString:key] isDirectory:NO];
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
    return _keyEncoder(string);
}

- (NSString *)decodedString:(NSString *)string
{
    return _keyDecoder(string);
}

- (PINDiskCacheSerializerBlock)defaultSerializer
{
    return ^NSData*(id<NSCoding> object, NSString *key){
        if (@available(iOS 11.0, macOS 10.13, tvOS 11.0, watchOS 4.0, *)) {
            NSError *error = nil;
            NSData *data = [NSKeyedArchiver archivedDataWithRootObject:object requiringSecureCoding:NO error:&error];
            PINDiskCacheError(error);
            return data;
        } else {
            return [NSKeyedArchiver archivedDataWithRootObject:object];
        }
    };
}

- (PINDiskCacheDeserializerBlock)defaultDeserializer
{
    return ^id(NSData * data, NSString *key){
        return [NSKeyedUnarchiver unarchiveObjectWithData:data];
    };
}

- (PINDiskCacheKeyEncoderBlock)defaultKeyEncoder
{
    return ^NSString *(NSString *decodedKey) {
        if (![decodedKey length]) {
            return @"";
        }
        
        if (@available(macOS 10.9, iOS 7.0, tvOS 9.0, watchOS 2.0, *)) {
            NSString *encodedString = [decodedKey stringByAddingPercentEncodingWithAllowedCharacters:[[NSCharacterSet characterSetWithCharactersInString:@".:/%"] invertedSet]];
            return encodedString;
        } else {
            CFStringRef static const charsToEscape = CFSTR(".:/%");
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            CFStringRef escapedString = CFURLCreateStringByAddingPercentEscapes(kCFAllocatorDefault,
                                                                                (__bridge CFStringRef)decodedKey,
                                                                                NULL,
                                                                                charsToEscape,
                                                                                kCFStringEncodingUTF8);
#pragma clang diagnostic pop
            
            return (__bridge_transfer NSString *)escapedString;
        }
    };
}

- (PINDiskCacheKeyEncoderBlock)defaultKeyDecoder
{
    return ^NSString *(NSString *encodedKey) {
        if (![encodedKey length]) {
            return @"";
        }
        
        if (@available(macOS 10.9, iOS 7.0, tvOS 9.0, watchOS 2.0, *)) {
            return [encodedKey stringByRemovingPercentEncoding];
        } else {
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
            CFStringRef unescapedString = CFURLCreateStringByReplacingPercentEscapesUsingEncoding(kCFAllocatorDefault,
                                                                                                  (__bridge CFStringRef)encodedKey,
                                                                                                  CFSTR(""),
                                                                                                  kCFStringEncodingUTF8);
#pragma clang diagnostic pop
            return (__bridge_transfer NSString *)unescapedString;
        }
    };
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

+ (NSLock *)sharedLock
{
    static NSLock *sharedLock;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedLock = [NSLock new];
    });
    return sharedLock;
}

+ (NSURL *)sharedTrashURL
{
    NSURL *trashURL = nil;
    
    [[PINDiskCache sharedLock] lock];
        if (_sharedTrashURL == nil) {
            NSString *uniqueString = [[NSProcessInfo processInfo] globallyUniqueString];
            _sharedTrashURL = [[[NSURL alloc] initFileURLWithPath:NSTemporaryDirectory()] URLByAppendingPathComponent:uniqueString isDirectory:YES];
            
            NSError *error = nil;
            [[NSFileManager defaultManager] createDirectoryAtURL:_sharedTrashURL
                                     withIntermediateDirectories:YES
                                                      attributes:nil
                                                           error:&error];
            PINDiskCacheError(error);
        }
        trashURL = _sharedTrashURL;
    [[PINDiskCache sharedLock] unlock];
    
    return trashURL;
}

+ (BOOL)moveItemAtURLToTrash:(NSURL *)itemURL
{
    if (![[NSFileManager defaultManager] fileExistsAtPath:[itemURL path]])
        return NO;
    
    NSError *error = nil;
    NSString *uniqueString = [[NSProcessInfo processInfo] globallyUniqueString];
    NSURL *uniqueTrashURL = [[PINDiskCache sharedTrashURL] URLByAppendingPathComponent:uniqueString isDirectory:NO];
    BOOL moved = [[NSFileManager defaultManager] moveItemAtURL:itemURL toURL:uniqueTrashURL error:&error];
    PINDiskCacheError(error);
    return moved;
}

+ (void)emptyTrash
{
    dispatch_async([PINDiskCache sharedTrashQueue], ^{
        NSURL *trashURL = nil;
      
        // If _sharedTrashURL is unset, there's nothing left to do because it hasn't been accessed and therefore items haven't been added to it.
        // If it is set, we can just remove it.
        // We also need to nil out _sharedTrashURL so that a new one will be created if there's an attempt to move a new file to the trash.
        [[PINDiskCache sharedLock] lock];
            if (_sharedTrashURL != nil) {
                trashURL = _sharedTrashURL;
                _sharedTrashURL = nil;
            }
        [[PINDiskCache sharedLock] unlock];
        
        if (trashURL != nil) {
            NSError *removeTrashedItemError = nil;
            [[NSFileManager defaultManager] removeItemAtURL:trashURL error:&removeTrashedItemError];
            PINDiskCacheError(removeTrashedItemError);
        }
    });
}

#pragma mark - Private Queue Methods -

- (BOOL)_locked_createCacheDirectory
{
    BOOL created = NO;
    if ([[NSFileManager defaultManager] fileExistsAtPath:[_cacheURL path]] == NO) {
        NSError *error = nil;
        created = [[NSFileManager defaultManager] createDirectoryAtURL:_cacheURL
                                                withIntermediateDirectories:YES
                                                                 attributes:nil
                                                                      error:&error];
        PINDiskCacheError(error);
    }
    

    
    // while this may not be true if success is false, it's better than deadlocking later.
    _diskWritable = YES;
    pthread_cond_broadcast(&_diskWritableCondition);
    
    return created;
}

+ (NSArray *)resourceKeys
{
    static NSArray *resourceKeys = nil;
    static dispatch_once_t predicate;

    dispatch_once(&predicate, ^{
        resourceKeys = @[ NSURLCreationDateKey, NSURLContentModificationDateKey, NSURLTotalFileAllocatedSizeKey ];
    });

    return resourceKeys;
}

/**
 * @return File size in bytes.
 */
- (NSUInteger)_locked_initializeDiskPropertiesForFile:(NSURL *)fileURL fileKey:(NSString *)fileKey
{
    NSError *error = nil;

    NSDictionary *dictionary = [fileURL resourceValuesForKeys:[PINDiskCache resourceKeys] error:&error];
    PINDiskCacheError(error);

    if (_metadata[fileKey] == nil) {
        _metadata[fileKey] = [[PINDiskCacheMetadata alloc] init];
    }

    NSDate *createdDate = dictionary[NSURLCreationDateKey];
    if (createdDate && fileKey)
        _metadata[fileKey].createdDate = createdDate;

    NSDate *lastModifiedDate = dictionary[NSURLContentModificationDateKey];
    if (lastModifiedDate && fileKey)
        _metadata[fileKey].lastModifiedDate = lastModifiedDate;

    NSNumber *fileSize = dictionary[NSURLTotalFileAllocatedSizeKey];
    if (fileSize) {
        _metadata[fileKey].size = fileSize;
    }

    if (_ttlCache) {
        NSTimeInterval ageLimit;
        ssize_t res = getxattr(PINDiskCacheFileSystemRepresentation(fileURL), PINDiskCacheAgeLimitAttributeName, &ageLimit, sizeof(NSTimeInterval), 0, 0);
        if(res > 0) {
            _metadata[fileKey].ageLimit = ageLimit;
        } else if (res == -1) {
            // Ignore if the extended attribute was never recorded for this file.
            if (errno != ENOATTR) {
                NSDictionary<NSErrorUserInfoKey, id> *userInfo = @{ PINDiskCacheErrorReadFailureCodeKey : @(errno)};
                error = [NSError errorWithDomain:PINDiskCacheErrorDomain code:PINDiskCacheErrorReadFailure userInfo:userInfo];
                PINDiskCacheError(error);
            }
        }
    }

    return [fileSize unsignedIntegerValue];
}

- (void)initializeDiskProperties
{
    NSUInteger byteCount = 0;

    NSError *error = nil;
    
    [self lock];
        NSArray *files = [[NSFileManager defaultManager] contentsOfDirectoryAtURL:_cacheURL
                                                       includingPropertiesForKeys:[PINDiskCache resourceKeys]
                                                                          options:NSDirectoryEnumerationSkipsHiddenFiles
                                                                            error:&error];
    [self unlock];
    
    PINDiskCacheError(error);
    
    for (NSURL *fileURL in files) {
        NSString *fileKey = [self keyForEncodedFileURL:fileURL];
        // Continually grab and release lock while processing files to avoid contention
        [self lock];
        if (_metadata[fileKey] == nil) {
            byteCount += [self _locked_initializeDiskPropertiesForFile:fileURL fileKey:fileKey];
        }
        [self unlock];
    }
    
    [self lock];
        if (byteCount > 0)
            _byteCount = byteCount;
    
        if (self->_byteLimit > 0 && self->_byteCount > self->_byteLimit)
            [self trimToSizeByDateAsync:self->_byteLimit completion:nil];

        if (self->_ttlCache)
            [self removeExpiredObjectsAsync:nil];
    
        _diskStateKnown = YES;
        pthread_cond_broadcast(&_diskStateKnownCondition);
    [self unlock];
}

- (void)asynchronouslySetFileModificationDate:(NSDate *)date forURL:(NSURL *)fileURL
{
    [self.operationQueue scheduleOperation:^{
        [self lockForWriting];
            [self _locked_setFileModificationDate:date forURL:fileURL];
        [self unlock];
    } withPriority:PINOperationQueuePriorityLow];
}

- (BOOL)_locked_setFileModificationDate:(NSDate *)date forURL:(NSURL *)fileURL
{
    if (!date || !fileURL) {
        return NO;
    }
    
    NSError *error = nil;
    BOOL success = [[NSFileManager defaultManager] setAttributes:@{ NSFileModificationDate: date }
                                                    ofItemAtPath:[fileURL path]
                                                           error:&error];
    PINDiskCacheError(error);
    
    return success;
}

- (void)asynchronouslySetAgeLimit:(NSTimeInterval)ageLimit forURL:(NSURL *)fileURL
{
    [self.operationQueue scheduleOperation:^{
        [self lockForWriting];
            [self _locked_setAgeLimit:ageLimit forURL:fileURL];
        [self unlock];
    } withPriority:PINOperationQueuePriorityLow];
}

- (BOOL)_locked_setAgeLimit:(NSTimeInterval)ageLimit forURL:(NSURL *)fileURL
{
    if (!fileURL) {
        return NO;
    }

    NSError *error = nil;
    if (ageLimit <= 0.0) {
        if (removexattr(PINDiskCacheFileSystemRepresentation(fileURL), PINDiskCacheAgeLimitAttributeName, 0) != 0) {
          // Ignore if the extended attribute was never recorded for this file.
          if (errno != ENOATTR) {
            NSDictionary<NSErrorUserInfoKey, id> *userInfo = @{ PINDiskCacheErrorWriteFailureCodeKey : @(errno)};
            error = [NSError errorWithDomain:PINDiskCacheErrorDomain code:PINDiskCacheErrorWriteFailure userInfo:userInfo];
            PINDiskCacheError(error);
          }
        }
    } else {
        if (setxattr(PINDiskCacheFileSystemRepresentation(fileURL), PINDiskCacheAgeLimitAttributeName, &ageLimit, sizeof(NSTimeInterval), 0, 0) != 0) {
            NSDictionary<NSErrorUserInfoKey, id> *userInfo = @{ PINDiskCacheErrorWriteFailureCodeKey : @(errno)};
            error = [NSError errorWithDomain:PINDiskCacheErrorDomain code:PINDiskCacheErrorWriteFailure userInfo:userInfo];
            PINDiskCacheError(error);
        }
    }

    if (!error) {
        NSString *key = [self keyForEncodedFileURL:fileURL];
        if (key) {
            _metadata[key].ageLimit = ageLimit;
        }
    }

    return !error;
}

- (BOOL)removeFileAndExecuteBlocksForKey:(NSString *)key
{
    NSURL *fileURL = [self encodedFileURLForKey:key];
    if (!fileURL) {
        return NO;
    }

    // We only need to lock until writable at the top because once writable, always writable
    [self lockForWriting];
        if (![[NSFileManager defaultManager] fileExistsAtPath:[fileURL path]]) {
            [self unlock];
            return NO;
        }
    
        PINDiskCacheObjectBlock willRemoveObjectBlock = _willRemoveObjectBlock;
        if (willRemoveObjectBlock) {
            [self unlock];
            willRemoveObjectBlock(self, key, nil);
            [self lock];
        }
        
        BOOL trashed = [PINDiskCache moveItemAtURLToTrash:fileURL];
        if (!trashed) {
            [self unlock];
            return NO;
        }
    
        [PINDiskCache emptyTrash];
        
        NSNumber *byteSize = _metadata[key].size;
        if (byteSize)
            self.byteCount = _byteCount - [byteSize unsignedIntegerValue]; // atomic
        
        [_metadata removeObjectForKey:key];
    
        PINDiskCacheObjectBlock didRemoveObjectBlock = _didRemoveObjectBlock;
        if (didRemoveObjectBlock) {
            [self unlock];
            _didRemoveObjectBlock(self, key, nil);
            [self lock];
        }
    
    [self unlock];
    
    return YES;
}

- (void)trimDiskToSize:(NSUInteger)trimByteCount
{
    NSMutableArray *keysToRemove = nil;
    
    [self lockForWriting];
        if (_byteCount > trimByteCount) {
            keysToRemove = [[NSMutableArray alloc] init];
            
            NSArray *keysSortedBySize = [_metadata keysSortedByValueUsingComparator:^NSComparisonResult(PINDiskCacheMetadata * _Nonnull obj1, PINDiskCacheMetadata * _Nonnull obj2) {
                return [obj1.size compare:obj2.size];
            }];
            
            NSUInteger bytesSaved = 0;
            for (NSString *key in [keysSortedBySize reverseObjectEnumerator]) { // largest objects first
                [keysToRemove addObject:key];
                NSNumber *byteSize = _metadata[key].size;
                if (byteSize) {
                    bytesSaved += [byteSize unsignedIntegerValue];
                }
                if (_byteCount - bytesSaved <= trimByteCount) {
                    break;
                }
            }
        }
    [self unlock];
    
    for (NSString *key in keysToRemove) {
        [self removeFileAndExecuteBlocksForKey:key];
    }
}

// This is the default trimming method which happens automatically
- (void)trimDiskToSizeByDate:(NSUInteger)trimByteCount
{
    if (self.isTTLCache) {
        [self removeExpiredObjects];
    }

    NSMutableArray *keysToRemove = nil;
  
    [self lockForWriting];
        if (_byteCount > trimByteCount) {
            keysToRemove = [[NSMutableArray alloc] init];
            
            // last modified represents last access.
            NSArray *keysSortedByLastModifiedDate = [_metadata keysSortedByValueUsingComparator:^NSComparisonResult(PINDiskCacheMetadata * _Nonnull obj1, PINDiskCacheMetadata * _Nonnull obj2) {
                return [obj1.lastModifiedDate compare:obj2.lastModifiedDate];
            }];
            
            NSUInteger bytesSaved = 0;
            // objects accessed last first.
            for (NSString *key in keysSortedByLastModifiedDate) {
                [keysToRemove addObject:key];
                NSNumber *byteSize = _metadata[key].size;
                if (byteSize) {
                    bytesSaved += [byteSize unsignedIntegerValue];
                }
                if (_byteCount - bytesSaved <= trimByteCount) {
                    break;
                }
            }
        }
    [self unlock];
    
    for (NSString *key in keysToRemove) {
        [self removeFileAndExecuteBlocksForKey:key];
    }
}

- (void)trimDiskToDate:(NSDate *)trimDate
{
    [self lockForWriting];
        NSArray *keysSortedByCreatedDate = [_metadata keysSortedByValueUsingComparator:^NSComparisonResult(PINDiskCacheMetadata * _Nonnull obj1, PINDiskCacheMetadata * _Nonnull obj2) {
            return [obj1.createdDate compare:obj2.createdDate];
        }];
    
        NSMutableArray *keysToRemove = [[NSMutableArray alloc] init];
        
        for (NSString *key in keysSortedByCreatedDate) { // oldest files first
            NSDate *createdDate = _metadata[key].createdDate;
            if (!createdDate || _metadata[key].ageLimit > 0.0)
                continue;
            
            if ([createdDate compare:trimDate] == NSOrderedAscending) { // older than trim date
                [keysToRemove addObject:key];
            } else {
                break;
            }
        }
    [self unlock];
    
    for (NSString *key in keysToRemove) {
        [self removeFileAndExecuteBlocksForKey:key];
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
    [self trimToDateAsync:date completion:nil];
    
    dispatch_time_t time = dispatch_time(DISPATCH_TIME_NOW, (int64_t)(ageLimit * NSEC_PER_SEC));
    dispatch_after(time, dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0), ^(void) {
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

- (void)lockFileAccessWhileExecutingBlockAsync:(PINCacheBlock)block
{
    if (block == nil) {
      return;
    }

    [self.operationQueue scheduleOperation:^{
        [self lockForWriting];
            block(self);
        [self unlock];
    } withPriority:PINOperationQueuePriorityLow];
}

- (void)containsObjectForKeyAsync:(NSString *)key completion:(PINDiskCacheContainsBlock)block
{
    if (!key || !block)
        return;
    
    [self.operationQueue scheduleOperation:^{
        block([self containsObjectForKey:key]);
    } withPriority:PINOperationQueuePriorityLow];
}

- (void)objectForKeyAsync:(NSString *)key completion:(PINDiskCacheObjectBlock)block
{
    [self.operationQueue scheduleOperation:^{
        NSURL *fileURL = nil;
        id <NSCoding> object = [self objectForKey:key fileURL:&fileURL];
        
        block(self, key, object);
    } withPriority:PINOperationQueuePriorityLow];
}

- (void)fileURLForKeyAsync:(NSString *)key completion:(PINDiskCacheFileURLBlock)block
{
    if (block == nil) {
      return;
    }

    [self.operationQueue scheduleOperation:^{
        NSURL *fileURL = [self fileURLForKey:key];
      
        [self lockForWriting];
            block(key, fileURL);
        [self unlock];
    } withPriority:PINOperationQueuePriorityLow];
}

- (void)setObjectAsync:(id <NSCoding>)object forKey:(NSString *)key completion:(PINDiskCacheObjectBlock)block
{
    [self setObjectAsync:object forKey:key withAgeLimit:0.0 completion:(PINDiskCacheObjectBlock)block];
}

- (void)setObjectAsync:(id <NSCoding>)object forKey:(NSString *)key withAgeLimit:(NSTimeInterval)ageLimit completion:(nullable PINDiskCacheObjectBlock)block
{
    [self.operationQueue scheduleOperation:^{
        NSURL *fileURL = nil;
        [self setObject:object forKey:key withAgeLimit:ageLimit fileURL:&fileURL];
        
        if (block) {
            block(self, key, object);
        }
    } withPriority:PINOperationQueuePriorityLow];
}

- (void)setObjectAsync:(id <NSCoding>)object forKey:(NSString *)key withCost:(NSUInteger)cost completion:(nullable PINCacheObjectBlock)block
{
    [self setObjectAsync:object forKey:key completion:(PINDiskCacheObjectBlock)block];
}

- (void)setObjectAsync:(id <NSCoding>)object forKey:(NSString *)key withCost:(NSUInteger)cost ageLimit:(NSTimeInterval)ageLimit completion:(nullable PINCacheObjectBlock)block
{
    [self setObjectAsync:object forKey:key withAgeLimit:ageLimit completion:(PINDiskCacheObjectBlock)block];
}

- (void)removeObjectForKeyAsync:(NSString *)key completion:(PINDiskCacheObjectBlock)block
{
    [self.operationQueue scheduleOperation:^{
        NSURL *fileURL = nil;
        [self removeObjectForKey:key fileURL:&fileURL];
        
        if (block) {
            block(self, key, nil);
        }
    } withPriority:PINOperationQueuePriorityLow];
}

- (void)trimToSizeAsync:(NSUInteger)trimByteCount completion:(PINCacheBlock)block
{
    PINOperationBlock operation = ^(id data) {
        [self trimToSize:((NSNumber *)data).unsignedIntegerValue];
    };
  
    dispatch_block_t completion = nil;
    if (block) {
        completion = ^{
            block(self);
        };
    }
    
    [self.operationQueue scheduleOperation:operation
                              withPriority:PINOperationQueuePriorityLow
                                identifier:PINDiskCacheOperationIdentifierTrimToSize
                            coalescingData:[NSNumber numberWithUnsignedInteger:trimByteCount]
                       dataCoalescingBlock:PINDiskTrimmingSizeCoalescingBlock
                                completion:completion];
}

- (void)trimToDateAsync:(NSDate *)trimDate completion:(PINCacheBlock)block
{
    PINOperationBlock operation = ^(id data){
        [self trimToDate:(NSDate *)data];
    };
    
    dispatch_block_t completion = nil;
    if (block) {
        completion = ^{
            block(self);
        };
    }
    
    [self.operationQueue scheduleOperation:operation
                              withPriority:PINOperationQueuePriorityLow
                                identifier:PINDiskCacheOperationIdentifierTrimToDate
                            coalescingData:trimDate
                       dataCoalescingBlock:PINDiskTrimmingDateCoalescingBlock
                                completion:completion];
}

- (void)trimToSizeByDateAsync:(NSUInteger)trimByteCount completion:(PINCacheBlock)block
{
    PINOperationBlock operation = ^(id data){
        [self trimToSizeByDate:((NSNumber *)data).unsignedIntegerValue];
    };
    
    dispatch_block_t completion = nil;
    if (block) {
        completion = ^{
            block(self);
        };
    }
    
    [self.operationQueue scheduleOperation:operation
                              withPriority:PINOperationQueuePriorityLow
                                identifier:PINDiskCacheOperationIdentifierTrimToSizeByDate
                            coalescingData:[NSNumber numberWithUnsignedInteger:trimByteCount]
                       dataCoalescingBlock:PINDiskTrimmingSizeCoalescingBlock
                                completion:completion];
}

- (void)removeExpiredObjectsAsync:(PINCacheBlock)block
{
    [self.operationQueue scheduleOperation:^{
        [self removeExpiredObjects];

        if (block) {
            block(self);
        }
    } withPriority:PINOperationQueuePriorityLow];
}

- (void)removeAllObjectsAsync:(PINCacheBlock)block
{
    [self.operationQueue scheduleOperation:^{
        [self removeAllObjects];
        
        if (block) {
            block(self);
        }
    } withPriority:PINOperationQueuePriorityLow];
}

- (void)enumerateObjectsWithBlockAsync:(PINDiskCacheFileURLEnumerationBlock)block completionBlock:(PINCacheBlock)completionBlock
{
    [self.operationQueue scheduleOperation:^{
        [self enumerateObjectsWithBlock:block];
        
        if (completionBlock) {
            completionBlock(self);
        }
    } withPriority:PINOperationQueuePriorityLow];
}

#pragma mark - Public Synchronous Methods -

- (void)synchronouslyLockFileAccessWhileExecutingBlock:(PIN_NOESCAPE PINCacheBlock)block
{
    if (block) {
        [self lockForWriting];
            block(self);
        [self unlock];
    }
}

- (BOOL)containsObjectForKey:(NSString *)key
{
    [self lock];
        if (_metadata[key] != nil || _diskStateKnown == NO) {
            BOOL objectExpired = NO;
            if (self->_ttlCache && _metadata[key].createdDate != nil) {
                NSTimeInterval ageLimit = _metadata[key].ageLimit > 0.0 ? _metadata[key].ageLimit : self->_ageLimit;
                objectExpired = ageLimit > 0 && fabs([_metadata[key].createdDate timeIntervalSinceDate:[NSDate date]]) > ageLimit;
            }
            [self unlock];
            return (!objectExpired && [self fileURLForKey:key updateFileModificationDate:NO] != nil);
        }
    [self unlock];
    return NO;
}

- (nullable id<NSCoding>)objectForKey:(NSString *)key
{
    return [self objectForKey:key fileURL:nil];
}

- (id)objectForKeyedSubscript:(NSString *)key
{
    return [self objectForKey:key];
}

- (nullable id <NSCoding>)objectForKey:(NSString *)key fileURL:(NSURL **)outFileURL
{
    [self lock];
        BOOL containsKey = _metadata[key] != nil || _diskStateKnown == NO;
    [self unlock];

    if (!key || !containsKey)
        return nil;
    
    id <NSCoding> object = nil;
    NSURL *fileURL = [self encodedFileURLForKey:key];
    
    NSDate *now = [NSDate date];
    [self lock];
        if (self->_ttlCache) {
            if (!_diskStateKnown) {
                if (_metadata[key] == nil) {
                    NSString *fileKey = [self keyForEncodedFileURL:fileURL];
                    [self _locked_initializeDiskPropertiesForFile:fileURL fileKey:fileKey];
                }
            }
        }

        NSTimeInterval ageLimit = _metadata[key].ageLimit > 0.0 ? _metadata[key].ageLimit : self->_ageLimit;
        if (!self->_ttlCache || ageLimit <= 0 || fabs([_metadata[key].createdDate timeIntervalSinceDate:now]) < ageLimit) {
            // If the cache should behave like a TTL cache, then only fetch the object if there's a valid ageLimit and  the object is still alive
            
            NSData *objectData = [[NSData alloc] initWithContentsOfFile:[fileURL path]];
          
            if (objectData) {
              //Be careful with locking below. We unlock here so that we're not locked while deserializing, we re-lock after.
              [self unlock];
              @try {
                  object = _deserializer(objectData, key);
              }
              @catch (NSException *exception) {
                  NSError *error = nil;
                  [self lock];
                      [[NSFileManager defaultManager] removeItemAtPath:[fileURL path] error:&error];
                  [self unlock];
                  PINDiskCacheError(error)
                  PINDiskCacheException(exception);
              }
              [self lock];
            }
            if (object) {
                _metadata[key].lastModifiedDate = now;
                [self asynchronouslySetFileModificationDate:now forURL:fileURL];
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
    
    NSDate *now = [NSDate date];
    NSURL *fileURL = [self encodedFileURLForKey:key];
    
    [self lockForWriting];
        if (fileURL.path && [[NSFileManager defaultManager] fileExistsAtPath:fileURL.path]) {
            if (updateFileModificationDate) {
                _metadata[key].lastModifiedDate = now;
                [self asynchronouslySetFileModificationDate:now forURL:fileURL];
            }
        } else {
            fileURL = nil;
        }
    [self unlock];
    return fileURL;
}

- (void)setObject:(id <NSCoding>)object forKey:(NSString *)key
{
    [self setObject:object forKey:key withAgeLimit:0.0];
}

- (void)setObject:(id <NSCoding>)object forKey:(NSString *)key withAgeLimit:(NSTimeInterval)ageLimit
{
    [self setObject:object forKey:key withAgeLimit:ageLimit fileURL:nil];
}

- (void)setObject:(id <NSCoding>)object forKey:(NSString *)key withCost:(NSUInteger)cost ageLimit:(NSTimeInterval)ageLimit
{
    [self setObject:object forKey:key withAgeLimit:ageLimit];
}

- (void)setObject:(id <NSCoding>)object forKey:(NSString *)key withCost:(NSUInteger)cost
{
    [self setObject:object forKey:key];
}

- (void)setObject:(id)object forKeyedSubscript:(NSString *)key
{
    if (object == nil) {
        [self removeObjectForKey:key];
    } else {
        [self setObject:object forKey:key];
    }
}

- (void)setObject:(id <NSCoding>)object forKey:(NSString *)key withAgeLimit:(NSTimeInterval)ageLimit fileURL:(NSURL **)outFileURL
{
    NSAssert(ageLimit <= 0.0 || (ageLimit > 0.0 && _ttlCache), @"ttlCache must be set to YES if setting an object-level age limit.");

    if (!key || !object)
        return;
    
    NSDataWritingOptions writeOptions = NSDataWritingAtomic;
    #if TARGET_OS_IPHONE
    if (self.writingProtectionOptionSet) {
        writeOptions |= self.writingProtectionOption;
    }
    #endif
  
    // Remain unlocked here so that we're not locked while serializing.
    NSData *data = _serializer(object, key);
    NSURL *fileURL = nil;

    NSUInteger byteLimit = self.byteLimit;
    if (data.length <= byteLimit || byteLimit == 0) {
        // The cache is large enough to fit this object (although we may need to evict others).
        fileURL = [self encodedFileURLForKey:key];
    } else {
        // The cache isn't large enough to fit this object (even if all others were evicted).
        // We should not write it to disk because it will be deleted immediately after.
        if (outFileURL) {
            *outFileURL = nil;
        }
        return;
    }

    [self lockForWriting];
        PINDiskCacheObjectBlock willAddObjectBlock = self->_willAddObjectBlock;
        if (willAddObjectBlock) {
            [self unlock];
                willAddObjectBlock(self, key, object);
            [self lock];
        }
    
        NSError *writeError = nil;
        BOOL written = [data writeToURL:fileURL options:writeOptions error:&writeError];
        PINDiskCacheError(writeError);
        
        if (written) {
            if (_metadata[key] == nil) {
                _metadata[key] = [[PINDiskCacheMetadata alloc] init];
            }
            
            NSError *error = nil;
            NSDictionary *values = [fileURL resourceValuesForKeys:@[ NSURLCreationDateKey, NSURLContentModificationDateKey, NSURLTotalFileAllocatedSizeKey ] error:&error];
            PINDiskCacheError(error);
            
            NSNumber *diskFileSize = [values objectForKey:NSURLTotalFileAllocatedSizeKey];
            if (diskFileSize) {
                NSNumber *prevDiskFileSize = self->_metadata[key].size;
                if (prevDiskFileSize) {
                    self.byteCount = self->_byteCount - [prevDiskFileSize unsignedIntegerValue];
                }
                self->_metadata[key].size = diskFileSize;
                self.byteCount = self->_byteCount + [diskFileSize unsignedIntegerValue]; // atomic
            }
            NSDate *createdDate = [values objectForKey:NSURLCreationDateKey];
            if (createdDate) {
                self->_metadata[key].createdDate = createdDate;
            }
            NSDate *lastModifiedDate = [values objectForKey:NSURLContentModificationDateKey];
            if (lastModifiedDate) {
                self->_metadata[key].lastModifiedDate = lastModifiedDate;
            }
            [self asynchronouslySetAgeLimit:ageLimit forURL:fileURL];
            if (self->_byteLimit > 0 && self->_byteCount > self->_byteLimit)
                [self trimToSizeByDateAsync:self->_byteLimit completion:nil];
        } else {
            fileURL = nil;
        }
    
        PINDiskCacheObjectBlock didAddObjectBlock = self->_didAddObjectBlock;
        if (didAddObjectBlock) {
            [self unlock];
                didAddObjectBlock(self, key, object);
            [self lock];
        }
    [self unlock];
    
    if (outFileURL) {
        *outFileURL = fileURL;
    }
}

- (void)removeObjectForKey:(NSString *)key
{
    [self removeObjectForKey:key fileURL:nil];
}

- (void)removeObjectForKey:(NSString *)key fileURL:(NSURL **)outFileURL
{
    if (!key)
        return;
    
    NSURL *fileURL = nil;
    
    fileURL = [self encodedFileURLForKey:key];
    
    [self removeFileAndExecuteBlocksForKey:key];
    
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
    
    [self trimDiskToSize:trimByteCount];
}

- (void)trimToDate:(NSDate *)trimDate
{
    if (!trimDate)
        return;
    
    if ([trimDate isEqualToDate:[NSDate distantPast]]) {
        [self removeAllObjects];
        return;
    }
    
    [self trimDiskToDate:trimDate];
}

- (void)trimToSizeByDate:(NSUInteger)trimByteCount
{
    if (trimByteCount == 0) {
        [self removeAllObjects];
        return;
    }
    
    [self trimDiskToSizeByDate:trimByteCount];
}

- (void)removeExpiredObjects
{
    [self lockForWriting];
        NSDate *now = [NSDate date];
        NSMutableArray<NSString *> *expiredObjectKeys = [NSMutableArray array];
        [_metadata enumerateKeysAndObjectsUsingBlock:^(NSString * _Nonnull key, PINDiskCacheMetadata * _Nonnull obj, BOOL * _Nonnull stop) {
            NSTimeInterval ageLimit = obj.ageLimit > 0.0 ? obj.ageLimit : self->_ageLimit;
            NSDate *expirationDate = [obj.createdDate dateByAddingTimeInterval:ageLimit];
            if ([expirationDate compare:now] == NSOrderedAscending) { // Expiration date has passed
                [expiredObjectKeys addObject:key];
            }
        }];
    [self unlock];

    for (NSString *key in expiredObjectKeys) {
        //unlock, removeFileAndExecuteBlocksForKey handles locking itself
        [self removeFileAndExecuteBlocksForKey:key];
    }
}

- (void)removeAllObjects
{
    // We don't need to know the disk state since we're just going to remove everything.
    [self lockForWriting];
        PINCacheBlock willRemoveAllObjectsBlock = self->_willRemoveAllObjectsBlock;
        if (willRemoveAllObjectsBlock) {
            [self unlock];
                willRemoveAllObjectsBlock(self);
            [self lock];
        }
    
        [PINDiskCache moveItemAtURLToTrash:self->_cacheURL];
        [PINDiskCache emptyTrash];
        
        [self _locked_createCacheDirectory];
        
        [self->_metadata removeAllObjects];
        self.byteCount = 0; // atomic
    
        PINCacheBlock didRemoveAllObjectsBlock = self->_didRemoveAllObjectsBlock;
        if (didRemoveAllObjectsBlock) {
            [self unlock];
                didRemoveAllObjectsBlock(self);
            [self lock];
        }
    
    [self unlock];
}

- (void)enumerateObjectsWithBlock:(PIN_NOESCAPE PINDiskCacheFileURLEnumerationBlock)block
{
    if (!block)
        return;
    
    [self lockAndWaitForKnownState];
        NSDate *now = [NSDate date];
    
        for (NSString *key in _metadata) {
            NSURL *fileURL = [self encodedFileURLForKey:key];
            // If the cache should behave like a TTL cache, then only fetch the object if there's a valid ageLimit and the object is still alive
            NSDate *createdDate = _metadata[key].createdDate;
            NSTimeInterval ageLimit = _metadata[key].ageLimit > 0.0 ? _metadata[key].ageLimit : self->_ageLimit;
            if (!self->_ttlCache || ageLimit <= 0 || (createdDate && fabs([createdDate timeIntervalSinceDate:now]) < ageLimit)) {
                BOOL stop = NO;
                block(key, fileURL, &stop);
                if (stop)
                    break;
            }
        }
    [self unlock];
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
    [self.operationQueue scheduleOperation:^{
        [self lock];
            self->_willAddObjectBlock = [block copy];
        [self unlock];
    } withPriority:PINOperationQueuePriorityHigh];
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
    [self.operationQueue scheduleOperation:^{
        [self lock];
            self->_willRemoveObjectBlock = [block copy];
        [self unlock];
    } withPriority:PINOperationQueuePriorityHigh];
}

- (PINCacheBlock)willRemoveAllObjectsBlock
{
    PINCacheBlock block = nil;
    
    [self lock];
        block = _willRemoveAllObjectsBlock;
    [self unlock];
    
    return block;
}

- (void)setWillRemoveAllObjectsBlock:(PINCacheBlock)block
{
    [self.operationQueue scheduleOperation:^{
        [self lock];
            self->_willRemoveAllObjectsBlock = [block copy];
        [self unlock];
    } withPriority:PINOperationQueuePriorityHigh];
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
    [self.operationQueue scheduleOperation:^{
        [self lock];
            self->_didAddObjectBlock = [block copy];
        [self unlock];
    } withPriority:PINOperationQueuePriorityHigh];
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
    [self.operationQueue scheduleOperation:^{
        [self lock];
            self->_didRemoveObjectBlock = [block copy];
        [self unlock];
    } withPriority:PINOperationQueuePriorityHigh];
}

- (PINCacheBlock)didRemoveAllObjectsBlock
{
    PINCacheBlock block = nil;
    
    [self lock];
        block = _didRemoveAllObjectsBlock;
    [self unlock];
    
    return block;
}

- (void)setDidRemoveAllObjectsBlock:(PINCacheBlock)block
{
    [self.operationQueue scheduleOperation:^{
        [self lock];
            self->_didRemoveAllObjectsBlock = [block copy];
        [self unlock];
    } withPriority:PINOperationQueuePriorityHigh];
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
    [self.operationQueue scheduleOperation:^{
        [self lock];
            self->_byteLimit = byteLimit;
        [self unlock];
        
        if (byteLimit > 0)
            [self trimDiskToSizeByDate:byteLimit];
    } withPriority:PINOperationQueuePriorityHigh];
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
    [self.operationQueue scheduleOperation:^{
        [self lock];
            self->_ageLimit = ageLimit;
        [self unlock];
        
        [self.operationQueue scheduleOperation:^{
            [self trimToAgeLimitRecursively];
        } withPriority:PINOperationQueuePriorityLow];
    } withPriority:PINOperationQueuePriorityHigh];
}

- (BOOL)isTTLCache
{
    BOOL isTTLCache;
    
    [self lock];
        isTTLCache = _ttlCache;
    [self unlock];
  
    return isTTLCache;
}

#if TARGET_OS_IPHONE
- (NSDataWritingOptions)writingProtectionOption
{
    NSDataWritingOptions option;
  
    [self lock];
        option = _writingProtectionOption;
    [self unlock];
  
    return option;
}

- (void)setWritingProtectionOption:(NSDataWritingOptions)writingProtectionOption
{
  [self.operationQueue scheduleOperation:^{
      NSDataWritingOptions option = NSDataWritingFileProtectionMask & writingProtectionOption;
    
      [self lock];
          self->_writingProtectionOptionSet = YES;
          self->_writingProtectionOption = option;
      [self unlock];
  } withPriority:PINOperationQueuePriorityHigh];
}
#endif

- (void)lockForWriting
{
    [self lock];
    
    // Lock if the disk isn't writable.
    if (_diskWritable == NO) {
        pthread_cond_wait(&_diskWritableCondition, &_mutex);
    }
}

- (void)lockAndWaitForKnownState
{
    [self lock];
    
    // Lock if the disk state isn't known.
    if (_diskStateKnown == NO) {
        pthread_cond_wait(&_diskStateKnownCondition, &_mutex);
    }
}

- (void)lock
{
    __unused int result = pthread_mutex_lock(&_mutex);
    NSAssert(result == 0, @"Failed to lock PINDiskCache %@. Code: %d", self, result);
}

- (void)unlock
{
    __unused int result = pthread_mutex_unlock(&_mutex);
    NSAssert(result == 0, @"Failed to unlock PINDiskCache %@. Code: %d", self, result);
}

@end

@implementation PINDiskCache (Deprecated)

- (void)lockFileAccessWhileExecutingBlock:(nullable PINCacheBlock)block
{
    [self lockFileAccessWhileExecutingBlockAsync:block];
}

- (void)containsObjectForKey:(NSString *)key block:(PINDiskCacheContainsBlock)block
{
    [self containsObjectForKeyAsync:key completion:block];
}

- (void)objectForKey:(NSString *)key block:(nullable PINDiskCacheObjectBlock)block
{
    [self objectForKeyAsync:key completion:block];
}

- (void)fileURLForKey:(NSString *)key block:(nullable PINDiskCacheFileURLBlock)block
{
    [self fileURLForKeyAsync:key completion:block];
}

- (void)setObject:(id <NSCoding>)object forKey:(NSString *)key block:(nullable PINDiskCacheObjectBlock)block
{
    [self setObjectAsync:object forKey:key completion:block];
}

- (void)removeObjectForKey:(NSString *)key block:(nullable PINDiskCacheObjectBlock)block
{
    [self removeObjectForKeyAsync:key completion:block];
}

- (void)trimToDate:(NSDate *)date block:(nullable PINDiskCacheBlock)block
{
    [self trimToDateAsync:date completion:^(id<PINCaching> diskCache) {
        if (block) {
            block((PINDiskCache *)diskCache);
        }
    }];
}

- (void)trimToSize:(NSUInteger)byteCount block:(nullable PINDiskCacheBlock)block
{
    [self trimToSizeAsync:byteCount completion:^(id<PINCaching> diskCache) {
        if (block) {
            block((PINDiskCache *)diskCache);
        }
    }];
}

- (void)trimToSizeByDate:(NSUInteger)byteCount block:(nullable PINDiskCacheBlock)block
{
    [self trimToSizeAsync:byteCount completion:^(id<PINCaching> diskCache) {
        if (block) {
            block((PINDiskCache *)diskCache);
        }
    }];
}

- (void)removeAllObjects:(nullable PINDiskCacheBlock)block
{
    [self removeAllObjectsAsync:^(id<PINCaching> diskCache) {
        if (block) {
            block((PINDiskCache *)diskCache);
        }
    }];
}

- (void)enumerateObjectsWithBlock:(PINDiskCacheFileURLBlock)block completionBlock:(nullable PINDiskCacheBlock)completionBlock
{
    [self enumerateObjectsWithBlockAsync:^(NSString * _Nonnull key, NSURL * _Nullable fileURL, BOOL * _Nonnull stop) {
      block(key, fileURL);
    } completionBlock:^(id<PINCaching> diskCache) {
        if (completionBlock) {
            completionBlock((PINDiskCache *)diskCache);
        }
    }];
}

- (void)setTtlCache:(BOOL)ttlCache
{
    [self.operationQueue scheduleOperation:^{
        [self lock];
            self->_ttlCache = ttlCache;
        [self unlock];
    } withPriority:PINOperationQueuePriorityHigh];
}

@end

@implementation PINDiskCacheMetadata
@end

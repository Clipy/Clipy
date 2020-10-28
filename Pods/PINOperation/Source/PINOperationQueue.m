//
//  PINOperationQueue.m
//  Pods
//
//  Created by Garrett Moon on 8/23/16.
//
//

#import "PINOperationQueue.h"
#import <pthread.h>

@class PINOperation;

@interface NSNumber (PINOperationQueue) <PINOperationReference>

@end

@interface PINOperationQueue () {
  pthread_mutex_t _lock;
  //increments with every operation to allow cancelation
  NSUInteger _operationReferenceCount;
  NSUInteger _maxConcurrentOperations;
  
  dispatch_group_t _group;
  
  dispatch_queue_t _serialQueue;
  BOOL _serialQueueBusy;
  
  dispatch_semaphore_t _concurrentSemaphore;
  dispatch_queue_t _concurrentQueue;
  dispatch_queue_t _semaphoreQueue;
  
  NSMutableOrderedSet<PINOperation *> *_queuedOperations;
  NSMutableOrderedSet<PINOperation *> *_lowPriorityOperations;
  NSMutableOrderedSet<PINOperation *> *_defaultPriorityOperations;
  NSMutableOrderedSet<PINOperation *> *_highPriorityOperations;
  
  NSMapTable<id<PINOperationReference>, PINOperation *> *_referenceToOperations;
  NSMapTable<NSString *, PINOperation *> *_identifierToOperations;
}

@end

@interface PINOperation : NSObject

@property (nonatomic, strong) PINOperationBlock block;
@property (nonatomic, strong) id <PINOperationReference> reference;
@property (nonatomic, assign) PINOperationQueuePriority priority;
@property (nonatomic, strong) NSMutableArray<dispatch_block_t> *completions;
@property (nonatomic, strong) NSString *identifier;
@property (nonatomic, strong) id data;

+ (instancetype)operationWithBlock:(PINOperationBlock)block reference:(id <PINOperationReference>)reference priority:(PINOperationQueuePriority)priority identifier:(nullable NSString *)identifier data:(nullable id)data completion:(nullable dispatch_block_t)completion;

- (void)addCompletion:(nullable dispatch_block_t)completion;

@end

@implementation PINOperation

+ (instancetype)operationWithBlock:(PINOperationBlock)block reference:(id<PINOperationReference>)reference priority:(PINOperationQueuePriority)priority identifier:(NSString *)identifier data:(id)data completion:(dispatch_block_t)completion
{
  PINOperation *operation = [[self alloc] init];
  operation.block = block;
  operation.reference = reference;
  operation.priority = priority;
  operation.identifier = identifier;
  operation.data = data;
  [operation addCompletion:completion];
  
  return operation;
}

- (void)addCompletion:(dispatch_block_t)completion
{
  if (completion == nil) {
    return;
  }
  if (_completions == nil) {
    _completions = [NSMutableArray array];
  }
  [_completions addObject:completion];
}

@end

@implementation PINOperationQueue

- (instancetype)initWithMaxConcurrentOperations:(NSUInteger)maxConcurrentOperations
{
  return [self initWithMaxConcurrentOperations:maxConcurrentOperations concurrentQueue:dispatch_queue_create("PINOperationQueue Concurrent Queue", DISPATCH_QUEUE_CONCURRENT)];
}

- (instancetype)initWithMaxConcurrentOperations:(NSUInteger)maxConcurrentOperations concurrentQueue:(dispatch_queue_t)concurrentQueue
{
  if (self = [super init]) {
    NSAssert(maxConcurrentOperations > 0, @"Max concurrent operations must be greater than 0.");
    _maxConcurrentOperations = maxConcurrentOperations;
    _operationReferenceCount = 0;
    
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    //mutex must be recursive to allow scheduling of operations from within operations
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE);
    pthread_mutex_init(&_lock, &attr);
    
    _group = dispatch_group_create();
    
    _serialQueue = dispatch_queue_create("PINOperationQueue Serial Queue", DISPATCH_QUEUE_SERIAL);
    
    _concurrentQueue = concurrentQueue;
    
    //Create a queue with max - 1 because this plus the serial queue add up to max.
    _concurrentSemaphore = dispatch_semaphore_create(_maxConcurrentOperations - 1);
    _semaphoreQueue = dispatch_queue_create("PINOperationQueue Serial Semaphore Queue", DISPATCH_QUEUE_SERIAL);
    
    _queuedOperations = [[NSMutableOrderedSet alloc] init];
    _lowPriorityOperations = [[NSMutableOrderedSet alloc] init];
    _defaultPriorityOperations = [[NSMutableOrderedSet alloc] init];
    _highPriorityOperations = [[NSMutableOrderedSet alloc] init];
    
    _referenceToOperations = [NSMapTable weakToWeakObjectsMapTable];
    _identifierToOperations = [NSMapTable weakToWeakObjectsMapTable];
  }
  return self;
}

- (void)dealloc
{
  pthread_mutex_destroy(&_lock);
}

+ (instancetype)sharedOperationQueue
{
    static PINOperationQueue *sharedOperationQueue = nil;
    static dispatch_once_t onceToken;
    dispatch_once(&onceToken, ^{
        sharedOperationQueue = [[PINOperationQueue alloc] initWithMaxConcurrentOperations:MAX([[NSProcessInfo processInfo] activeProcessorCount], 2)];
    });
    return sharedOperationQueue;
}

- (id <PINOperationReference>)nextOperationReference
{
  [self lock];
    id <PINOperationReference> reference = [NSNumber numberWithUnsignedInteger:++_operationReferenceCount];
  [self unlock];
  return reference;
}

// Deprecated
- (id <PINOperationReference>)addOperation:(dispatch_block_t)block
{
  return [self scheduleOperation:block];
}

- (id <PINOperationReference>)scheduleOperation:(dispatch_block_t)block
{
  return [self scheduleOperation:block withPriority:PINOperationQueuePriorityDefault];
}

// Deprecated
- (id <PINOperationReference>)addOperation:(dispatch_block_t)block withPriority:(PINOperationQueuePriority)priority
{
  return [self scheduleOperation:block withPriority:priority];
}

- (id <PINOperationReference>)scheduleOperation:(dispatch_block_t)block withPriority:(PINOperationQueuePriority)priority
{
  PINOperation *operation = [PINOperation operationWithBlock:^(id data) { block(); }
                                                   reference:[self nextOperationReference]
                                                    priority:priority
                                                  identifier:nil
                                                        data:nil
                                                  completion:nil];
  [self lock];
    [self locked_addOperation:operation];
  [self unlock];
  
  [self scheduleNextOperations:NO];
  
  return operation.reference;
}

// Deprecated
- (id<PINOperationReference>)addOperation:(PINOperationBlock)block
                             withPriority:(PINOperationQueuePriority)priority
                               identifier:(NSString *)identifier
                           coalescingData:(id)coalescingData
                      dataCoalescingBlock:(PINOperationDataCoalescingBlock)dataCoalescingBlock
                               completion:(dispatch_block_t)completion
{
  return [self scheduleOperation:block
                    withPriority:priority
                      identifier:identifier
                  coalescingData:coalescingData
             dataCoalescingBlock:dataCoalescingBlock
                      completion:completion];
}

- (id<PINOperationReference>)scheduleOperation:(PINOperationBlock)block
                                  withPriority:(PINOperationQueuePriority)priority
                                    identifier:(NSString *)identifier
                                coalescingData:(id)coalescingData
                           dataCoalescingBlock:(PINOperationDataCoalescingBlock)dataCoalescingBlock
                                    completion:(dispatch_block_t)completion
{
  id<PINOperationReference> reference = nil;
  BOOL isNewOperation = NO;
  
  [self lock];
    PINOperation *operation = nil;
    if (identifier != nil && (operation = [_identifierToOperations objectForKey:identifier]) != nil) {
      // There is an exisiting operation with the provided identifier, let's coalesce these operations
      if (dataCoalescingBlock != nil) {
        operation.data = dataCoalescingBlock(operation.data, coalescingData);
      }
      
      [operation addCompletion:completion];
    } else {
      isNewOperation = YES;
      operation = [PINOperation operationWithBlock:block
                                         reference:[self nextOperationReference]
                                          priority:priority
                                        identifier:identifier
                                              data:coalescingData
                                        completion:completion];
      [self locked_addOperation:operation];
    }
    reference = operation.reference;
  [self unlock];
  
  if (isNewOperation) {
    [self scheduleNextOperations:NO];
  }
  
  return reference;
}

- (void)locked_addOperation:(PINOperation *)operation
{
  NSMutableOrderedSet *queue = [self operationQueueWithPriority:operation.priority];
  
  dispatch_group_enter(_group);
  [queue addObject:operation];
  [_queuedOperations addObject:operation];
  [_referenceToOperations setObject:operation forKey:operation.reference];
  if (operation.identifier != nil) {
    [_identifierToOperations setObject:operation forKey:operation.identifier];
  }
}

- (void)cancelAllOperations
{
  [self lock];
    for (PINOperation *operation in [[_referenceToOperations copy] objectEnumerator]) {
      [self locked_cancelOperation:operation.reference];
    }
  [self unlock];
}


- (BOOL)cancelOperation:(id <PINOperationReference>)operationReference
{
  [self lock];
    BOOL success = [self locked_cancelOperation:operationReference];
  [self unlock];
  return success;
}

- (NSUInteger)maxConcurrentOperations
{
  [self lock];
    NSUInteger maxConcurrentOperations = _maxConcurrentOperations;
  [self unlock];
  return maxConcurrentOperations;
}

- (void)setMaxConcurrentOperations:(NSUInteger)maxConcurrentOperations
{
  NSAssert(maxConcurrentOperations > 0, @"Max concurrent operations must be greater than 0.");
  [self lock];
    __block NSInteger difference = maxConcurrentOperations - _maxConcurrentOperations;
    _maxConcurrentOperations = maxConcurrentOperations;
  [self unlock];
  
  if (difference == 0) {
    return;
  }
  
  dispatch_async(_semaphoreQueue, ^{
    while (difference != 0) {
      if (difference > 0) {
        dispatch_semaphore_signal(self->_concurrentSemaphore);
        difference--;
      } else {
        dispatch_semaphore_wait(self->_concurrentSemaphore, DISPATCH_TIME_FOREVER);
        difference++;
      }
    }
  });
}

#pragma mark - private methods

- (BOOL)locked_cancelOperation:(id <PINOperationReference>)operationReference
{
  PINOperation *operation = [_referenceToOperations objectForKey:operationReference];
  BOOL success = [self locked_removeOperation:operation];
  if (success) {
    dispatch_group_leave(_group);
  }
  return success;
}

- (void)setOperationPriority:(PINOperationQueuePriority)priority withReference:(id <PINOperationReference>)operationReference
{
  [self lock];
    PINOperation *operation = [_referenceToOperations objectForKey:operationReference];
    if (operation && operation.priority != priority) {
      NSMutableOrderedSet *oldQueue = [self operationQueueWithPriority:operation.priority];
      [oldQueue removeObject:operation];
      
      operation.priority = priority;
      
      NSMutableOrderedSet *queue = [self operationQueueWithPriority:priority];
      [queue addObject:operation];
    }
  [self unlock];
}

/**
 Schedule next operations schedules the next operation by queue order onto the serial queue if
 it's available and one operation by priority order onto the concurrent queue.
 */
- (void)scheduleNextOperations:(BOOL)onlyCheckSerial
{
  [self lock];
  
    //get next available operation in order, ignoring priority and run it on the serial queue
    if (_serialQueueBusy == NO) {
      PINOperation *operation = [self locked_nextOperationByQueue];
      if (operation) {
        _serialQueueBusy = YES;
        dispatch_async(_serialQueue, ^{
          operation.block(operation.data);
          for (dispatch_block_t completion in operation.completions) {
            completion();
          }
          dispatch_group_leave(self->_group);
          
          [self lock];
            self->_serialQueueBusy = NO;
          [self unlock];
          
          //see if there are any other operations
          [self scheduleNextOperations:YES];
        });
      }
    }
  
  NSInteger maxConcurrentOperations = _maxConcurrentOperations;
  
  [self unlock];
  
  if (onlyCheckSerial) {
    return;
  }

  //if only one concurrent operation is set, let's just use the serial queue for executing it
  if (maxConcurrentOperations < 2) {
    return;
  }
  
  dispatch_async(_semaphoreQueue, ^{
    dispatch_semaphore_wait(self->_concurrentSemaphore, DISPATCH_TIME_FOREVER);
    [self lock];
      PINOperation *operation = [self locked_nextOperationByPriority];
    [self unlock];
  
    if (operation) {
      dispatch_async(self->_concurrentQueue, ^{
        operation.block(operation.data);
        for (dispatch_block_t completion in operation.completions) {
          completion();
        }
        dispatch_group_leave(self->_group);
        dispatch_semaphore_signal(self->_concurrentSemaphore);
      });
    } else {
      dispatch_semaphore_signal(self->_concurrentSemaphore);
    }
  });
}

- (NSMutableOrderedSet *)operationQueueWithPriority:(PINOperationQueuePriority)priority
{
  switch (priority) {
    case PINOperationQueuePriorityLow:
      return _lowPriorityOperations;
      
    case PINOperationQueuePriorityDefault:
      return _defaultPriorityOperations;
      
    case PINOperationQueuePriorityHigh:
      return _highPriorityOperations;
          
    default:
      NSAssert(NO, @"Invalid priority set");
      return _defaultPriorityOperations;
  }
}

//Call with lock held
- (PINOperation *)locked_nextOperationByPriority
{
  PINOperation *operation = [_highPriorityOperations firstObject];
  if (operation == nil) {
    operation = [_defaultPriorityOperations firstObject];
  }
  if (operation == nil) {
    operation = [_lowPriorityOperations firstObject];
  }
  if (operation) {
    [self locked_removeOperation:operation];
  }
  return operation;
}

//Call with lock held
- (PINOperation *)locked_nextOperationByQueue
{
  PINOperation *operation = [_queuedOperations firstObject];
  [self locked_removeOperation:operation];
  return operation;
}

- (void)waitUntilAllOperationsAreFinished
{
  [self scheduleNextOperations:NO];
  dispatch_group_wait(_group, DISPATCH_TIME_FOREVER);
}

//Call with lock held
- (BOOL)locked_removeOperation:(PINOperation *)operation
{
  if (operation) {
    NSMutableOrderedSet *priorityQueue = [self operationQueueWithPriority:operation.priority];
    if ([priorityQueue containsObject:operation]) {
      [priorityQueue removeObject:operation];
      [_queuedOperations removeObject:operation];
      if (operation.identifier) {
        [_identifierToOperations removeObjectForKey:operation.identifier];
      }
      return YES;
    }
  }
  return NO;
}

- (void)lock
{
  pthread_mutex_lock(&_lock);
}

- (void)unlock
{
  pthread_mutex_unlock(&_lock);
}

@end

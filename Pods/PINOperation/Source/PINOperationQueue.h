//
//  PINOperationQueue.h
//  Pods
//
//  Created by Garrett Moon on 8/23/16.
//
//

#import <Foundation/Foundation.h>
#import "PINOperationTypes.h"
#import "PINOperationMacros.h"

NS_ASSUME_NONNULL_BEGIN

typedef void(^PINOperationBlock)(id _Nullable data);
typedef _Nullable id(^PINOperationDataCoalescingBlock)(id _Nullable existingData, id _Nullable newData);

@protocol PINOperationReference;

PINOP_SUBCLASSING_RESTRICTED
@interface PINOperationQueue : NSObject

- (instancetype)init NS_UNAVAILABLE;

/**
 * Initializes and returns a newly allocated operation queue with the specified number of maximum concurrent operations.
 *
 * @param maxConcurrentOperations The maximum number of queued operations that can execute at the same time.
 *
 */
- (instancetype)initWithMaxConcurrentOperations:(NSUInteger)maxConcurrentOperations;

/**
 * Initializes and returns a newly allocated operation queue with the specified number of maximum concurrent operations and the concurrent queue they will be scheduled on.
 *
 * @param maxConcurrentOperations The maximum number of queued operations that can execute at the same time.
 * @param concurrentQueue The operation queue to schedule concurrent operations
 *
 */
- (instancetype)initWithMaxConcurrentOperations:(NSUInteger)maxConcurrentOperations concurrentQueue:(dispatch_queue_t)concurrentQueue NS_DESIGNATED_INITIALIZER;

/**
 * Returns the shared instance of the PINOperationQueue class.
 */
+ (instancetype)sharedOperationQueue;

/**
 * Adds the specified operation object to the receiver.
 *
 * @param operation The operation object to be added to the queue.
 *
 */
- (id <PINOperationReference>)scheduleOperation:(dispatch_block_t)operation;

/**
 * Adds the specified operation object to the receiver.
 *
 * @param operation The operation object to be added to the queue.
 * @param priority The execution priority of the operation in an operation queue.
 */
- (id <PINOperationReference>)scheduleOperation:(dispatch_block_t)operation withPriority:(PINOperationQueuePriority)priority;

/**
 * Adds the specified operation object to the receiver.
 *
 * @param operation The operation object to be added to the queue.
 * @param priority The execution priority of the operation in an operation queue.
 * @param identifier A string that identifies the operations that can be coalesced.
 * @param coalescingData The optional data consumed by this operation that needs to be updated/coalesced with data of a new operation when coalescing the two operations happens.
 * @param dataCoalescingBlock The optional block called to update/coalesce the data of this operation with data of a new operation when coalescing the two operations happens.
 * @param completion The block to execute after the operation finished.
 */
- (id <PINOperationReference>)scheduleOperation:(PINOperationBlock)operation
                                   withPriority:(PINOperationQueuePriority)priority
                                     identifier:(nullable NSString *)identifier
                                 coalescingData:(nullable id)coalescingData
                            dataCoalescingBlock:(nullable PINOperationDataCoalescingBlock)dataCoalescingBlock
                                     completion:(nullable dispatch_block_t)completion;

/**
 * The maximum number of queued operations that can execute at the same time.
 *
 * @discussion The value in this property affects only the operations that the current queue has executing at the same time. Other operation queues can also execute their maximum number of operations in parallel.
 * Reducing the number of concurrent operations does not affect any operations that are currently executing.
 *
 * Setting this value to 1 the operations will not be processed by priority as the operations will processed in a FIFO order to prevent deadlocks if operations depend on certain other operations to run in order.
 *
 */
@property (assign) NSUInteger maxConcurrentOperations;

/**
 * Marks the operation as cancelled
 */
- (BOOL)cancelOperation:(id <PINOperationReference>)operationReference;

/**
 * Cancels all queued operations
 */
- (void)cancelAllOperations;

/**
 * Blocks the current thread until all of the receiver’s queued and executing operations finish executing.
 *
 * @discussion When called, this method blocks the current thread and waits for the receiver’s current and queued
 * operations to finish executing. While the current thread is blocked, the receiver continues to launch already
 * queued operations and monitor those that are executing.
 *
 * @warning This should never be called from within an operation submitted to the PINOperationQueue as this will result
 * in a deadlock.
 */
- (void)waitUntilAllOperationsAreFinished;

/**
 * Sets the priority for a operation via it's reference
 *
 * @param priority The new priority for the operation
 * @param reference The reference for the operation
 *
 */
- (void)setOperationPriority:(PINOperationQueuePriority)priority withReference:(id <PINOperationReference>)reference;

#pragma mark - Deprecated

- (id <PINOperationReference>)addOperation:(dispatch_block_t)operation __deprecated_msg("Use scheduleOperation: instead.");

- (id <PINOperationReference>)addOperation:(dispatch_block_t)operation withPriority:(PINOperationQueuePriority)priority __deprecated_msg("Use scheduleOperation:withPriority: instead.");

- (id <PINOperationReference>)addOperation:(PINOperationBlock)operation
                              withPriority:(PINOperationQueuePriority)priority
                                identifier:(nullable NSString *)identifier
                            coalescingData:(nullable id)coalescingData
                       dataCoalescingBlock:(nullable PINOperationDataCoalescingBlock)dataCoalescingBlock
                                completion:(nullable dispatch_block_t)completion __deprecated_msg("Use scheduleOperation:withPriority:identifier:coalescingData:dataCoalescingBlock:completion: instead.");

@end

@protocol PINOperationReference <NSObject>

@end

NS_ASSUME_NONNULL_END

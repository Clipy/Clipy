//
//  PINOperationGroup.m
//  PINQueue
//
//  Created by Garrett Moon on 10/8/16.
//  Copyright © 2016 Pinterest. All rights reserved.
//

#import "PINOperationGroup.h"
#import "PINOperation.h"
#import <pthread.h>

@interface NSNumber (PINGroupOperationQueue) <PINGroupOperationReference>
@end

@interface PINOperationGroup ()
{
  pthread_mutex_t _lock;
  
  PINOperationQueue *_operationQueue;
  NSMutableArray <dispatch_block_t> *_operations;
  NSMutableArray <NSNumber *> *_operationPriorities;
  NSMutableArray <id <PINGroupOperationReference>> *_operationReferences;
  NSMapTable <id <PINGroupOperationReference>, id <PINOperationReference>> *_groupToOperationReferences;
  NSUInteger _operationReferenceCount;
  
  dispatch_group_t _group;
  
  dispatch_block_t _completion;
  
  BOOL _started;
  BOOL _canceled;
}

- (instancetype)initWithOperationQueue:(PINOperationQueue *)operationQueue NS_DESIGNATED_INITIALIZER;

@end

@implementation PINOperationGroup

- (instancetype)initWithOperationQueue:(PINOperationQueue *)operationQueue
{
  if (self = [super init]) {
    pthread_mutex_init(&_lock, NULL);
    
    _operationQueue = operationQueue;
    
    _operations = [[NSMutableArray alloc] init];
    _operationReferences = [[NSMutableArray alloc] init];
    _operationPriorities = [[NSMutableArray alloc] init];
    
    _groupToOperationReferences = [NSMapTable weakToStrongObjectsMapTable];
    _group = dispatch_group_create();
  }
  return self;
}

- (void)dealloc
{
  pthread_mutex_destroy(&_lock);
}

+ (instancetype)asyncOperationGroupWithQueue:(PINOperationQueue *)operationQueue
{
  return [[self alloc] initWithOperationQueue:operationQueue];
}

- (id <PINGroupOperationReference>)locked_nextOperationReference
{
  id <PINGroupOperationReference> reference = [NSNumber numberWithUnsignedInteger:++_operationReferenceCount];
  return reference;
}

- (void)start
{
  [self lock];
    NSAssert(_canceled == NO, @"Operation group canceled.");
    if (_started == NO && _canceled == NO) {
      for (NSUInteger idx = 0; idx < _operations.count; idx++) {
        dispatch_group_enter(_group);
        dispatch_block_t originalOperation = _operations[idx];
        dispatch_block_t groupBlock = ^{
          originalOperation();
          dispatch_group_leave(self->_group);
        };
        
        id <PINOperationReference> operationReference = [_operationQueue scheduleOperation:groupBlock withPriority:[_operationPriorities[idx] unsignedIntegerValue]];
        [_groupToOperationReferences setObject:operationReference forKey:_operationReferences[idx]];
      }
      
      if (_completion) {
        dispatch_queue_t completionQueue = dispatch_get_global_queue(DISPATCH_QUEUE_PRIORITY_DEFAULT, 0);
        dispatch_group_notify(_group, completionQueue, ^{
          [self runCompletionIfNeeded];
        });
      }
      
      _operations = nil;
      _operationPriorities = nil;
      _operationReferences = nil;
    }
  [self unlock];
}

- (void)cancel
{
  [self lock];
    _canceled = YES;
  
    for (id <PINOperationReference>operationReference in [_groupToOperationReferences objectEnumerator]) {
      if ([_operationQueue cancelOperation:operationReference]) {
        dispatch_group_leave(_group);
      }
    }
  
    //TODO just nil out instead? Does it make sense to support adding operations after cancelation?
    [_groupToOperationReferences removeAllObjects];
    [_operations removeAllObjects];
    [_operationPriorities removeAllObjects];
    [_operationReferences removeAllObjects];
    
    _completion = nil;
  [self unlock];
}

- (id <PINGroupOperationReference>)addOperation:(dispatch_block_t)operation
{
  return [self addOperation:operation withPriority:PINOperationQueuePriorityDefault];
}

- (id <PINGroupOperationReference>)addOperation:(dispatch_block_t)operation withPriority:(PINOperationQueuePriority)priority
{
  [self lock];
    id <PINGroupOperationReference> reference = nil;
    NSAssert(_started == NO && _canceled == NO, @"Operation group already started or canceled.");
    if (_started == NO && _canceled == NO) {
      reference = [self locked_nextOperationReference];
      [_operations addObject:operation];
      [_operationPriorities addObject:@(priority)];
      [_operationReferences addObject:reference];
    }
  [self unlock];
  
  return reference;
}

- (void)setCompletion:(dispatch_block_t)completion
{
  [self lock];
    NSAssert(_started == NO && _canceled == NO, @"Operation group already started or canceled.");
    if (_started == NO && _canceled == NO) {
      _completion = completion;
    }
  [self unlock];
}

- (void)waitUntilComplete
{
  [self start];
  dispatch_group_wait(_group, DISPATCH_TIME_FOREVER);
  [self runCompletionIfNeeded];
}

- (void)runCompletionIfNeeded
{
  dispatch_block_t completion;
  [self lock];
    completion = _completion;
    _completion = nil;
  [self unlock];

  if (completion) {
    completion();
  }
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

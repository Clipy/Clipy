//
//  PINCacheMacros.h
//  PINCache
//
//  Created by Adlai Holler on 1/31/17.
//  Copyright © 2017 Pinterest. All rights reserved.
//

#ifndef PIN_SUBCLASSING_RESTRICTED
#if defined(__has_attribute) && __has_attribute(objc_subclassing_restricted)
#define PIN_SUBCLASSING_RESTRICTED __attribute__((objc_subclassing_restricted))
#else
#define PIN_SUBCLASSING_RESTRICTED
#endif // #if defined(__has_attribute) && __has_attribute(objc_subclassing_restricted)
#endif // #ifndef PIN_SUBCLASSING_RESTRICTED

#ifndef PIN_NOESCAPE
#if defined(__has_attribute) && __has_attribute(noescape)
#define PIN_NOESCAPE __attribute__((noescape))
#else
#define PIN_NOESCAPE
#endif // #if defined(__has_attribute) && __has_attribute(noescape)
#endif // #ifndef PIN_NOESCAPE

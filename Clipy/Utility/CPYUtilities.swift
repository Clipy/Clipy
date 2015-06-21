//
//  CPYUtilities.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/21.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa

class CPYUtilities: NSObject {

    static func registerUserDefaultKeys() {
        /*
        var defaultValues = [String: AnyObject]()
        
        defaultValues.updateValue(self.defaultHotKeyCombos(), forKey: kCPYPrefHotKeysKey)
        /* General */
        defaultValues.updateValue(NSNumber(bool: false), forKey: kCPYPrefLoginItemKey)
        defaultValues.updateValue(NSNumber(bool: false), forKey: kCPYPrefSuppressAlertForLoginItemKey)
        defaultValues.updateValue(NSNumber(integer: 30), forKey: kCPYPrefMaxHistorySizeKey)
        defaultValues.updateValue(NSNumber(integer: 1800), forKey: kCPYPrefAutosaveDelayKey)
        defaultValues.updateValue(NSNumber(bool: true), forKey: kCPYPrefSaveHistoryOnQuitKey)
        defaultValues.updateValue(NSNumber(integer: 1), forKey: kCPYPrefShowStatusItemKey)
        defaultValues.updateValue(NSNumber(float: 0.75), forKey: kCPYPrefTimeIntervalKey)
        defaultValues.updateValue(self.storeTypesDictinary(), forKey: kCPYPrefStoreTypesKey)
        defaultValues.updateValue(self.defaultExcludeList(), forKey: kCPYPrefExcludeAppsKey)
        defaultValues.updateValue(NSNumber(bool: true), forKey: kCPYPrefInputPasteCommandKey)
        defaultValues.updateValue(NSNumber(integer: 1), forKey: kCPYPrefTagOfSeparatorForExportHistoryToFileKey)
        defaultValues.updateValue(NSNumber(bool: true), forKey: kCPYPrefReorderClipsAfterPasting)
        
        /* Menu */
        defaultValues.updateValue(NSNumber(integer: 16), forKey: kCPYPrefMenuIconSizeKey)
        defaultValues.updateValue(NSNumber(integer: 20), forKey: kCPYPrefMaxMenuItemTitleLengthKey)
        defaultValues.updateValue(NSNumber(integer: 0), forKey: kCPYPrefNumberOfItemsPlaceInlineKey)
        defaultValues.updateValue(NSNumber(integer: 10), forKey: kCPYPrefNumberOfItemsPlaceInsideFolderKey)
        defaultValues.updateValue(NSNumber(bool: false), forKey: kCPYPrefMenuItemsTitleStartWithZeroKey)
        defaultValues.updateValue(NSNumber(bool: true), forKey: kCPYPrefShowAlertBeforeClearHistoryKey)
        defaultValues.updateValue(NSNumber(bool: true), forKey: kCPYPrefAddClearHistoryMenuItemKey)
        defaultValues.updateValue(NSNumber(bool: true), forKey: kCPYPrefShowIconInTheMenuKey)
        defaultValues.updateValue(NSNumber(bool: true), forKey: kCPYPrefMenuItemsAreMarkedWithNumbersKey)
        defaultValues.updateValue(NSNumber(bool: false), forKey: kCPYPrefAddNumericKeyEquivalentsKey)
        defaultValues.updateValue(NSNumber(bool: true), forKey: kCPYPrefShowToolTipOnMenuItemKey)
        defaultValues.updateValue(NSNumber(bool: true), forKey: kCPYPrefShowImageInTheMenuKey)
        defaultValues.updateValue(NSNumber(integer: 200), forKey: kCPYPrefMaxLengthOfToolTipKey)
        
        /* Snippet */
        
        /* Updates */
        defaultValues.updateValue(NSNumber(bool: false), forKey: kCPYEnableAutomaticCheckPreReleaseKey)
        
        /*
        /* General */
        [defaultValues setObject:[NSNumber numberWithBool:YES] forKey:CMPrefExportHistoryAsSingleFileKey];
        
        /* Menu */
        
        [defaultValues setObject:[NSNumber numberWithBool:YES] forKey:CMPrefShowLabelsInMenuKey];
        [defaultValues setObject:[NSNumber numberWithBool:YES] forKey:CMPrefAddClearHistoryMenuItemKey];
        
        
        [defaultValues setObject:[NSNumber numberWithBool:NO] forKey:CMPrefChangeFontSizeKey];
        [defaultValues setObject:[NSNumber numberWithInteger:0] forKey:CMPrefHowToChangeFontSizeKey];
        [defaultValues setObject:[NSNumber numberWithUnsignedInteger:14] forKey:CMPrefSelectedFontSizeKey];
        
        [defaultValues setObject:[NSNumber numberWithUnsignedInteger:100] forKey:CMPrefThumbnailWidthKey];
        [defaultValues setObject:[NSNumber numberWithUnsignedInteger:32] forKey:CMPrefThumbnailHeightKey];
        
        /* Snippet */
        [defaultValues setObject:[NSNumber numberWithInteger:CMPositionOfSnippetsBelowClips] forKey:CMPrefPositionOfSnippetsKey];
        /* Updates */
        [defaultValues setObject:[NSNumber numberWithBool:YES] forKey:CMEnableAutomaticCheckKey];
        [defaultValues setObject:[NSNumber numberWithInteger:86400] forKey:CMUpdateCheckIntervalKey]; // daily
        
        [[NSUserDefaults standardUserDefaults] registerDefaults:defaultValues];
        
        */
        
        //println(defaultValues)
        
        NSUserDefaults.standardUserDefaults().registerDefaults(defaultValues)
        NSUserDefaults.standardUserDefaults().synchronize()

        */
    }
    
}

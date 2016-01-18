//
//  CPYLoginItemManager.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/01/18.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation

final class CPYLoginItemManager {
    static func launchAtLoginEnabled(enabled: Bool) {
        guard let localizedName = NSRunningApplication.currentApplication().localizedName else {
            return
        }
        
        let fileName = enabled ? "addLoginItem" : "deleteLoginItem"
        let filePath = NSBundle.mainBundle().pathForResource(fileName, ofType: "scpt")
        
        do {
            let template = try NSString(contentsOfFile: filePath!, encoding: NSUTF8StringEncoding)
    
            var source: String
            if enabled {
                let bundlePath = NSBundle.mainBundle().bundlePath
                source = NSString(format: template, bundlePath, localizedName) as String
            } else {
                source = NSString(format: template, localizedName, localizedName) as String
            }
        
            // Run script
            let script = NSAppleScript(source: source)
            
            var error: NSDictionary?
            script?.executeAndReturnError(&error)
            
        } catch {}
    }
}
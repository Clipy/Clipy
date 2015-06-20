//
//  AppDelegate.swift
//  Clipy
//
//  Created by 古林俊佑 on 2015/06/21.
//  Copyright (c) 2015年 Shunsuke Furubayashi. All rights reserved.
//

import Cocoa

@NSApplicationMain
class AppDelegate: NSObject {


    // MARK: - Init
    override func awakeFromNib() {
        super.awakeFromNib()
        self.initController()
    }
    
    deinit {
        NSNotificationCenter.defaultCenter().removeObserver(self)
    }
    
    private func initController() {
        self.registerUserDefaultKey()
        
        
    }

    // MARK: - Override Methods
    override func validateMenuItem(menuItem: NSMenuItem) -> Bool {
        return true
    }
    
    // MARK: - KVO
    override func observeValueForKeyPath(keyPath: String, ofObject object: AnyObject, change: [NSObject : AnyObject], context: UnsafeMutablePointer<Void>) {
        
    }
    
    // MARK: - Menu Actions
    internal func showPreferenceWindow() {
        
    }
    
    internal func showSnippetEditorWindow() {
        
    }
    
    internal func clearAllHistory() {
        
    }
    
    internal func selectClipMenuItem() {
        
    }
    
    internal func selectSnippetMenuItem() {
        
    }
    
    // MARK: - Register User Default Keys
    private func registerUserDefaultKey() {
        
    }
   

}

// MARK: - NSApplication Delegate
extension AppDelegate: NSApplicationDelegate {

    func applicationDidFinishLaunching(aNotification: NSNotification) {
        // Insert code here to initialize your application
    }
    
    func applicationWillTerminate(aNotification: NSNotification) {
        // Insert code here to tear down your application
    }
 
}

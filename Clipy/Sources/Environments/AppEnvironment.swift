//
//  AppEnvironment.swift
//
//  Clipy
//  GitHub: https://github.com/clipy
//  HP: https://clipy-app.com
//
//  Created by Econa77 on 2017/08/10.
//
//  Copyright © 2015-2018 Clipy Project.
//

import Foundation

struct AppEnvironment {

    // MARK: - Properties
    private static var stack = [Environment()]

    static var current: Environment {
        return stack.last ?? Environment()
    }

    // MARK: - Stacks
    static func push(environment: Environment) {
        stack.append(environment)
    }

    @discardableResult
    static func popLast() -> Environment? {
        return stack.popLast()
    }

    static func replaceCurrent(environment: Environment) {
        push(environment: environment)
        stack.remove(at: stack.count - 2)
    }

    static func push(clipService: ClipService = current.clipService,
                     hotKeyService: HotKeyService = current.hotKeyService,
                     dataCleanService: DataCleanService = current.dataCleanService,
                     pasteService: PasteService = current.pasteService,
                     excludeAppService: ExcludeAppService = current.excludeAppService,
                     accessibilityService: AccessibilityService = current.accessibilityService,
                     menuManager: MenuManager = current.menuManager,
                     defaults: UserDefaults = current.defaults) {
        push(environment: Environment(clipService: clipService,
                                      hotKeyService: hotKeyService,
                                      dataCleanService: dataCleanService,
                                      pasteService: pasteService,
                                      excludeAppService: excludeAppService,
                                      accessibilityService: accessibilityService,
                                      menuManager: menuManager,
                                      defaults: defaults))
    }

    static func replaceCurrent(clipService: ClipService = current.clipService,
                               hotKeyService: HotKeyService = current.hotKeyService,
                               dataCleanService: DataCleanService = current.dataCleanService,
                               pasteService: PasteService = current.pasteService,
                               excludeAppService: ExcludeAppService = current.excludeAppService,
                               accessibilityService: AccessibilityService = current.accessibilityService,
                               menuManager: MenuManager = current.menuManager,
                               defaults: UserDefaults = current.defaults) {
        replaceCurrent(environment: Environment(clipService: clipService,
                                                hotKeyService: hotKeyService,
                                                dataCleanService: dataCleanService,
                                                pasteService: pasteService,
                                                excludeAppService: excludeAppService,
                                                accessibilityService: accessibilityService,
                                                menuManager: menuManager,
                                                defaults: defaults))
    }

    static func fromStorage(defaults: UserDefaults = .standard) -> Environment {
        var excludeApplications = [CPYAppInfo]()
        if let data = defaults.object(forKey: Constants.UserDefaults.excludeApplications) as? Data, let applications = NSKeyedUnarchiver.unarchiveObject(with: data) as? [CPYAppInfo] {
            excludeApplications = applications
        }
        let excludeAppService = ExcludeAppService(applications: excludeApplications)
        return Environment(clipService: current.clipService,
                           hotKeyService: current.hotKeyService,
                           dataCleanService: current.dataCleanService,
                           pasteService: current.pasteService,
                           excludeAppService: excludeAppService,
                           accessibilityService: current.accessibilityService,
                           menuManager: current.menuManager,
                           defaults: current.defaults)
    }

 }

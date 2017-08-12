//
//  AppEnvironment.swift
//  Clipy
//
//  Created by 古林俊佑 on 2017/08/10.
//  Copyright © 2017年 Shunsuke Furubayashi. All rights reserved.
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
                     menuManager: MenuManager = current.menuManager,
                     defaults: KeyValueStorable = current.defaults) {
        push(environment: Environment(clipService: clipService,
                                      hotKeyService: hotKeyService,
                                      dataCleanService: dataCleanService,
                                      pasteService: pasteService,
                                      excludeAppService: excludeAppService,
                                      menuManager: menuManager,
                                      defaults: defaults))
    }

    static func replaceCurrent(clipService: ClipService = current.clipService,
                               hotKeyService: HotKeyService = current.hotKeyService,
                               dataCleanService: DataCleanService = current.dataCleanService,
                               pasteService: PasteService = current.pasteService,
                               excludeAppService: ExcludeAppService = current.excludeAppService,
                               menuManager: MenuManager = current.menuManager,
                               defaults: KeyValueStorable = current.defaults) {
        replaceCurrent(environment: Environment(clipService: clipService,
                                                hotKeyService: hotKeyService,
                                                dataCleanService: dataCleanService,
                                                pasteService: pasteService,
                                                excludeAppService: excludeAppService,
                                                menuManager: menuManager,
                                                defaults: defaults))
    }

    static func fromStorage(defaults: UserDefaults = .standard) -> Environment {
        var excludeApplications = [CPYAppInfo]()
        if let data = defaults.data(forKey: Constants.UserDefaults.excludeApplications), let applications = NSKeyedUnarchiver.unarchiveObject(with: data) as? [CPYAppInfo] {
            excludeApplications = applications
        }
        let excludeAppService = ExcludeAppService(applications: excludeApplications)
        return Environment(excludeAppService: excludeAppService)
    }

 }

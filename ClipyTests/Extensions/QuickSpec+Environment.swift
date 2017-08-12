import Quick
@testable import Clipy

extension QuickSpec {

    func withEnvironment(_ environment: Environment, closure: () -> Void) {
        AppEnvironment.push(environment: environment)
        closure()
        AppEnvironment.popLast()
    }

    func withEnvironment(clipService: ClipService = AppEnvironment.current.clipService,
                         hotKeyService: HotKeyService = AppEnvironment.current.hotKeyService,
                         dataCleanService: DataCleanService = AppEnvironment.current.dataCleanService,
                         pasteService: PasteService = AppEnvironment.current.pasteService,
                         excludeAppService: ExcludeAppService = AppEnvironment.current.excludeAppService,
                         menuManager: MenuManager = AppEnvironment.current.menuManager,
                         defaults: KeyValueStorable = AppEnvironment.current.defaults,
                         closure: () -> Void) {


        withEnvironment(Environment(clipService: clipService,
                                    hotKeyService: hotKeyService,
                                    dataCleanService: dataCleanService,
                                    pasteService: pasteService,
                                    excludeAppService: excludeAppService,
                                    menuManager: menuManager,
                                    defaults: defaults),
                        closure: closure)
    }
    
}

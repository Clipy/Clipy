//
//  DataCleanService.swift
//  Clipy
//
//  Created by 古林俊佑 on 2016/11/20.
//  Copyright © 2016年 Shunsuke Furubayashi. All rights reserved.
//

import Foundation
import RxSwift
import RealmSwift
import PINCache

final class DataCleanService {

    // MARK: - Properties
    static let shared = DataCleanService()

    fileprivate let disposeBag = DisposeBag()
    fileprivate let scheduler = SerialDispatchQueueScheduler(qos: .utility)
    fileprivate let defaults = UserDefaults.standard

    // MARK: - Initialize
    init() {
        // Clean datas every 30 minutes
        Observable<Int>.interval(60 * 30, scheduler: scheduler)
            .subscribe(onNext: { [weak self] _ in
                self?.cleanDatas()
            })
            .addDisposableTo(disposeBag)
    }

    // MARK: - Delete Data
    func cleanDatas() {
        trimDatabase()
        clean()
    }

    private func clean() {
        let fileManager = FileManager.default
        guard let paths = try? fileManager.contentsOfDirectory(atPath: CPYUtilities.applicationSupportFolder()) else { return }

        let realm = try! Realm()
        let allClipPaths = Array(realm.objects(CPYClip.self)
                                    .filter { !$0.isInvalidated }
                                    .flatMap { $0.dataPath.components(separatedBy: "/").last })

        // Delete diff datas
        DispatchQueue.main.async {
            Set(allClipPaths).symmetricDifference(paths)
                .map { CPYUtilities.applicationSupportFolder() + "/" + "\($0)" }
                .forEach { CPYUtilities.deleteData(at: $0) }
        }
    }

    private func trimDatabase() {
        let realm = try! Realm()
        let clips = realm.objects(CPYClip.self).sorted(byProperty: "updateTime", ascending: false)
        let maxHistorySize = defaults.integer(forKey: Constants.UserDefaults.maxHistorySize)

        if clips.count <= maxHistorySize { return }
        // Delete first clip
        let lastClip = clips[maxHistorySize - 1]
        if lastClip.isInvalidated { return }

        // Deletion target
        let updateTime = lastClip.updateTime
        let targetClips = realm.objects(CPYClip.self).filter("updateTime < %d", updateTime)

        // Delete saved images
        targetClips
            .filter { !$0.isInvalidated && !$0.thumbnailPath.isEmpty }
            .map { $0.thumbnailPath }
            .forEach { PINCache.shared().removeObject(forKey: $0) }
        // Delete at database
        realm.transaction {
            realm.delete(targetClips)
        }
    }
}

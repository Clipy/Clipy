////////////////////////////////////////////////////////////////////////////
//
// Copyright 2016 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

import Realm
import Realm.Private
import Foundation

/**
 An object representing a Realm Object Server user.

 - see: `SyncUser`
 */
public typealias SyncUser = RLMSyncUser

/**
 A singleton which configures and manages the Realm Object Server synchronization-related
 functionality.

 - see: `RLMSyncManager`
 */
public typealias SyncManager = RLMSyncManager

extension SyncManager {
    /// The sole instance of the singleton.
    public static var shared: SyncManager {
        return __shared()
    }
}

/**
 A session object which represents communication between the client and server for a specific
 Realm.

 - see: `RLMSyncSession`
 */
public typealias SyncSession = RLMSyncSession

/**
 A closure type for a closure which can be set on the `SyncManager` to allow errors to be reported
 to the application.

 - see: `RLMSyncErrorReportingBlock`
 */
public typealias ErrorReportingBlock = RLMSyncErrorReportingBlock

/**
 A closure type for a closure which is used by certain APIs to asynchronously return a `SyncUser`
 object to the application.

 - see: `RLMUserCompletionBlock`
 */
public typealias UserCompletionBlock = RLMUserCompletionBlock

/**
 An error associated with the SDK's synchronization functionality. All errors reported by
 an error handler registered on the `SyncManager` are of this type.

 - see: `RLMSyncError`
 */
public typealias SyncError = RLMSyncError

/**
 An error associated with network requests made to the authentication server. This type of error
 may be returned in the callback block to `SyncUser.logIn()` upon certain types of failed login
 attempts (for example, if the request is malformed or if the server is experiencing an issue).

 - see: `RLMSyncAuthError`
 */
public typealias SyncAuthError = RLMSyncAuthError

/**
 An error associated with retrieving or modifying user permissions to access a synchronized Realm.

 - see: `RLMSyncPermissionError`
 */
public typealias SyncPermissionError = RLMSyncPermissionError

/**
 An enum which can be used to specify the level of logging.

 - see: `RLMSyncLogLevel`
 */
public typealias SyncLogLevel = RLMSyncLogLevel

/**
 An enum representing the different states a sync management object can take.

 - see: `RLMSyncManagementObjectStatus`
 */
public typealias SyncManagementObjectStatus = RLMSyncManagementObjectStatus

/**
 A data type whose values represent different authentication providers that can be used with
 the Realm Object Server.

 - see: `RLMIdentityProvider`
 */
public typealias Provider = RLMIdentityProvider

public extension SyncError {
    /// Given a client reset error, extract and return the recovery file path and the reset closure.
    public func clientResetInfo() -> (String, () -> Void)? {
        if code == SyncError.clientResetError,
            let recoveryPath = userInfo[kRLMSyncPathOfRealmBackupCopyKey] as? String,
            let block = _nsError.__rlmSync_clientResetBlock() {
            return (recoveryPath, block)
        }
        return nil
    }
}

/**
 A `SyncConfiguration` represents configuration parameters for Realms intended to sync with
 a Realm Object Server.
 */
public struct SyncConfiguration {
    /// The `SyncUser` who owns the Realm that this configuration should open.
    public let user: SyncUser

    /**
     The URL of the Realm on the Realm Object Server that this configuration should open.

     - warning: The URL must be absolute (e.g. `realms://example.com/~/foo`), and cannot end with
                `.realm`, `.realm.lock` or `.realm.management`.
     */
    public let realmURL: URL

    /**
     A policy that determines what should happen when all references to Realms opened by this
     configuration go out of scope.
     */
    internal let stopPolicy: RLMSyncStopPolicy

    /**
     Whether the SSL certificate of the Realm Object Server should be validated.
     */
    public let enableSSLValidation: Bool

    internal init(config: RLMSyncConfiguration) {
        self.user = config.user
        self.realmURL = config.realmURL
        self.stopPolicy = config.stopPolicy
        self.enableSSLValidation = config.enableSSLValidation
    }

    func asConfig() -> RLMSyncConfiguration {
        let config = RLMSyncConfiguration(user: user, realmURL: realmURL)
        config.stopPolicy = stopPolicy
        config.enableSSLValidation = enableSSLValidation
        return config
    }

    /**
     Initialize a sync configuration with a user and a Realm URL.

     Additional settings can be optionally specified. Descriptions of these
     settings follow.
     
     `enableSSLValidation` is true by default. It can be disabled for debugging
     purposes.

     - warning: The URL must be absolute (e.g. `realms://example.com/~/foo`), and cannot end with
                `.realm`, `.realm.lock` or `.realm.management`.
     
     - warning: NEVER disable SSL validation for a system running in production.
     */
    public init(user: SyncUser, realmURL: URL, enableSSLValidation: Bool = true) {
        self.user = user
        self.realmURL = realmURL
        self.stopPolicy = .afterChangesUploaded
        self.enableSSLValidation = enableSSLValidation
    }
}

/// A `SyncCredentials` represents data that uniquely identifies a Realm Object Server user.
public struct SyncCredentials {
    public typealias Token = String

    internal var token: Token
    internal var provider: Provider
    internal var userInfo: [String: Any]

    /**
     Initialize new credentials using a custom token, authentication provider, and user information
     dictionary. In most cases, the convenience initializers should be used instead.
     */
    public init(customToken token: Token, provider: Provider, userInfo: [String: Any] = [:]) {
        self.token = token
        self.provider = provider
        self.userInfo = userInfo
    }

    internal init(_ credentials: RLMSyncCredentials) {
        self.token = credentials.token
        self.provider = credentials.provider
        self.userInfo = credentials.userInfo
    }

    /// Initialize new credentials using a Facebook account token.
    public static func facebook(token: Token) -> SyncCredentials {
        return SyncCredentials(RLMSyncCredentials(facebookToken: token))
    }

    /// Initialize new credentials using a Google account token.
    public static func google(token: Token) -> SyncCredentials {
        return SyncCredentials(RLMSyncCredentials(googleToken: token))
    }

    /// Initialize new credentials using a CloudKit account token.
    public static func cloudKit(token: Token) -> SyncCredentials {
        return SyncCredentials(RLMSyncCredentials(cloudKitToken: token))
    }

    /// Initialize new credentials using a Realm Object Server username and password.
    public static func usernamePassword(username: String,
                                        password: String,
                                        register: Bool = false) -> SyncCredentials {
        return SyncCredentials(RLMSyncCredentials(username: username, password: password, register: register))
    }

    /// Initialize new credentials using a Realm Object Server access token.
    public static func accessToken(_ accessToken: String, identity: String) -> SyncCredentials {
        return SyncCredentials(RLMSyncCredentials(accessToken: accessToken, identity: identity))
    }
}

extension RLMSyncCredentials {
    internal convenience init(_ credentials: SyncCredentials) {
        self.init(customToken: credentials.token, provider: credentials.provider, userInfo: credentials.userInfo)
    }
}

extension SyncUser {
    /**
     Given credentials and a server URL, log in a user and asynchronously return a `SyncUser`
     object which can be used to open `Realm`s and retrieve `SyncSession`s.
     */
    public static func logIn(with credentials: SyncCredentials,
                             server authServerURL: URL,
                             timeout: TimeInterval = 30,
                             onCompletion completion: @escaping UserCompletionBlock) {
        return SyncUser.__logIn(with: RLMSyncCredentials(credentials),
                                authServerURL: authServerURL,
                                timeout: timeout,
                                onCompletion: completion)
    }

    /// A dictionary of all valid, logged-in user identities corresponding to their `SyncUser` objects.
    public static var all: [String: SyncUser] {
        return __allUsers()
    }

    /**
     The logged-in user. `nil` if none exists. Only use this property if your application expects
     no more than one logged-in user at any given time.

     - warning: Throws an Objective-C exception if more than one logged-in user exists.
     */
    public static var current: SyncUser? {
        return __current()
    }

    /**
     Returns an instance of the Management Realm owned by the user.

     This Realm can be used to control access permissions for Realms managed by the user.
     This includes granting other users access to Realms.
     */
    public func managementRealm() throws -> Realm {
        var config = Realm.Configuration.fromRLMRealmConfiguration(.managementConfiguration(for: self))
        guard let permissionChangeClass = NSClassFromString("RealmSwift.SyncPermissionChange") as? Object.Type else {
            fatalError("Internal error: could not build `SyncPermissionChange` metaclass from string.")
        }
        config.objectTypes = [permissionChangeClass,
                              SyncPermissionOffer.self,
                              SyncPermissionOfferResponse.self]
        return try Realm(configuration: config)
    }

    /**
     Returns an instance of the Permission Realm owned by the user.

     This read-only Realm contains `SyncPermission` objects reflecting the
     synchronized Realms and permission details this user has access to.
     */
    @available(*, deprecated, message: "Use SyncUser.retrievePermissions()")
    public func permissionRealm() throws -> Realm {
        var config = Realm.Configuration.fromRLMRealmConfiguration(.permissionConfiguration(for: self))
        config.objectTypes = [SyncPermission.self]
        return try Realm(configuration: config)
    }
}

/**
 A value which represents a permission granted to a user to interact
 with a Realm. These values are passed into APIs on `SyncUser`, and
 returned from `SyncPermissionResults`.

 - see: `RLMSyncPermissionValue`
 */
public typealias SyncPermissionValue = RLMSyncPermissionValue

/**
 An enumeration describing possible access levels.

 - see: `RLMSyncAccessLevel`
 */
public typealias SyncAccessLevel = RLMSyncAccessLevel

/**
 A collection of `SyncPermissionValue`s that represent the permissions
 that have been configured on all the Realms that some user is allowed
 to administer.

 - see: `RLMSyncPermissionResults`
 */
public typealias SyncPermissionResults = RLMSyncPermissionResults

#if swift(>=3.1)
extension SyncPermissionResults: RandomAccessCollection {
    public subscript(index: Int) -> SyncPermissionValue {
        return object(at: index)
    }

    public func index(after i: Int) -> Int {
        return i + 1
    }

    public var startIndex: Int {
        return 0
    }

    public var endIndex: Int {
        return count
    }
}
#else
extension SyncPermissionResults {
    /// Return the first permission value in the results, or `nil` if
    /// the results are empty.
    public var first: SyncPermissionValue? {
        return count > 0 ? object(at: 0) : nil
    }

    /// Return the last permission value in the results, or `nil` if
    /// the results are empty.
    public var last: SyncPermissionValue? {
        return count > 0 ? object(at: count - 1) : nil
    }
}

extension SyncPermissionResults: Sequence {
    public struct Iterator: IteratorProtocol {
        private let iteratorBase: NSFastEnumerationIterator

        fileprivate init(results: SyncPermissionResults) {
            iteratorBase = NSFastEnumerationIterator(results)
        }

        public func next() -> SyncPermissionValue? {
            return iteratorBase.next() as! SyncPermissionValue?
        }
    }

    public func makeIterator() -> SyncPermissionResults.Iterator {
        return Iterator(results: self)
    }
}
#endif

/**
 This model is used to reflect permissions.

 It should be used in conjunction with a `SyncUser`'s Permission Realm.
 You can only read this Realm. Use the objects in Management Realm to
 make request for modifications of permissions.

 See https://realm.io/docs/realm-object-server/#permissions for general
 documentation.
 */
@available(*, deprecated, message: "Use `SyncPermissionValue`")
public final class SyncPermission: Object {
    /// The date this object was last modified.
    @objc public dynamic var updatedAt = Date()

    /// The ID of the affected user by the permission.
    @objc public dynamic var userId = ""
    /// The path to the realm.
    @objc public dynamic var path = ""

    /// Whether the affected user is allowed to read from the Realm.
    @objc public dynamic var mayRead = false
    /// Whether the affected user is allowed to write to the Realm.
    @objc public dynamic var mayWrite = false
    /// Whether the affected user is allowed to manage the access rights for others.
    @objc public dynamic var mayManage = false

    /// :nodoc:
    override public class func shouldIncludeInDefaultSchema() -> Bool {
        return false
    }

    /// :nodoc:
    override public class func _realmObjectName() -> String? {
        return "Permission"
    }
}

/**
 This model is used for requesting changes to a Realm's permissions.

 It should be used in conjunction with a `SyncUser`'s Management Realm.

 See https://realm.io/docs/realm-object-server/#permissions for general
 documentation.
 */
@available(*, deprecated, message: "Use `SyncUser.applyPermission()` and `SyncUser.revokePermission()`")
public final class SyncPermissionChange: Object {
    /// The globally unique ID string of this permission change object.
    @objc public dynamic var id = UUID().uuidString
    /// The date this object was initially created.
    @objc public dynamic var createdAt = Date()
    /// The date this object was last modified.
    @objc public dynamic var updatedAt = Date()

    /// The status code of the object that was processed by Realm Object Server.
    public let statusCode = RealmOptional<Int>()
    /// An error or informational message, typically written to by the Realm Object Server.
    @objc public dynamic var statusMessage: String?

    /// Sync management object status.
    public var status: SyncManagementObjectStatus {
        return SyncManagementObjectStatus(statusCode: statusCode)
    }
    /// The remote URL to the realm.
    @objc public dynamic var realmUrl = "*"
    /// The identity of a user affected by this permission change.
    @objc public dynamic var userId = "*"

    /// Define read access. Set to `true` or `false` to update this value. Leave unset
    /// to preserve the existing setting.
    public let mayRead = RealmOptional<Bool>()
    /// Define write access. Set to `true` or `false` to update this value. Leave unset
    /// to preserve the existing setting.
    public let mayWrite = RealmOptional<Bool>()
    /// Define management access. Set to `true` or `false` to update this value. Leave
    /// unset to preserve the existing setting.
    public let mayManage = RealmOptional<Bool>()

    /**
     Construct a permission change object used to change the access permissions for a user on a Realm.

     - parameter realmURL:  The Realm URL whose permissions settings should be changed.
                            Use `*` to change the permissions of all Realms managed by the Management Realm's `SyncUser`.
     - parameter userID:    The user or users who should be granted these permission changes.
                            Use `*` to change the permissions for all users.
     - parameter mayRead:   Define read access. Set to `true` or `false` to update this value.
                            Leave unset to preserve the existing setting.
     - parameter mayWrite:  Define write access. Set to `true` or `false` to update this value.
                            Leave unset to preserve the existing setting.
     - parameter mayManage: Define management access. Set to `true` or `false` to update this value.
                            Leave unset to preserve the existing setting.
     */
    public convenience init(realmURL: String, userID: String, mayRead: Bool?, mayWrite: Bool?, mayManage: Bool?) {
        self.init()
        self.realmUrl = realmURL
        self.userId = userID
        self.mayRead.value = mayRead
        self.mayWrite.value = mayWrite
        self.mayManage.value = mayManage
    }

    /// :nodoc:
    override public class func primaryKey() -> String? {
        return "id"
    }

    /// :nodoc:
    override public class func shouldIncludeInDefaultSchema() -> Bool {
        return false
    }

    /// :nodoc:
    override public class func _realmObjectName() -> String? {
        return "PermissionChange"
    }
}

/**
 This model is used for offering permission changes to other users.

 It should be used in conjunction with a `SyncUser`'s Management Realm.

 See https://realm.io/docs/realm-object-server/#permissions for general
 documentation.
 */
public final class SyncPermissionOffer: Object {
    /// The globally unique ID string of this permission offer object.
    @objc public dynamic var id = UUID().uuidString
    /// The date this object was initially created.
    @objc public dynamic var createdAt = Date()
    /// The date this object was last modified.
    @objc public dynamic var updatedAt = Date()

    /// The status code of the object that was processed by Realm Object Server.
    public let statusCode = RealmOptional<Int>()
    /// An error or informational message, typically written to by the Realm Object Server.
    @objc public dynamic var statusMessage: String?

    /// Sync management object status.
    public var status: SyncManagementObjectStatus {
        return SyncManagementObjectStatus(statusCode: statusCode)
    }
    /// A token which uniquely identifies this offer. Generated by the server.
    @objc public dynamic var token: String?
    /// The remote URL to the realm.
    @objc public dynamic var realmUrl = ""

    /// Whether this offer allows the receiver to read from the Realm.
    @objc public dynamic var mayRead = false
    /// Whether this offer allows the receiver to write to the Realm.
    @objc public dynamic var mayWrite = false
    /// Whether this offer allows the receiver to manage the access rights for others.
    @objc public dynamic var mayManage = false

    /// When this token will expire and become invalid.
    @objc public dynamic var expiresAt: Date?

    /**
     Construct a permission offer object used to offer permission changes to other users.

     - parameter realmURL:  The URL to the Realm on which to apply these permission changes
                            to, once the offer is accepted.
     - parameter expiresAt: When this token will expire and become invalid.
                            Pass `nil` if this offer should not expire.
     - parameter mayRead:   Grant or revoke read access.
     - parameter mayWrite:  Grant or revoked read-write access.
     - parameter mayManage: Grant or revoke administrative access.
     */
    public convenience init(realmURL: String, expiresAt: Date?, mayRead: Bool, mayWrite: Bool, mayManage: Bool) {
        self.init()
        self.realmUrl = realmURL
        self.expiresAt = expiresAt
        self.mayRead = mayRead
        self.mayWrite = mayWrite
        self.mayManage = mayManage
    }

    /// :nodoc:
    override public class func indexedProperties() -> [String] {
        return ["token"]
    }

    /// :nodoc:
    override public class func primaryKey() -> String? {
        return "id"
    }

    /// :nodoc:
    override public class func shouldIncludeInDefaultSchema() -> Bool {
        return false
    }

    /// :nodoc:
    override public class func _realmObjectName() -> String? {
        return "PermissionOffer"
    }
}

/**
 This model is used to apply permission changes defined in the permission offer
 object represented by the specified token, which was created by another user's
 `SyncPermissionOffer` object.

 It should be used in conjunction with a `SyncUser`'s Management Realm.

 See https://realm.io/docs/realm-object-server/#permissions for general
 documentation.
 */
public final class SyncPermissionOfferResponse: Object {
    /// The globally unique ID string of this permission offer response object.
    @objc public dynamic var id = UUID().uuidString
    /// The date this object was initially created.
    @objc public dynamic var createdAt = Date()
    /// The date this object was last modified.
    @objc public dynamic var updatedAt = Date()

    /// The status code of the object that was processed by Realm Object Server.
    public let statusCode = RealmOptional<Int>()
    /// An error or informational message, typically written to by the Realm Object Server.
    @objc public dynamic var statusMessage: String?

    /// Sync management object status.
    public var status: SyncManagementObjectStatus {
        return SyncManagementObjectStatus(statusCode: statusCode)
    }
    /// The received token which uniquely identifies another user's `SyncPermissionOffer`.
    @objc public dynamic var token = ""
    /// The remote URL to the realm on which these permission changes were applied.
    @objc public dynamic var realmUrl: String?

    /**
     Construct a permission offer response object used to apply permission changes
     defined in the permission offer object represented by the specified token,
     which was created by another user's `SyncPermissionOffer` object.

     - parameter token: The received token which uniquely identifies another user's
                        `SyncPermissionOffer`.
     */
    public convenience init(token: String) {
        self.init()
        self.token = token
    }

    /// :nodoc:
    override public class func primaryKey() -> String? {
        return "id"
    }

    /// :nodoc:
    override public class func shouldIncludeInDefaultSchema() -> Bool {
        return false
    }

    /// :nodoc:
    override public class func _realmObjectName() -> String? {
        return "PermissionOfferResponse"
    }
}

fileprivate extension SyncManagementObjectStatus {
    fileprivate init(statusCode: RealmOptional<Int>) {
        guard let statusCode = statusCode.value else {
            self = .notProcessed
            return
        }
        if statusCode == 0 {
            self = .success
        } else {
            self = .error
        }
    }
}

public extension SyncSession {
    /**
     The transfer direction (upload or download) tracked by a given progress notification block.

     Progress notification blocks can be registered on sessions if your app wishes to be informed
     how many bytes have been uploaded or downloaded, for example to show progress indicator UIs.
     */
    public enum ProgressDirection {
        /// For monitoring upload progress.
        case upload
        /// For monitoring download progress.
        case download
    }

    /**
     The desired behavior of a progress notification block.

     Progress notification blocks can be registered on sessions if your app wishes to be informed
     how many bytes have been uploaded or downloaded, for example to show progress indicator UIs.
     */
    public enum ProgressMode {
        /**
         The block will be called forever, or until it is unregistered by calling
         `ProgressNotificationToken.stop()`.

         Notifications will always report the latest number of transferred bytes, and the
         most up-to-date number of total transferrable bytes.
         */
        case reportIndefinitely
        /**
         The block will, upon registration, store the total number of bytes
         to be transferred. When invoked, it will always report the most up-to-date number
         of transferrable bytes out of that original number of transferrable bytes.

         When the number of transferred bytes reaches or exceeds the
         number of transferrable bytes, the block will be unregistered.
         */
        case forCurrentlyOutstandingWork
    }

    /**
     A token corresponding to a progress notification block.

     Call `stop()` on the token to stop notifications. If the notification block has already
     been automatically stopped, calling `stop()` does nothing. `stop()` should be called
     before the token is destroyed.
     */
    public typealias ProgressNotificationToken = RLMProgressNotificationToken

    /**
     A struct encapsulating progress information, as well as useful helper methods.
     */
    public struct Progress {
        /// The number of bytes that have been transferred.
        public let transferredBytes: Int

        /**
         The total number of transferrable bytes (bytes that have been transferred,
         plus bytes pending transfer).

         If the notification block is tracking downloads, this number represents the size of the
         changesets generated by all other clients using the Realm.
         If the notification block is tracking uploads, this number represents the size of the
         changesets representing the local changes on this client.
         */
        public let transferrableBytes: Int

        /// The fraction of bytes transferred out of all transferrable bytes. If this value is 1,
        /// no bytes are waiting to be transferred (either all bytes have already been transferred,
        /// or there are no bytes to be transferred in the first place).
        public var fractionTransferred: Double {
            if transferrableBytes == 0 {
                return 1
            }
            let percentage = Double(transferredBytes) / Double(transferrableBytes)
            return percentage > 1 ? 1 : percentage
        }

        /// Whether all pending bytes have already been transferred.
        public var isTransferComplete: Bool {
            return transferredBytes >= transferrableBytes
        }

        fileprivate init(transferred: UInt, transferrable: UInt) {
            transferredBytes = Int(transferred)
            transferrableBytes = Int(transferrable)
        }
    }

    /**
     Register a progress notification block.

     If the session has already received progress information from the
     synchronization subsystem, the block will be called immediately. Otherwise, it
     will be called as soon as progress information becomes available.

     Multiple blocks can be registered with the same session at once. Each block
     will be invoked on a side queue devoted to progress notifications.

     The token returned by this method must be retained as long as progress
     notifications are desired, and the `stop()` method should be called on it
     when notifications are no longer needed and before the token is destroyed.

     If no token is returned, the notification block will never be called again.
     There are a number of reasons this might be true. If the session has previously
     experienced a fatal error it will not accept progress notification blocks. If
     the block was configured in the `forCurrentlyOutstandingWork` mode but there
     is no additional progress to report (for example, the number of transferrable bytes
     and transferred bytes are equal), the block will not be called again.

     - parameter direction: The transfer direction (upload or download) to track in this progress notification block.
     - parameter mode:      The desired behavior of this progress notification block.
     - parameter block:     The block to invoke when notifications are available.

     - returns: A token which must be held for as long as you want notifications to be delivered.

     - see: `ProgressDirection`, `Progress`, `ProgressNotificationToken`
     */
    public func addProgressNotification(for direction: ProgressDirection,
                                        mode: ProgressMode,
                                        block: @escaping (Progress) -> Void) -> ProgressNotificationToken? {
        return __addProgressNotification(for: (direction == .upload ? .upload : .download),
                                         mode: (mode == .reportIndefinitely
                                            ? .reportIndefinitely
                                            : .forCurrentlyOutstandingWork)) { transferred, transferrable in
                                                block(Progress(transferred: transferred, transferrable: transferrable))
        }
    }
}

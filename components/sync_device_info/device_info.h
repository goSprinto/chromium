// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_H_
#define COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_H_

#include <memory>
#include <set>
#include <string>

#include "base/callback.h"
#include "base/macros.h"
#include "base/optional.h"
#include "base/time/time.h"
#include "components/sync/protocol/sync.pb.h"

namespace base {
class DictionaryValue;
}

namespace syncer {

// A class that holds information regarding the properties of a device.
class DeviceInfo {
 public:
  struct SharingInfo {
    SharingInfo(std::string fcm_token,
                std::string p256dh,
                std::string auth_secret,
                std::set<sync_pb::SharingSpecificFields::EnabledFeatures>
                    enabled_features);
    SharingInfo(const SharingInfo& other);
    SharingInfo(SharingInfo&& other);
    SharingInfo& operator=(const SharingInfo& other);
    ~SharingInfo();

    // FCM registration token of device for sending Sharing messages.
    std::string fcm_token;

    // Subscription public key required for Sharing message encryption[RFC8291].
    std::string p256dh;

    // Auth secret key required for Sharing message encryption[RFC8291].
    std::string auth_secret;

    // Set of Sharing features enabled on the device.
    std::set<sync_pb::SharingSpecificFields::EnabledFeatures> enabled_features;

    bool operator==(const SharingInfo& other) const;
  };

  DeviceInfo(const std::string& guid,
             const std::string& client_name,
             const std::string& chrome_version,
             const std::string& sync_user_agent,
             const sync_pb::SyncEnums::DeviceType device_type,
             const std::string& signin_scoped_device_id,
             base::Time last_updated_timestamp,
             bool send_tab_to_self_receiving_enabled,
             const base::Optional<SharingInfo>& sharing_info);
  ~DeviceInfo();

  // Sync specific unique identifier for the device. Note if a device
  // is wiped and sync is set up again this id WILL be different.
  // The same device might have more than 1 guid if the device has multiple
  // accounts syncing.
  const std::string& guid() const;

  // The host name for the client.
  const std::string& client_name() const;

  // Chrome version string.
  const std::string& chrome_version() const;

  // The user agent is the combination of OS type, chrome version and which
  // channel of chrome(stable or beta). For more information see
  // |LocalDeviceInfoProviderImpl::MakeUserAgentForSyncApi|.
  const std::string& sync_user_agent() const;

  // Third party visible id for the device. See |public_id_| for more details.
  const std::string& public_id() const;

  // Device Type.
  sync_pb::SyncEnums::DeviceType device_type() const;

  // Device_id that is stable until user signs out. This device_id is used for
  // annotating login scoped refresh token.
  const std::string& signin_scoped_device_id() const;

  // Returns the time at which this device was last updated to the sync servers.
  base::Time last_updated_timestamp() const;

  // Whether the receiving side of the SendTabToSelf feature is enabled.
  bool send_tab_to_self_receiving_enabled() const;

  // Returns Sharing related info of the device.
  const base::Optional<SharingInfo>& sharing_info() const;

  // Gets the OS in string form.
  std::string GetOSString() const;

  // Gets the device type in string form.
  std::string GetDeviceTypeString() const;

  // Compares this object's fields with another's.
  bool Equals(const DeviceInfo& other) const;

  // Apps can set ids for a device that is meaningful to them but
  // not unique enough so the user can be tracked. Exposing |guid|
  // would lead to a stable unique id for a device which can potentially
  // be used for tracking.
  void set_public_id(const std::string& id);

  void set_send_tab_to_self_receiving_enabled(bool new_value);

  void set_sharing_info(const base::Optional<SharingInfo>& sharing_info);

  // Converts the |DeviceInfo| values to a JS friendly DictionaryValue,
  // which extension APIs can expose to third party apps.
  std::unique_ptr<base::DictionaryValue> ToValue();

 private:
  const std::string guid_;

  const std::string client_name_;

  const std::string chrome_version_;

  const std::string sync_user_agent_;

  const sync_pb::SyncEnums::DeviceType device_type_;

  std::string signin_scoped_device_id_;

  // Exposing |guid| would lead to a stable unique id for a device which
  // can potentially be used for tracking. Public ids are privacy safe
  // ids in that the same device will have different id for different apps
  // and they are also reset when app/extension is uninstalled.
  std::string public_id_;

  const base::Time last_updated_timestamp_;

  bool send_tab_to_self_receiving_enabled_;

  base::Optional<SharingInfo> sharing_info_;

  DISALLOW_COPY_AND_ASSIGN(DeviceInfo);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_DEVICE_INFO_DEVICE_INFO_H_

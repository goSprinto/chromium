// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_SYNC_NIGORI_NIGORI_SYNC_BRIDGE_IMPL_H_
#define COMPONENTS_SYNC_NIGORI_NIGORI_SYNC_BRIDGE_IMPL_H_

#include <memory>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/optional.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/sync/engine/sync_encryption_handler.h"
#include "components/sync/model/conflict_resolution.h"
#include "components/sync/model/model_error.h"
#include "components/sync/nigori/cryptographer_impl.h"
#include "components/sync/nigori/keystore_keys_handler.h"
#include "components/sync/nigori/nigori_local_change_processor.h"
#include "components/sync/nigori/nigori_state.h"
#include "components/sync/nigori/nigori_sync_bridge.h"

namespace sync_pb {
class NigoriLocalData;
}  // namespace sync_pb

namespace syncer {

class Encryptor;
class NigoriStorage;

// USS implementation of SyncEncryptionHandler.
// This class holds the current Nigori state and processes incoming changes and
// queries:
// 1. Serves observers of SyncEncryptionHandler interface.
// 2. Allows the passphrase manipulations (via SyncEncryptionHandler).
// 3. Communicates local and remote changes with a processor (via
// NigoriSyncBridge).
// 4. Handles keystore keys from a sync server (via KeystoreKeysHandler).
class NigoriSyncBridgeImpl : public KeystoreKeysHandler,
                             public NigoriSyncBridge,
                             public SyncEncryptionHandler {
 public:
  // |encryptor| must be not null and must outlive this object.
  NigoriSyncBridgeImpl(
      std::unique_ptr<NigoriLocalChangeProcessor> processor,
      std::unique_ptr<NigoriStorage> storage,
      const Encryptor* encryptor,
      const base::RepeatingCallback<std::string()>& random_salt_generator,
      const std::string& packed_explicit_passphrase_key);
  ~NigoriSyncBridgeImpl() override;

  // SyncEncryptionHandler implementation.
  void AddObserver(Observer* observer) override;
  void RemoveObserver(Observer* observer) override;
  bool Init() override;
  void SetEncryptionPassphrase(const std::string& passphrase) override;
  void SetDecryptionPassphrase(const std::string& passphrase) override;
  void AddTrustedVaultDecryptionKeys(
      const std::vector<std::string>& keys) override;
  void EnableEncryptEverything() override;
  bool IsEncryptEverythingEnabled() const override;
  base::Time GetKeystoreMigrationTime() const override;
  KeystoreKeysHandler* GetKeystoreKeysHandler() override;

  // KeystoreKeysHandler implementation.
  bool NeedKeystoreKey() const override;
  bool SetKeystoreKeys(const std::vector<std::string>& keys) override;

  // NigoriSyncBridge implementation.
  base::Optional<ModelError> MergeSyncData(
      base::Optional<EntityData> data) override;
  base::Optional<ModelError> ApplySyncChanges(
      base::Optional<EntityData> data) override;
  std::unique_ptr<EntityData> GetData() override;
  ConflictResolution ResolveConflict(const EntityData& local_data,
                                     const EntityData& remote_data) override;
  void ApplyDisableSyncChanges() override;

  // TODO(crbug.com/922900): investigate whether we need this getter outside of
  // tests and decide whether this method should be a part of
  // SyncEncryptionHandler interface.
  const Cryptographer& GetCryptographerForTesting() const;
  sync_pb::NigoriSpecifics::PassphraseType GetPassphraseTypeForTesting() const;
  ModelTypeSet GetEncryptedTypesForTesting() const;
  bool HasPendingKeysForTesting() const;

  static std::string PackExplicitPassphraseKeyForTesting(
      const Encryptor& encryptor,
      const CryptographerImpl& cryptographer);

 private:
  base::Optional<ModelError> UpdateLocalState(
      const sync_pb::NigoriSpecifics& specifics);

  base::Optional<ModelError> UpdateCryptographerFromKeystoreNigori(
      const sync_pb::EncryptedData& encryption_keybag,
      const sync_pb::EncryptedData& keystore_decryptor_token);

  void UpdateCryptographerFromNonKeystoreNigori(
      const sync_pb::EncryptedData& keybag);

  // Uses the cryptographer to try to decrypt pending keys. If success, the
  // newly decrypted keys are put in the cryptographer's keybag, pending keys
  // are cleared and the function returns true. Otherwise, it returns false and
  // the state remains unchanged. It does not change the default key.
  bool TryDecryptPendingKeys();

  base::Time GetExplicitPassphraseTime() const;

  // Returns key derivation params based on |passphrase_type_| and
  // |custom_passphrase_key_derivation_params_|. Should be called only if
  // |passphrase_type_| is an explicit passphrase.
  KeyDerivationParams GetKeyDerivationParamsForPendingKeys() const;

  // If there are pending keys and depending on the passphrase type, it invokes
  // the appropriate observer methods (if any).
  void MaybeNotifyOfPendingKeys() const;

  // Persists Nigori derived from explicit passphrase into preferences, in case
  // error occurs during serialization/encryption, corresponding preference
  // just won't be updated.
  void MaybeNotifyBootstrapTokenUpdated() const;

  // Serializes state of the bridge and sync metadata into the proto.
  sync_pb::NigoriLocalData SerializeAsNigoriLocalData() const;

  const Encryptor* const encryptor_;

  const std::unique_ptr<NigoriLocalChangeProcessor> processor_;
  const std::unique_ptr<NigoriStorage> storage_;

  // Used for generation of random salt for deriving keys from custom
  // passphrase if SCRYPT is enabled.
  const base::RepeatingCallback<std::string()> random_salt_generator_;

  // Stores a key derived from explicit passphrase and loaded from the prefs.
  // Empty (i.e. default value) if prefs doesn't contain this key or in case of
  // decryption/decoding errors.
  const sync_pb::NigoriKey explicit_passphrase_key_;

  syncer::NigoriState state_;

  // TODO(crbug/922900): consider using checked ObserverList once
  // SyncEncryptionHandlerImpl is no longer needed or consider refactoring old
  // implementation to use checked ObserverList as well.
  base::ObserverList<SyncEncryptionHandler::Observer>::Unchecked observers_;

  SEQUENCE_CHECKER(sequence_checker_);

  DISALLOW_COPY_AND_ASSIGN(NigoriSyncBridgeImpl);
};

}  // namespace syncer

#endif  // COMPONENTS_SYNC_NIGORI_NIGORI_SYNC_BRIDGE_IMPL_H_

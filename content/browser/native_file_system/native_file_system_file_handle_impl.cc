// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "content/browser/native_file_system/native_file_system_file_handle_impl.h"

#include "base/guid.h"
#include "base/logging.h"
#include "base/strings/stringprintf.h"
#include "content/browser/native_file_system/native_file_system_error.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "net/base/mime_util.h"
#include "storage/browser/blob/blob_data_builder.h"
#include "storage/browser/blob/blob_impl.h"
#include "storage/browser/blob/blob_storage_context.h"
#include "storage/browser/fileapi/file_system_operation_runner.h"
#include "third_party/blink/public/mojom/blob/blob.mojom.h"
#include "third_party/blink/public/mojom/blob/serialized_blob.mojom.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_error.mojom.h"
#include "third_party/blink/public/mojom/native_file_system/native_file_system_transfer_token.mojom.h"

using blink::mojom::NativeFileSystemStatus;
using storage::BlobDataHandle;
using storage::BlobImpl;
using storage::FileSystemOperation;
using storage::IsolatedContext;

namespace content {

NativeFileSystemFileHandleImpl::NativeFileSystemFileHandleImpl(
    NativeFileSystemManagerImpl* manager,
    const BindingContext& context,
    const storage::FileSystemURL& url,
    const SharedHandleState& handle_state)
    : NativeFileSystemHandleBase(manager,
                                 context,
                                 url,
                                 handle_state,
                                 /*is_directory=*/false) {}

NativeFileSystemFileHandleImpl::~NativeFileSystemFileHandleImpl() = default;

void NativeFileSystemFileHandleImpl::GetPermissionStatus(
    bool writable,
    GetPermissionStatusCallback callback) {
  DoGetPermissionStatus(writable, std::move(callback));
}

void NativeFileSystemFileHandleImpl::RequestPermission(
    bool writable,
    RequestPermissionCallback callback) {
  DoRequestPermission(writable, std::move(callback));
}

void NativeFileSystemFileHandleImpl::AsBlob(AsBlobCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (GetReadPermissionStatus() != PermissionStatus::GRANTED) {
    std::move(callback).Run(native_file_system_error::FromStatus(
                                NativeFileSystemStatus::kPermissionDenied),
                            nullptr);
    return;
  }

  // TODO(mek): Check backend::SupportsStreaming and create snapshot file if
  // streaming is not supported.
  operation_runner()->GetMetadata(
      url(),
      FileSystemOperation::GET_METADATA_FIELD_IS_DIRECTORY |
          FileSystemOperation::GET_METADATA_FIELD_SIZE |
          FileSystemOperation::GET_METADATA_FIELD_LAST_MODIFIED,
      base::BindOnce(&NativeFileSystemFileHandleImpl::DidGetMetaDataForBlob,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void NativeFileSystemFileHandleImpl::CreateFileWriter(
    bool keep_existing_data,
    CreateFileWriterCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  RunWithWritePermission(
      base::BindOnce(&NativeFileSystemFileHandleImpl::CreateFileWriterImpl,
                     weak_factory_.GetWeakPtr(), keep_existing_data),
      base::BindOnce([](CreateFileWriterCallback callback) {
        std::move(callback).Run(native_file_system_error::FromStatus(
                                    NativeFileSystemStatus::kPermissionDenied),
                                mojo::NullRemote());
      }),
      std::move(callback));
}

void NativeFileSystemFileHandleImpl::Transfer(
    mojo::PendingReceiver<blink::mojom::NativeFileSystemTransferToken> token) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  manager()->CreateTransferToken(*this, std::move(token));
}

void NativeFileSystemFileHandleImpl::DidGetMetaDataForBlob(
    AsBlobCallback callback,
    base::File::Error result,
    const base::File::Info& info) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);

  if (result != base::File::FILE_OK) {
    std::move(callback).Run(native_file_system_error::FromFileError(result),
                            nullptr);
    return;
  }

  std::string uuid = base::GenerateGUID();
  auto blob_builder = std::make_unique<storage::BlobDataBuilder>(uuid);

  // Only append if the file has data.
  if (info.size > 0) {
    // Use AppendFileSystemFile here, since we're streaming the file directly
    // from the file system backend, and the file thus might not actually be
    // backed by a file on disk.
    blob_builder->AppendFileSystemFile(url().ToGURL(), 0, info.size,
                                       info.last_modified,
                                       file_system_context());
  }

  base::FilePath::StringType extension = url().path().Extension();
  if (!extension.empty()) {
    std::string mime_type;
    // TODO(https://crbug.com/962306): Using GetMimeTypeFromExtension and
    // including platform defined mime type mappings might be nice/make sense,
    // however that method can potentially block and thus can't be called from
    // the IO thread.
    if (net::GetWellKnownMimeTypeFromExtension(extension.substr(1), &mime_type))
      blob_builder->set_content_type(mime_type);
  }

  std::unique_ptr<BlobDataHandle> blob_handle =
      blob_context()->AddFinishedBlob(std::move(blob_builder));
  if (blob_handle->IsBroken()) {
    std::move(callback).Run(
        native_file_system_error::FromStatus(
            NativeFileSystemStatus::kOperationFailed, "Failed to create blob."),
        nullptr);
    return;
  }

  std::string content_type = blob_handle->content_type();
  mojo::PendingRemote<blink::mojom::Blob> blob_remote;
  BlobImpl::Create(std::move(blob_handle),
                   blob_remote.InitWithNewPipeAndPassReceiver());

  std::move(callback).Run(
      native_file_system_error::Ok(),
      blink::mojom::SerializedBlob::New(uuid, content_type, info.size,
                                        std::move(blob_remote)));
}

void NativeFileSystemFileHandleImpl::CreateFileWriterImpl(
    bool keep_existing_data,
    CreateFileWriterCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK_EQ(GetWritePermissionStatus(),
            blink::mojom::PermissionStatus::GRANTED);

  // We first attempt to create the swap file, even if we might do a subsequent
  // operation to copy a file to the same path if keep_existing_data is set.
  // This file creation has to be `exclusive`, meaning, it will fail if a file
  // already exists. Using the filesystem for synchronization, a successful
  // creation of the file ensures that this File Writer creation request owns
  // the file and eliminates possible race conditions.
  CreateSwapFile(
      /*count=*/0, keep_existing_data, std::move(callback));
}

void NativeFileSystemFileHandleImpl::CreateSwapFile(
    int count,
    bool keep_existing_data,
    CreateFileWriterCallback callback) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  DCHECK(count >= 0);
  DCHECK(max_swap_files_ >= 0);

  if (GetWritePermissionStatus() != blink::mojom::PermissionStatus::GRANTED) {
    std::move(callback).Run(native_file_system_error::FromStatus(
                                NativeFileSystemStatus::kPermissionDenied),
                            mojo::NullRemote());
    return;
  }

  auto swap_path =
      base::FilePath(url().virtual_path()).AddExtensionASCII(".crswap");
  auto swap_file_system = file_system();

  if (count >= max_swap_files_) {
    DLOG(ERROR) << "Error Creating Swap File, count: " << count
                << " exceeds max unique files of: " << max_swap_files_
                << " base path: " << swap_path;
    std::move(callback).Run(native_file_system_error::FromStatus(
                                NativeFileSystemStatus::kOperationFailed,
                                "Failed to create swap file."),
                            mojo::NullRemote());
    return;
  }

  if (count > 0) {
    swap_path =
        swap_path.InsertBeforeExtensionASCII(base::StringPrintf(".%d", count));
  }

  // First attempt to just create the swap file in the same file system as this
  // file.
  storage::FileSystemURL swap_url =
      manager()->context()->CreateCrackedFileSystemURL(
          url().origin().GetURL(), url().mount_type(), swap_path);

  // If that failed, it means this file was part of an isolated file system,
  // and specifically, a single file isolated file system. In that case we'll
  // need to register a new isolated file system for the swap file, and use that
  // for the writer.
  if (!swap_url.is_valid()) {
    DCHECK_EQ(url().mount_type(), storage::kFileSystemTypeIsolated);

    swap_path = base::FilePath(url().path()).AddExtensionASCII(".crswap");
    if (count > 0) {
      swap_path = swap_path.InsertBeforeExtensionASCII(
          base::StringPrintf(".%d", count));
    }

    auto handle =
        manager()->CreateFileSystemURLFromPath(context().origin, swap_path);
    swap_url = std::move(handle.url);
    swap_file_system = std::move(handle.file_system);
  }

  operation_runner()->CreateFile(
      swap_url,
      /*exclusive=*/true,
      base::BindOnce(&NativeFileSystemFileHandleImpl::DidCreateSwapFile,
                     weak_factory_.GetWeakPtr(), count, swap_url,
                     swap_file_system, keep_existing_data,
                     std::move(callback)));
}

void NativeFileSystemFileHandleImpl::DidCreateSwapFile(
    int count,
    const storage::FileSystemURL& swap_url,
    storage::IsolatedContext::ScopedFSHandle swap_file_system,
    bool keep_existing_data,
    CreateFileWriterCallback callback,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (result == base::File::FILE_ERROR_EXISTS) {
    // Creation attempt failed. We need to find an unused filename.
    CreateSwapFile(count + 1, keep_existing_data, std::move(callback));
    return;
  }

  if (result != base::File::FILE_OK) {
    DLOG(ERROR) << "Error Creating Swap File, status: "
                << base::File::ErrorToString(result)
                << " path: " << swap_url.path();
    std::move(callback).Run(native_file_system_error::FromFileError(
                                result, "Error creating swap file."),
                            mojo::NullRemote());
    return;
  }

  if (!keep_existing_data) {
    std::move(callback).Run(
        native_file_system_error::Ok(),
        manager()->CreateFileWriter(
            context(), url(), swap_url,
            NativeFileSystemManagerImpl::SharedHandleState(
                handle_state().read_grant, handle_state().write_grant,
                swap_file_system)));
    return;
  }

  operation_runner()->Copy(
      url(), swap_url,
      storage::FileSystemOperation::OPTION_PRESERVE_LAST_MODIFIED,
      storage::FileSystemOperation::ERROR_BEHAVIOR_ABORT,
      storage::FileSystemOperation::CopyProgressCallback(),
      base::BindOnce(&NativeFileSystemFileHandleImpl::DidCopySwapFile,
                     weak_factory_.GetWeakPtr(), swap_url, swap_file_system,
                     std::move(callback)));
}

void NativeFileSystemFileHandleImpl::DidCopySwapFile(
    const storage::FileSystemURL& swap_url,
    storage::IsolatedContext::ScopedFSHandle swap_file_system,
    CreateFileWriterCallback callback,
    base::File::Error result) {
  DCHECK_CURRENTLY_ON(BrowserThread::IO);
  if (result != base::File::FILE_OK) {
    DLOG(ERROR) << "Error Creating Swap File, status: "
                << base::File::ErrorToString(result)
                << " path: " << swap_url.path();
    std::move(callback).Run(native_file_system_error::FromFileError(
                                result, "Error copying to swap file."),
                            mojo::NullRemote());
    return;
  }
  std::move(callback).Run(
      native_file_system_error::Ok(),
      manager()->CreateFileWriter(
          context(), url(), swap_url,
          NativeFileSystemManagerImpl::SharedHandleState(
              handle_state().read_grant, handle_state().write_grant,
              swap_file_system)));
}

base::WeakPtr<NativeFileSystemHandleBase>
NativeFileSystemFileHandleImpl::AsWeakPtr() {
  return weak_factory_.GetWeakPtr();
}

}  // namespace content

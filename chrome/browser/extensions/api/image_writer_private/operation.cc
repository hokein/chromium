// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/file_util.h"
#include "base/files/file_enumerator.h"
#include "base/threading/worker_pool.h"
#include "chrome/browser/extensions/api/image_writer_private/error_messages.h"
#include "chrome/browser/extensions/api/image_writer_private/operation.h"
#include "chrome/browser/extensions/api/image_writer_private/operation_manager.h"
#include "content/public/browser/browser_thread.h"

namespace extensions {
namespace image_writer {

using content::BrowserThread;

const int kMD5BufferSize = 1024;
#if defined(OS_CHROMEOS)
// Chrome OS only has a 1 GB temporary partition.  This is too small to hold our
// unzipped image. Fortunately we mount part of the temporary partition under
// /var/tmp.
const char kChromeOSTempRoot[] = "/var/tmp";
#endif

Operation::Operation(base::WeakPtr<OperationManager> manager,
                     const ExtensionId& extension_id,
                     const std::string& device_path)
    : manager_(manager),
      extension_id_(extension_id),
#if defined(OS_WIN)
      device_path_(base::FilePath::FromUTF8Unsafe(device_path)),
#else
      device_path_(device_path),
#endif
#if defined(OS_LINUX) && !defined(CHROMEOS)
      image_file_(base::kInvalidPlatformFileValue),
      device_file_(base::kInvalidPlatformFileValue),
#endif
      stage_(image_writer_api::STAGE_UNKNOWN),
      progress_(0) {
}

Operation::~Operation() {}

void Operation::Cancel() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  stage_ = image_writer_api::STAGE_NONE;

  CleanUp();
}

void Operation::Abort() {
  Error(error::kAborted);
}

int Operation::GetProgress() {
  return progress_;
}

image_writer_api::Stage Operation::GetStage() {
  return stage_;
}

void Operation::Start() {
#if defined(OS_CHROMEOS)
  if (!temp_dir_.CreateUniqueTempDirUnderPath(
           base::FilePath(kChromeOSTempRoot))) {
#else
  if (!temp_dir_.CreateUniqueTempDir()) {
#endif
    Error(error::kTempDirError);
    return;
  }

  AddCleanUpFunction(
      base::Bind(base::IgnoreResult(&base::ScopedTempDir::Delete),
                 base::Unretained(&temp_dir_)));

  StartImpl();
}

void Operation::Unzip(const base::Closure& continuation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  if (IsCancelled()) {
    return;
  }

  if (image_path_.Extension() != FILE_PATH_LITERAL(".zip")) {
    BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, continuation);
    return;
  }

  SetStage(image_writer_api::STAGE_UNZIP);

  if (!(zip_reader_.Open(image_path_) && zip_reader_.AdvanceToNextEntry() &&
        zip_reader_.OpenCurrentEntryInZip())) {
    Error(error::kUnzipGenericError);
    return;
  }

  if (zip_reader_.HasMore()) {
    Error(error::kUnzipInvalidArchive);
    return;
  }

  // Create a new target to unzip to.  The original file is opened by the
  // zip_reader_.
  zip::ZipReader::EntryInfo* entry_info = zip_reader_.current_entry_info();
  if (entry_info) {
    image_path_ = temp_dir_.path().Append(entry_info->file_path().BaseName());
  } else {
    Error(error::kTempDirError);
    return;
  }

  zip_reader_.ExtractCurrentEntryToFilePathAsync(
      image_path_,
      base::Bind(&Operation::OnUnzipSuccess, this, continuation),
      base::Bind(&Operation::OnUnzipFailure, this),
      base::Bind(&Operation::OnUnzipProgress,
                 this,
                 zip_reader_.current_entry_info()->original_size()));
}

void Operation::Finish() {
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    BrowserThread::PostTask(
        BrowserThread::FILE, FROM_HERE, base::Bind(&Operation::Finish, this));
    return;
  }

  CleanUp();

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&OperationManager::OnComplete, manager_, extension_id_));
}

void Operation::Error(const std::string& error_message) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    BrowserThread::PostTask(BrowserThread::FILE,
                            FROM_HERE,
                            base::Bind(&Operation::Error, this, error_message));
    return;
  }

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&OperationManager::OnError,
                 manager_,
                 extension_id_,
                 stage_,
                 progress_,
                 error_message));

  CleanUp();
}

void Operation::SetProgress(int progress) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&Operation::SetProgress,
                   this,
                   progress));
    return;
  }

  if (progress <= progress_) {
    return;
  }

  if (IsCancelled()) {
    return;
  }

  progress_ = progress;

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&OperationManager::OnProgress,
                 manager_,
                 extension_id_,
                 stage_,
                 progress_));
}

void Operation::SetStage(image_writer_api::Stage stage) {
  if (!BrowserThread::CurrentlyOn(BrowserThread::FILE)) {
    BrowserThread::PostTask(
        BrowserThread::FILE,
        FROM_HERE,
        base::Bind(&Operation::SetStage,
                   this,
                   stage));
    return;
  }

  if (IsCancelled()) {
    return;
  }

  stage_ = stage;
  progress_ = 0;

  BrowserThread::PostTask(
      BrowserThread::UI,
      FROM_HERE,
      base::Bind(&OperationManager::OnProgress,
                 manager_,
                 extension_id_,
                 stage_,
                 progress_));
}

bool Operation::IsCancelled() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  return stage_ == image_writer_api::STAGE_NONE;
}

void Operation::AddCleanUpFunction(const base::Closure& callback) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  cleanup_functions_.push_back(callback);
}

void Operation::GetMD5SumOfFile(
    const base::FilePath& file_path,
    int64 file_size,
    int progress_offset,
    int progress_scale,
    const base::Callback<void(const std::string&)>& callback) {
  if (IsCancelled()) {
    return;
  }

  base::MD5Init(&md5_context_);

  base::PlatformFile file = base::CreatePlatformFile(
      file_path,
      base::PLATFORM_FILE_OPEN | base::PLATFORM_FILE_READ,
      NULL,
      NULL);
  if (file == base::kInvalidPlatformFileValue) {
    Error(error::kImageOpenError);
    return;
  }

  if (file_size <= 0) {
    if (!base::GetFileSize(file_path, &file_size)) {
      Error(error::kImageOpenError);
      return;
    }
  }

  BrowserThread::PostTask(BrowserThread::FILE,
                          FROM_HERE,
                          base::Bind(&Operation::MD5Chunk,
                                     this,
                                     file,
                                     0,
                                     file_size,
                                     progress_offset,
                                     progress_scale,
                                     callback));
}

void Operation::MD5Chunk(
    const base::PlatformFile& file,
    int64 bytes_processed,
    int64 bytes_total,
    int progress_offset,
    int progress_scale,
    const base::Callback<void(const std::string&)>& callback) {
  if (IsCancelled()) {
    base::ClosePlatformFile(file);
    return;
  }

  CHECK_LE(bytes_processed, bytes_total);

  scoped_ptr<char[]> buffer(new char[kMD5BufferSize]);
  int read_size = std::min(bytes_total - bytes_processed,
                           static_cast<int64>(kMD5BufferSize));

  if (read_size == 0) {
    // Nothing to read, we are done.
    base::MD5Digest digest;
    base::MD5Final(&digest, &md5_context_);
    callback.Run(base::MD5DigestToBase16(digest));
  } else {
    int len =
        base::ReadPlatformFile(file, bytes_processed, buffer.get(), read_size);

    if (len == read_size) {
      // Process data.
      base::MD5Update(&md5_context_, base::StringPiece(buffer.get(), len));
      int percent_curr =
          ((bytes_processed + len) * progress_scale) / bytes_total +
          progress_offset;
      SetProgress(percent_curr);

      BrowserThread::PostTask(BrowserThread::FILE,
                              FROM_HERE,
                              base::Bind(&Operation::MD5Chunk,
                                         this,
                                         file,
                                         bytes_processed + len,
                                         bytes_total,
                                         progress_offset,
                                         progress_scale,
                                         callback));
      // Skip closing the file.
      return;
    } else {
      // We didn't read the bytes we expected.
      Error(error::kHashReadError);
    }
  }
  base::ClosePlatformFile(file);
}

void Operation::OnUnzipSuccess(const base::Closure& continuation) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  SetProgress(kProgressComplete);
  BrowserThread::PostTask(BrowserThread::FILE, FROM_HERE, continuation);
}

void Operation::OnUnzipFailure() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  Error(error::kUnzipGenericError);
}

void Operation::OnUnzipProgress(int64 total_bytes, int64 progress_bytes) {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));

  int progress_percent = 100 * progress_bytes / total_bytes;
  SetProgress(progress_percent);
}

void Operation::CleanUp() {
  DCHECK(BrowserThread::CurrentlyOn(BrowserThread::FILE));
  for (std::vector<base::Closure>::iterator it = cleanup_functions_.begin();
       it != cleanup_functions_.end();
       ++it) {
    it->Run();
  }
  cleanup_functions_.clear();
}

}  // namespace image_writer
}  // namespace extensions

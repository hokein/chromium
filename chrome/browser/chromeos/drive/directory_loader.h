// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_DRIVE_DIRECTORY_LOADER_H_
#define CHROME_BROWSER_CHROMEOS_DRIVE_DIRECTORY_LOADER_H_

#include <map>
#include <set>
#include <string>
#include <vector>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "base/memory/scoped_ptr.h"
#include "base/memory/scoped_vector.h"
#include "base/observer_list.h"
#include "chrome/browser/chromeos/drive/file_errors.h"
#include "google_apis/drive/drive_common_callbacks.h"
#include "google_apis/drive/gdata_errorcode.h"

namespace base {
class SequencedTaskRunner;
}  // namespace base

namespace google_apis {
class AboutResource;
}  // namespace google_apis

namespace drive {

class DriveServiceInterface;
class EventLogger;
class JobScheduler;
class ResourceEntry;

namespace internal {

class AboutResourceLoader;
class ChangeList;
class ChangeListLoaderObserver;
class DirectoryFetchInfo;
class LoaderController;
class ResourceMetadata;

// DirectoryLoader is used to load directory contents.
class DirectoryLoader {
 public:
  DirectoryLoader(EventLogger* logger,
                  base::SequencedTaskRunner* blocking_task_runner,
                  ResourceMetadata* resource_metadata,
                  JobScheduler* scheduler,
                  DriveServiceInterface* drive_service,
                  AboutResourceLoader* about_resource_loader,
                  LoaderController* apply_task_controller);
  ~DirectoryLoader();

  // Adds and removes the observer.
  void AddObserver(ChangeListLoaderObserver* observer);
  void RemoveObserver(ChangeListLoaderObserver* observer);

  // Starts loading the directory contents if needed.
  // |callback| must not be null.
  void LoadDirectoryIfNeeded(const base::FilePath& directory_path,
                             const FileOperationCallback& callback);

 private:
  class FeedFetcher;

  // Part of LoadDirectoryIfNeeded().
  void LoadDirectoryIfNeededAfterGetEntry(const base::FilePath& directory_path,
                                          const FileOperationCallback& callback,
                                          bool should_try_loading_parent,
                                          const ResourceEntry* entry,
                                          FileError error);
  void LoadDirectoryIfNeededAfterLoadParent(
      const base::FilePath& directory_path,
      const FileOperationCallback& callback,
      FileError error);

  // Starts loading directory contents and calls |callback| when it's done.
  void Load(const DirectoryFetchInfo& directory_fetch_info,
            const FileOperationCallback& callback);
  void LoadAfterGetLargestChangestamp(
      const DirectoryFetchInfo& directory_fetch_info,
      int64 local_changestamp);
  void LoadAfterGetAboutResource(
      const DirectoryFetchInfo& directory_fetch_info,
      int64 local_changestamp,
      google_apis::GDataErrorCode status,
      scoped_ptr<google_apis::AboutResource> about_resource);

  // Part of Load().
  // This function should be called when the directory load is complete.
  // Flushes the callbacks waiting for the directory to be loaded.
  void OnDirectoryLoadComplete(const DirectoryFetchInfo& directory_fetch_info,
                               FileError error);

  // ================= Implementation for directory loading =================
  // Loads the directory contents from server, and updates the local metadata.
  // Runs |callback| when it is finished.
  void LoadDirectoryFromServer(const DirectoryFetchInfo& directory_fetch_info);

  // Part of LoadDirectoryFromServer() for a normal directory.
  void LoadDirectoryFromServerAfterLoad(
      const DirectoryFetchInfo& directory_fetch_info,
      FeedFetcher* fetcher,
      FileError error,
      ScopedVector<ChangeList> change_lists);

  // Part of LoadDirectoryFromServer().
  void LoadDirectoryFromServerAfterRefresh(
      const DirectoryFetchInfo& directory_fetch_info,
      const base::FilePath* directory_path,
      FileError error);

  EventLogger* logger_;  // Not owned.
  scoped_refptr<base::SequencedTaskRunner> blocking_task_runner_;
  ResourceMetadata* resource_metadata_;  // Not owned.
  JobScheduler* scheduler_;  // Not owned.
  DriveServiceInterface* drive_service_;  // Not owned.
  AboutResourceLoader* about_resource_loader_;  // Not owned.
  LoaderController* loader_controller_;  // Not owned.
  ObserverList<ChangeListLoaderObserver> observers_;
  typedef std::map<std::string, std::vector<FileOperationCallback> >
      LoadCallbackMap;
  LoadCallbackMap pending_load_callback_;

  // Set of the running feed fetcher for the fast fetch.
  std::set<FeedFetcher*> fast_fetch_feed_fetcher_set_;

  // Note: This should remain the last member so it'll be destroyed and
  // invalidate its weak pointers before any other members are destroyed.
  base::WeakPtrFactory<DirectoryLoader> weak_ptr_factory_;
  DISALLOW_COPY_AND_ASSIGN(DirectoryLoader);
};

}  // namespace internal
}  // namespace drive

#endif  // CHROME_BROWSER_CHROMEOS_DRIVE_DIRECTORY_LOADER_H_

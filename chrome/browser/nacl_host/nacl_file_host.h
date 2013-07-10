// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_NACL_HOST_NACL_FILE_HOST_H_
#define CHROME_BROWSER_NACL_HOST_NACL_FILE_HOST_H_

#include <string>

#include "base/callback.h"
#include "base/memory/ref_counted.h"
#include "ipc/ipc_platform_file.h"

class ExtensionInfoMap;
class GURL;
class NaClHostMessageFilter;

namespace base {
class FilePath;
}

namespace IPC {
class Message;
}

// Opens NaCl Files in the Browser process, on behalf of the NaCl plugin.

namespace nacl_file_host {
typedef base::Callback<void(IPC::PlatformFileForTransit)> TempFileCallback;

// Open a Pnacl file (readonly) on behalf of the NaCl plugin.
void GetReadonlyPnaclFd(
    scoped_refptr<NaClHostMessageFilter> nacl_host_message_filter,
    const std::string& filename,
    IPC::Message* reply_msg);

// Return true if the filename requested is valid for opening.
// Sets file_to_open to the base::FilePath which we will attempt to open.
bool PnaclCanOpenFile(const std::string& filename,
                      base::FilePath* file_to_open);

// Creates a temporary file that will be deleted when the last handle
// is closed, or earlier.
void CreateTemporaryFile(
    scoped_refptr<NaClHostMessageFilter> nacl_host_message_filter,
    TempFileCallback cb);

// Opens a NaCl executable file for reading and executing.
void OpenNaClExecutable(
    scoped_refptr<NaClHostMessageFilter> nacl_host_message_filter,
    scoped_refptr<ExtensionInfoMap> extension_info_map,
    int render_view_id,
    const GURL& file_url,
    IPC::Message* reply_msg);

}  // namespace nacl_file_host

#endif  // CHROME_BROWSER_NACL_HOST_NACL_FILE_HOST_H_

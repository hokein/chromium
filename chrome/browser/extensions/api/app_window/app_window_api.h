// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_EXTENSIONS_API_APP_WINDOW_APP_WINDOW_API_H_
#define CHROME_BROWSER_EXTENSIONS_API_APP_WINDOW_APP_WINDOW_API_H_

#include "apps/app_window.h"
#include "chrome/browser/extensions/chrome_extension_function.h"

namespace extensions {

namespace api {
namespace app_window {
struct CreateWindowOptions;
}
}

class AppWindowCreateFunction : public ChromeAsyncExtensionFunction {
 public:
  AppWindowCreateFunction();
  DECLARE_EXTENSION_FUNCTION("app.window.create", APP_WINDOW_CREATE)

  void SendDelayedResponse();

 protected:
  virtual ~AppWindowCreateFunction() {}
  virtual bool RunImpl() OVERRIDE;

 private:
  bool inject_html_titlebar_;

  apps::AppWindow::Frame GetFrameFromString(const std::string& frame_string);
  bool GetFrameOptions(
      const extensions::api::app_window::CreateWindowOptions& options,
      apps::AppWindow::CreateParams* create_params);
  void UpdateFrameOptionsForChannel(
      apps::AppWindow::CreateParams* create_params);
};

}  // namespace extensions

#endif  // CHROME_BROWSER_EXTENSIONS_API_APP_WINDOW_APP_WINDOW_API_H_

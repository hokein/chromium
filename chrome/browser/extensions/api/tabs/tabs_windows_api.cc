// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/tabs/tabs_windows_api.h"

#include "base/lazy_instance.h"
#include "chrome/browser/extensions/api/tabs/tabs_event_router.h"
#include "chrome/browser/extensions/api/tabs/windows_event_router.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/tabs.h"
#include "chrome/common/extensions/api/windows.h"
#include "extensions/browser/event_router.h"
#include "extensions/browser/extension_system.h"

namespace extensions {

TabsWindowsAPI::TabsWindowsAPI(content::BrowserContext* context)
    : browser_context_(context) {
  EventRouter* event_router =
      ExtensionSystem::Get(browser_context_)->event_router();

  // Tabs API Events.
  event_router->RegisterObserver(this, api::tabs::OnCreated::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnUpdated::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnMoved::kEventName);
  event_router->RegisterObserver(this,
                                 api::tabs::OnSelectionChanged::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnActiveChanged::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnActivated::kEventName);
  event_router->RegisterObserver(this,
                                 api::tabs::OnHighlightChanged::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnHighlighted::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnDetached::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnAttached::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnRemoved::kEventName);
  event_router->RegisterObserver(this, api::tabs::OnReplaced::kEventName);

  // Windows API Events.
  event_router->RegisterObserver(this, api::windows::OnCreated::kEventName);
  event_router->RegisterObserver(this, api::windows::OnRemoved::kEventName);
  event_router->RegisterObserver(this,
                                 api::windows::OnFocusChanged::kEventName);
}

TabsWindowsAPI::~TabsWindowsAPI() {
}

// static
TabsWindowsAPI* TabsWindowsAPI::Get(content::BrowserContext* context) {
  return ProfileKeyedAPIFactory<TabsWindowsAPI>::GetForProfile(context);
}

TabsEventRouter* TabsWindowsAPI::tabs_event_router() {
  if (!tabs_event_router_.get())
    tabs_event_router_.reset(
        new TabsEventRouter(Profile::FromBrowserContext(browser_context_)));
  return tabs_event_router_.get();
}

WindowsEventRouter* TabsWindowsAPI::windows_event_router() {
  if (!windows_event_router_)
    windows_event_router_.reset(
        new WindowsEventRouter(Profile::FromBrowserContext(browser_context_)));
  return windows_event_router_.get();
}

void TabsWindowsAPI::Shutdown() {
  ExtensionSystem::Get(browser_context_)->event_router()->UnregisterObserver(
      this);
}

static base::LazyInstance<ProfileKeyedAPIFactory<TabsWindowsAPI> >
g_factory = LAZY_INSTANCE_INITIALIZER;

ProfileKeyedAPIFactory<TabsWindowsAPI>* TabsWindowsAPI::GetFactoryInstance() {
  return g_factory.Pointer();
}

void TabsWindowsAPI::OnListenerAdded(const EventListenerInfo& details) {
  // Initialize the event routers.
  tabs_event_router();
  windows_event_router();
  ExtensionSystem::Get(browser_context_)->event_router()->UnregisterObserver(
      this);
}

}  // namespace extensions

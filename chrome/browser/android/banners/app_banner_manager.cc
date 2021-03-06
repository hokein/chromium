// Copyright 2014 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/banners/app_banner_manager.h"

#include "base/android/jni_android.h"
#include "base/android/jni_string.h"
#include "base/command_line.h"
#include "chrome/browser/android/banners/app_banner_settings_helper.h"
#include "chrome/browser/bitmap_fetcher.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_switches.h"
#include "content/public/browser/android/content_view_core.h"
#include "content/public/browser/navigation_details.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/frame_navigate_params.h"
#include "jni/AppBannerManager_jni.h"
#include "ui/gfx/android/java_bitmap.h"

using base::android::ConvertJavaStringToUTF8;
using base::android::ConvertUTF8ToJavaString;

namespace {
const char kBannerTag[] = "google-play-id";
}

AppBannerManager::AppBannerManager(JNIEnv* env, jobject obj)
    : MetaTagObserver(kBannerTag),
      weak_java_banner_view_manager_(env, obj) {}

AppBannerManager::~AppBannerManager() {
}

void AppBannerManager::Destroy(JNIEnv* env, jobject obj) {
  delete this;
}

void AppBannerManager::BlockBanner(JNIEnv* env,
                                   jobject obj,
                                   jstring jurl,
                                   jstring jpackage) {
  if (!web_contents())
    return;

  GURL url(ConvertJavaStringToUTF8(env, jurl));
  std::string package_name = ConvertJavaStringToUTF8(env, jpackage);
  AppBannerSettingsHelper::Block(web_contents(), url, package_name);
}

void AppBannerManager::ReplaceWebContents(JNIEnv* env,
                                          jobject obj,
                                          jobject jweb_contents) {
  content::WebContents* web_contents =
      content::WebContents::FromJavaWebContents(jweb_contents);
  Observe(web_contents);
}

void AppBannerManager::DidNavigateMainFrame(
    const content::LoadCommittedDetails& details,
    const content::FrameNavigateParams& params) {
  // Get rid of the current banner.
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = weak_java_banner_view_manager_.get(env);
  if (jobj.is_null())
    return;
  Java_AppBannerManager_dismissCurrentBanner(env, jobj.obj());
}

void AppBannerManager::OnFetchComplete(const GURL url, const SkBitmap* bitmap) {
  if (bitmap) {
    JNIEnv* env = base::android::AttachCurrentThread();

    ScopedJavaLocalRef<jobject> jobj = weak_java_banner_view_manager_.get(env);
    if (jobj.is_null())
      return;

    ScopedJavaLocalRef<jstring> jimage_url(
        ConvertUTF8ToJavaString(env, url.spec()));
    ScopedJavaLocalRef<jobject> jimage = gfx::ConvertToJavaBitmap(bitmap);
    Java_AppBannerManager_createBanner(env,
                                       jobj.obj(),
                                       jimage_url.obj(),
                                       jimage.obj());
  } else {
    DVLOG(1) << "Failed to retrieve image: " << url;
  }

  fetcher_.reset();
}

void AppBannerManager::HandleMetaTagContent(const std::string& tag_content,
                                            const GURL& expected_url) {
  DCHECK(web_contents());

  if (!AppBannerSettingsHelper::IsAllowed(web_contents(),
                                          expected_url,
                                          tag_content)) {
    return;
  }

  // Send the info to the Java side to get info about the app.
  JNIEnv* env = base::android::AttachCurrentThread();
  ScopedJavaLocalRef<jobject> jobj = weak_java_banner_view_manager_.get(env);
  if (jobj.is_null())
    return;

  ScopedJavaLocalRef<jstring> jurl(
      ConvertUTF8ToJavaString(env, expected_url.spec()));
  ScopedJavaLocalRef<jstring> jpackage(
      ConvertUTF8ToJavaString(env, tag_content));
  Java_AppBannerManager_prepareBanner(env,
                                      jobj.obj(),
                                      jurl.obj(),
                                      jpackage.obj());
}

bool AppBannerManager::FetchIcon(JNIEnv* env,
                                 jobject obj,
                                 jstring jimage_url) {
  std::string image_url = ConvertJavaStringToUTF8(env, jimage_url);
  if (!web_contents())
    return false;

  // Begin asynchronously fetching the app icon.
  Profile* profile =
      Profile::FromBrowserContext(web_contents()->GetBrowserContext());
  fetcher_.reset(new chrome::BitmapFetcher(GURL(image_url), this));
  fetcher_.get()->Start(profile);
  return true;
}

jlong Init(JNIEnv* env, jobject obj) {
  AppBannerManager* manager = new AppBannerManager(env, obj);
  return reinterpret_cast<intptr_t>(manager);
}

jboolean IsEnabled(JNIEnv* env, jclass clazz) {
  return !CommandLine::ForCurrentProcess()->HasSwitch(
      switches::kDisableAppBanners);
}

// Register native methods
bool RegisterAppBannerManager(JNIEnv* env) {
  return RegisterNativesImpl(env);
}

// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/arc_apps.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/arc_apps_factory.h"
#include "chrome/browser/apps/app_service/dip_px_util.h"
#include "chrome/browser/chromeos/arc/arc_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/app_list/arc/arc_app_dialog.h"
#include "chrome/browser/ui/app_list/arc/arc_app_icon.h"
#include "chrome/browser/ui/app_list/arc/arc_app_utils.h"
#include "chrome/grit/component_extension_resources.h"
#include "chrome/services/app_service/public/cpp/intent_filter_util.h"
#include "components/arc/app_permissions/arc_app_permissions_bridge.h"
#include "components/arc/arc_service_manager.h"
#include "components/arc/intent_helper/arc_intent_helper_bridge.h"
#include "components/arc/mojom/app_permissions.mojom.h"
#include "components/arc/session/arc_bridge_service.h"
#include "content/public/browser/system_connector.h"
#include "extensions/grit/extensions_browser_resources.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/size.h"

// TODO(crbug.com/826982): consider that, per khmel@, "App icon can be
// overwritten (setTaskDescription) or by assigning the icon for the app
// window. In this case some consumers (Shelf for example) switch to
// overwritten icon... IIRC this applies to shelf items and ArcAppWindow icon".

namespace {

void OnArcAppIconCompletelyLoaded(
    apps::mojom::IconCompression icon_compression,
    int32_t size_hint_in_dip,
    apps::IconEffects icon_effects,
    apps::mojom::Publisher::LoadIconCallback callback,
    ArcAppIcon* icon) {
  if (!icon) {
    std::move(callback).Run(apps::mojom::IconValue::New());
    return;
  }

  apps::mojom::IconValuePtr iv = apps::mojom::IconValue::New();
  iv->icon_compression = icon_compression;
  iv->is_placeholder_icon = false;

  if (icon_compression == apps::mojom::IconCompression::kUncompressed) {
    iv->uncompressed = icon->image_skia();
    if (icon_effects != apps::IconEffects::kNone) {
      apps::ApplyIconEffects(icon_effects, size_hint_in_dip, &iv->uncompressed);
    }
  } else {
    auto& compressed_images = icon->compressed_images();
    auto iter =
        compressed_images.find(apps_util::GetPrimaryDisplayUIScaleFactor());
    if (iter == compressed_images.end()) {
      std::move(callback).Run(apps::mojom::IconValue::New());
      return;
    }
    const std::string& data = iter->second;
    iv->compressed = std::vector<uint8_t>(data.begin(), data.end());
    if (icon_effects != apps::IconEffects::kNone) {
      // TODO(crbug.com/988321): decompress the image, apply icon effects then
      // re-compress.
    }
  }

  std::move(callback).Run(std::move(iv));
}

void UpdateAppPermissions(
    const base::flat_map<arc::mojom::AppPermission,
                         arc::mojom::PermissionStatePtr>& new_permissions,
    std::vector<apps::mojom::PermissionPtr>* permissions) {
  for (const auto& new_permission : new_permissions) {
    auto permission = apps::mojom::Permission::New();
    permission->permission_id = static_cast<uint32_t>(new_permission.first);
    permission->value_type = apps::mojom::PermissionValueType::kBool;
    permission->value = static_cast<uint32_t>(new_permission.second->granted);
    permission->is_managed = new_permission.second->managed;

    permissions->push_back(std::move(permission));
  }
}

}  // namespace

namespace apps {

// static
ArcApps* ArcApps::Get(Profile* profile) {
  return ArcAppsFactory::GetForProfile(profile);
}

// static
ArcApps* ArcApps::CreateForTesting(Profile* profile,
                                   apps::AppServiceProxy* proxy) {
  return new ArcApps(profile, proxy);
}

ArcApps::ArcApps(Profile* profile) : ArcApps(profile, nullptr) {}

ArcApps::ArcApps(Profile* profile, apps::AppServiceProxy* proxy)
    : profile_(profile), arc_icon_once_loader_(profile) {
  if (!arc::IsArcAllowedForProfile(profile_) ||
      (arc::ArcServiceManager::Get() == nullptr)) {
    return;
  }

  if (!proxy) {
    proxy = apps::AppServiceProxyFactory::GetForProfile(profile);
  }
  mojo::Remote<apps::mojom::AppService>& app_service = proxy->AppService();
  if (!app_service.is_bound()) {
    return;
  }

  // Make some observee-observer connections.
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }
  prefs->AddObserver(this);
  proxy->SetArcIsRegistered();

  auto* intent_helper_bridge =
      arc::ArcIntentHelperBridge::GetForBrowserContext(profile_);
  if (intent_helper_bridge) {
    arc_intent_helper_observer_.Add(intent_helper_bridge);
  }

  app_service->RegisterPublisher(receiver_.BindNewPipeAndPassRemote(),
                                 apps::mojom::AppType::kArc);
}

ArcApps::~ArcApps() = default;

void ArcApps::Shutdown() {
  // Disconnect the observee-observer connections that we made during the
  // constructor.
  //
  // This isn't entirely correct. The object returned by
  // ArcAppListPrefs::Get(some_profile) can vary over the lifetime of that
  // profile. If it changed, we'll try to disconnect from different
  // ArcAppListPrefs-related objects than the ones we connected to, at the time
  // of this object's construction.
  //
  // Even so, this is probably harmless, assuming that calling
  // foo->RemoveObserver(bar) is a no-op (and e.g. does not crash) if bar
  // wasn't observing foo in the first place, and assuming that the dangling
  // observee-observer connection on the old foo's are never followed again.
  //
  // To fix this properly, we would probably need to add something like an
  // OnArcAppListPrefsWillBeDestroyed method to ArcAppListPrefs::Observer, and
  // in this class's implementation of that method, disconnect. Furthermore,
  // when the new ArcAppListPrefs object is created, we'll have to somehow be
  // notified so we can re-connect this object as an observer.
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (prefs) {
    prefs->RemoveObserver(this);
  }
  arc_icon_once_loader_.StopObserving(prefs);

  arc_intent_helper_observer_.RemoveAll();
}

void ArcApps::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  std::vector<apps::mojom::AppPtr> apps;
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (prefs) {
    for (const auto& app_id : prefs->GetAppIds()) {
      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
          prefs->GetApp(app_id);
      if (app_info) {
        apps.push_back(Convert(prefs, app_id, *app_info));
      }
    }
  }
  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscriber->OnApps(std::move(apps));
  subscribers_.Add(std::move(subscriber));
}

void ArcApps::LoadIcon(const std::string& app_id,
                       apps::mojom::IconKeyPtr icon_key,
                       apps::mojom::IconCompression icon_compression,
                       int32_t size_hint_in_dip,
                       bool allow_placeholder_icon,
                       LoadIconCallback callback) {
  if (!icon_key) {
    std::move(callback).Run(apps::mojom::IconValue::New());
    return;
  }
  IconEffects icon_effects = static_cast<IconEffects>(icon_key->icon_effects);

  // Treat the Play Store as a special case, loading an icon defined by a
  // resource instead of asking the Android VM (or the cache of previous
  // responses from the Android VM). Presumably this is for bootstrapping:
  // the Play Store icon (the UI for enabling and installing Android apps)
  // should be showable even before the user has installed their first
  // Android app and before bringing up an Android VM for the first time.
  if (app_id == arc::kPlayStoreAppId) {
    LoadPlayStoreIcon(icon_compression, size_hint_in_dip, icon_effects,
                      std::move(callback));
  } else if (allow_placeholder_icon) {
    constexpr bool is_placeholder_icon = true;
    LoadIconFromResource(icon_compression, size_hint_in_dip,
                         IDR_APP_DEFAULT_ICON, is_placeholder_icon,
                         icon_effects, std::move(callback));
  } else {
    arc_icon_once_loader_.LoadIcon(
        app_id, size_hint_in_dip, icon_compression,
        base::BindOnce(&OnArcAppIconCompletelyLoaded, icon_compression,
                       size_hint_in_dip, icon_effects, std::move(callback)));
  }
}

void ArcApps::Launch(const std::string& app_id,
                     int32_t event_flags,
                     apps::mojom::LaunchSource launch_source,
                     int64_t display_id) {
  auto uit = arc::UserInteractionType::NOT_USER_INITIATED;
  switch (launch_source) {
    case apps::mojom::LaunchSource::kUnknown:
      return;
    case apps::mojom::LaunchSource::kFromAppListGrid:
      uit = arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER;
      break;
    case apps::mojom::LaunchSource::kFromAppListGridContextMenu:
      uit = arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER_CONTEXT_MENU;
      break;
    case apps::mojom::LaunchSource::kFromAppListQuery:
      uit = arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER_SEARCH;
      break;
    case apps::mojom::LaunchSource::kFromAppListQueryContextMenu:
      uit = arc::UserInteractionType::
          APP_STARTED_FROM_LAUNCHER_SEARCH_CONTEXT_MENU;
      break;
    case apps::mojom::LaunchSource::kFromAppListRecommendation:
      uit = arc::UserInteractionType::APP_STARTED_FROM_LAUNCHER_SUGGESTED_APP;
      break;
    case apps::mojom::LaunchSource::kFromParentalControls:
      uit = arc::UserInteractionType::APP_STARTED_FROM_SETTINGS;
      break;
    case apps::mojom::LaunchSource::kFromShelf:
      uit = arc::UserInteractionType::APP_STARTED_FROM_SHELF;
      break;
    case apps::mojom::LaunchSource::kFromFileManager:
      uit = arc::UserInteractionType::APP_STARTED_FROM_FILE_MANAGER;
      break;
  }

  arc::LaunchApp(profile_, app_id, event_flags, uit, display_id);
}

void ArcApps::SetPermission(const std::string& app_id,
                            apps::mojom::PermissionPtr permission) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }
  const std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs->GetApp(app_id);
  if (!app_info) {
    LOG(ERROR) << "SetPermission failed, could not find app with id " << app_id;
    return;
  }

  auto* arc_service_manager = arc::ArcServiceManager::Get();
  if (!arc_service_manager) {
    LOG(WARNING) << "SetPermission failed, ArcServiceManager not available.";
    return;
  }

  auto permission_type =
      static_cast<arc::mojom::AppPermission>(permission->permission_id);
  if (permission->value) {
    auto* permissions_instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->app_permissions(),
        GrantPermission);
    if (permissions_instance) {
      permissions_instance->GrantPermission(app_info->package_name,
                                            permission_type);
    }
  } else {
    auto* permissions_instance = ARC_GET_INSTANCE_FOR_METHOD(
        arc_service_manager->arc_bridge_service()->app_permissions(),
        RevokePermission);
    if (permissions_instance) {
      permissions_instance->RevokePermission(app_info->package_name,
                                             permission_type);
    }
  }
}

void ArcApps::Uninstall(const std::string& app_id) {
  if (!profile_) {
    return;
  }
  arc::ShowArcAppUninstallDialog(profile_, nullptr /* controller */, app_id);
}

void ArcApps::OpenNativeSettings(const std::string& app_id) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }
  const std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
      prefs->GetApp(app_id);
  if (!app_info) {
    LOG(ERROR) << "Cannot open native settings for " << app_id
               << ". App is not found.";
    return;
  }
  arc::ShowPackageInfo(app_info->package_name,
                       arc::mojom::ShowPackageInfoPage::MAIN,
                       display::Screen::GetScreen()->GetPrimaryDisplay().id());
}

void ArcApps::OnAppRegistered(const std::string& app_id,
                              const ArcAppListPrefs::AppInfo& app_info) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (prefs) {
    Publish(Convert(prefs, app_id, app_info));
  }
}

void ArcApps::OnAppStatesChanged(const std::string& app_id,
                                 const ArcAppListPrefs::AppInfo& app_info) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (prefs) {
    Publish(Convert(prefs, app_id, app_info));
  }
}

void ArcApps::OnAppRemoved(const std::string& app_id) {
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kArc;
  app->app_id = app_id;
  app->readiness = apps::mojom::Readiness::kUninstalledByUser;
  Publish(std::move(app));
}

void ArcApps::OnAppIconUpdated(const std::string& app_id,
                               const ArcAppIconDescriptor& descriptor) {
  static constexpr uint32_t icon_effects = 0;
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kArc;
  app->app_id = app_id;
  app->icon_key = icon_key_factory_.MakeIconKey(icon_effects);
  Publish(std::move(app));
}

void ArcApps::OnAppNameUpdated(const std::string& app_id,
                               const std::string& name) {
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kArc;
  app->app_id = app_id;
  app->name = name;
  Publish(std::move(app));
}

void ArcApps::OnAppLastLaunchTimeUpdated(const std::string& app_id) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }
  std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
  if (!app_info) {
    return;
  }
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_type = apps::mojom::AppType::kArc;
  app->app_id = app_id;
  app->last_launch_time = app_info->last_launch_time;
  Publish(std::move(app));
}

void ArcApps::OnPackageInstalled(
    const arc::mojom::ArcPackageInfo& package_info) {
  ConvertAndPublishPackageApps(package_info);
}

void ArcApps::OnPackageModified(
    const arc::mojom::ArcPackageInfo& package_info) {
  static constexpr bool update_icon = false;
  ConvertAndPublishPackageApps(package_info, update_icon);
}

void ArcApps::OnPackageListInitialRefreshed() {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }
  for (const auto& app_id : prefs->GetAppIds()) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
    if (app_info) {
      Publish(Convert(prefs, app_id, *app_info));
    }
  }
}

void ArcApps::OnIntentFiltersUpdated(
    const base::Optional<std::string>& package_name) {
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (!prefs) {
    return;
  }

  auto GetAppInfoAndPublish = [prefs, this](std::string app_id) {
    std::unique_ptr<ArcAppListPrefs::AppInfo> app_info = prefs->GetApp(app_id);
    if (app_info) {
      Publish(Convert(prefs, app_id, *app_info));
    }
  };

  // If there is no specific package_name, update all apps, otherwise update
  // apps for the package.

  // Note: Cannot combine the two for-loops because the return type of
  // GetAppIds() is std::vector<std::string> and the return type of
  // GetAppsForPackage() is std::unordered_set<std::string>.
  if (package_name == base::nullopt) {
    for (const auto& app_id : prefs->GetAppIds()) {
      GetAppInfoAndPublish(app_id);
    }
  } else {
    for (const auto& app_id : prefs->GetAppsForPackage(package_name.value())) {
      GetAppInfoAndPublish(app_id);
    }
  }
}

void ArcApps::LoadPlayStoreIcon(apps::mojom::IconCompression icon_compression,
                                int32_t size_hint_in_dip,
                                IconEffects icon_effects,
                                LoadIconCallback callback) {
  // Use overloaded Chrome icon for Play Store that is adapted to Chrome style.
  constexpr bool quantize_to_supported_scale_factor = true;
  int size_hint_in_px = apps_util::ConvertDipToPx(
      size_hint_in_dip, quantize_to_supported_scale_factor);
  int resource_id = (size_hint_in_px <= 32) ? IDR_ARC_SUPPORT_ICON_32
                                            : IDR_ARC_SUPPORT_ICON_192;
  constexpr bool is_placeholder_icon = false;
  LoadIconFromResource(icon_compression, size_hint_in_dip, resource_id,
                       is_placeholder_icon, icon_effects, std::move(callback));
}

apps::mojom::InstallSource GetInstallSource(const ArcAppListPrefs* prefs,
                                            const std::string& package_name) {
  // TODO(crbug.com/1010821): Create a generic check for kSystem apps.
  if (prefs->GetAppIdByPackageName(package_name) == arc::kPlayStoreAppId) {
    return apps::mojom::InstallSource::kSystem;
  }

  if (prefs->IsDefault(package_name)) {
    return apps::mojom::InstallSource::kDefault;
  }

  if (prefs->IsOem(package_name)) {
    return apps::mojom::InstallSource::kOem;
  }

  if (prefs->IsControlledByPolicy(package_name)) {
    return apps::mojom::InstallSource::kPolicy;
  }

  return apps::mojom::InstallSource::kUser;
}

apps::mojom::AppPtr ArcApps::Convert(ArcAppListPrefs* prefs,
                                     const std::string& app_id,
                                     const ArcAppListPrefs::AppInfo& app_info,
                                     bool update_icon) {
  apps::mojom::AppPtr app = apps::mojom::App::New();

  app->app_type = apps::mojom::AppType::kArc;
  app->app_id = app_id;
  app->readiness = app_info.suspended
                       ? apps::mojom::Readiness::kDisabledByPolicy
                       : apps::mojom::Readiness::kReady;
  app->name = app_info.name;
  app->short_name = app->name;

  if (update_icon) {
    IconEffects icon_effects = IconEffects::kNone;
    if (app_info.suspended) {
      icon_effects =
          static_cast<IconEffects>(icon_effects | IconEffects::kGray);
    }
    app->icon_key = icon_key_factory_.MakeIconKey(icon_effects);
  }

  app->last_launch_time = app_info.last_launch_time;
  app->install_time = app_info.install_time;

  app->install_source = GetInstallSource(prefs, app_info.package_name);

  app->is_platform_app = apps::mojom::OptionalBool::kFalse;
  app->recommendable = apps::mojom::OptionalBool::kTrue;
  app->searchable = apps::mojom::OptionalBool::kTrue;

  auto show = app_info.show_in_launcher ? apps::mojom::OptionalBool::kTrue
                                        : apps::mojom::OptionalBool::kFalse;
  app->show_in_launcher = show;
  app->show_in_search = show;
  app->show_in_management = show;

  std::unique_ptr<ArcAppListPrefs::PackageInfo> package =
      prefs->GetPackage(app_info.package_name);
  if (package) {
    UpdateAppPermissions(package->permissions, &app->permissions);
  }

  auto* intent_helper_bridge =
      arc::ArcIntentHelperBridge::GetForBrowserContext(profile_);
  if (intent_helper_bridge) {
    UpdateAppIntentFilters(app_info.package_name, intent_helper_bridge,
                           &app->intent_filters);
  }

  return app;
}

void ArcApps::Publish(apps::mojom::AppPtr app) {
  for (auto& subscriber : subscribers_) {
    std::vector<apps::mojom::AppPtr> apps;
    apps.push_back(app.Clone());
    subscriber->OnApps(std::move(apps));
  }
}

void ArcApps::ConvertAndPublishPackageApps(
    const arc::mojom::ArcPackageInfo& package_info,
    bool update_icon) {
  if (!package_info.permissions.has_value()) {
    return;
  }
  ArcAppListPrefs* prefs = ArcAppListPrefs::Get(profile_);
  if (prefs) {
    for (const auto& app_id :
         prefs->GetAppsForPackage(package_info.package_name)) {
      std::unique_ptr<ArcAppListPrefs::AppInfo> app_info =
          prefs->GetApp(app_id);
      if (app_info) {
        Publish(Convert(prefs, app_id, *app_info, update_icon));
      }
    }
  }
}

void ArcApps::UpdateAppIntentFilters(
    std::string package_name,
    arc::ArcIntentHelperBridge* intent_helper_bridge,
    std::vector<apps::mojom::IntentFilterPtr>* intent_filters) {
  const std::vector<arc::IntentFilter>& arc_intent_filters =
      intent_helper_bridge->GetIntentFilterForPackage(package_name);
  for (auto& arc_intent_filter : arc_intent_filters) {
    auto intent_filter = apps::mojom::IntentFilter::New();

    std::vector<apps::mojom::ConditionValuePtr> scheme_condition_values;
    for (auto& scheme : arc_intent_filter.schemes()) {
      scheme_condition_values.push_back(apps_util::MakeConditionValue(
          scheme, apps::mojom::PatternMatchType::kNone));
    }
    if (!scheme_condition_values.empty()) {
      auto scheme_condition =
          apps_util::MakeCondition(apps::mojom::ConditionType::kScheme,
                                   std::move(scheme_condition_values));
      intent_filter->conditions.push_back(std::move(scheme_condition));
    }

    std::vector<apps::mojom::ConditionValuePtr> host_condition_values;
    for (auto& authority : arc_intent_filter.authorities()) {
      host_condition_values.push_back(apps_util::MakeConditionValue(
          authority.host(), apps::mojom::PatternMatchType::kNone));
    }
    if (!host_condition_values.empty()) {
      auto host_condition = apps_util::MakeCondition(
          apps::mojom::ConditionType::kHost, std::move(host_condition_values));
      intent_filter->conditions.push_back(std::move(host_condition));
    }

    std::vector<apps::mojom::ConditionValuePtr> path_condition_values;
    for (auto& path : arc_intent_filter.paths()) {
      apps::mojom::PatternMatchType match_type;
      switch (path.match_type()) {
        case arc::mojom::PatternType::PATTERN_LITERAL:
          match_type = apps::mojom::PatternMatchType::kLiteral;
          break;
        case arc::mojom::PatternType::PATTERN_PREFIX:
          match_type = apps::mojom::PatternMatchType::kPrefix;
          break;
        case arc::mojom::PatternType::PATTERN_SIMPLE_GLOB:
          match_type = apps::mojom::PatternMatchType::kGlob;
          break;
        default:
          NOTREACHED();
      }
      path_condition_values.push_back(
          apps_util::MakeConditionValue(path.pattern(), match_type));
    }
    if (!path_condition_values.empty()) {
      auto path_condition =
          apps_util::MakeCondition(apps::mojom::ConditionType::kPattern,
                                   std::move(path_condition_values));
      intent_filter->conditions.push_back(std::move(path_condition));
    }

    intent_filters->push_back(std::move(intent_filter));
  }
}

}  // namespace apps

// darknessrecomp - ReXGlue Recompiled Project
//
// Customize your app by overriding virtual hooks from rex::ReXApp.

#pragma once

#include <filesystem>

#include <rex/rex_app.h>

#include "vr_bridge.h"

class DarknessrecompApp : public rex::ReXApp {
 public:
  using rex::ReXApp::ReXApp;

  static std::unique_ptr<rex::ui::WindowedApp> Create(
      rex::ui::WindowedAppContext& ctx) {
    return std::unique_ptr<DarknessrecompApp>(new DarknessrecompApp(ctx, "darknessrecomp",
        PPCImageConfig));
  }

  // Select the Xenos GPU backend. Without this there is no renderer at all.
  // The input factory adds the MCP Game Bridge pad beside the SDK's own drivers (vr_bridge.cpp).
  void OnPreSetup(rex::RuntimeConfig& config) override {
    config.gpu_plugin = "xenos";
    config.input_factory = REX_INPUT_BACKEND(DkCreateInputSystem);
  }

  // MCP Game Bridge³: the game's log into the bridge, then the pipe once the runtime exists.
  void OnPostInitLogging() override { DkBridgeLogging(); }
  void OnPostSetup() override { DkBridgeStart(runtime()); }
  void OnShutdown() override { DkBridgeStop(); }

  // Default the game data root to an "Assets" folder beside the executable, so the build
  // can be launched by double-clicking instead of needing --game_data_root on the command
  // line. A freshly generated project leaves this hook commented out, which is why a new
  // port's first run fails with "--game_data_root was not provided".
  //
  // Both hooks follow the pattern used by Condemned2Recomp
  // (https://github.com/psxrestore/Condemned2Recomp, BSD 3-Clause) - credit to psxrestore.
  void OnConfigurePaths(rex::PathConfig& paths) override {
    if (paths.game_data_root.empty()) {
      const auto assets_dir = paths.config_path.parent_path() / "Assets";
      if (std::filesystem::is_regular_file(assets_dir / "default.xex")) {
        paths.game_data_root = assets_dir;
      }
    }
  }

  // Override virtual hooks for customization:
  // void OnLoadXexImage(std::string& xex_image) override {}
  // void OnPostLoadXexImage() override {}
  // void OnCreateDialogs(rex::ui::ImGuiDrawer* drawer) override {}
  // std::unique_ptr<rex::ui::ImGuiDialog> CreateAchievementsOverlay() override;
  // std::unique_ptr<rex::ui::AchievementNotificationDialog>
  // CreateAchievementNotificationDialog() override;

};

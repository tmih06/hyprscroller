#include <hyprland/src/plugins/PluginAPI.hpp>
#include <hyprland/src/version.h>
#include "scroller.h"

HANDLE PHANDLE = nullptr;

APICALL EXPORT std::string PLUGIN_API_VERSION() {
    return HYPRLAND_API_VERSION;
}

APICALL EXPORT PLUGIN_DESCRIPTION_INFO PLUGIN_INIT(HANDLE handle) {
    PHANDLE = handle;

    const std::string CLIENT_HASH = __hyprland_api_get_client_hash();
    const std::string SERVER_HASH = __hyprland_api_get_hash();

    if (CLIENT_HASH != SERVER_HASH) {
        HyprlandAPI::addNotification(PHANDLE, "[hyprscroller] Failure in initialization: Version mismatch",
                                     CHyprColor{1.0, 0.2, 0.2, 1.0}, 5000);
        throw std::runtime_error("[hyprscroller] Version mismatch: client=" + CLIENT_HASH + " server=" + SERVER_HASH);
    }

    // Register custom scroller tiled algorithm with dynamic resizing
    HyprlandAPI::addTiledAlgo(PHANDLE, "scroller", &typeid(ScrollerLayout), []() -> UP<Layout::ITiledAlgorithm> {
        return makeUnique<ScrollerLayout>();
    });

    HyprlandAPI::addNotification(PHANDLE, "[hyprscroller] Scroller layout loaded",
                                 CHyprColor{0.2, 1.0, 0.2, 1.0}, 3000);

    return {
        .name = "hyprscroller",
        .description = "Dynamic scroller layout for Hyprland",
        .author = "tmih06",
        .version = "2.0"
    };
}

APICALL EXPORT void PLUGIN_EXIT() {
    HyprlandAPI::removeAlgo(PHANDLE, "scroller");
}

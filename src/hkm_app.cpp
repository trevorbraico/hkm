#include "hkm_app.hpp"
#include "hkm_layer.hpp"
#include "loggable.hpp"
#include "inputs.hpp"
#include "wlr-layer-shell-unstable-v1.h"
#include <wayland-client.h>
#include <unordered_map>
#include <functional>
#include <string>

void registryRemove(void *data, wl_registry *registry, uint32_t name) { /* noop */ }

void registryGlobal(void *data, wl_registry *registry, uint32_t name, const char *interface, uint32_t version)
{
    HkmApp *app = (HkmApp *) data;

    const static std::unordered_map<std::string, std::function<void()>>
        handlers{

            { wl_compositor_interface.name, [&] {
                app->compositor_ = (wl_compositor *) wl_registry_bind(registry, name, &wl_compositor_interface, 6);
            } },

            { wl_shm_interface.name, [&] {
                auto *ptr = (wl_shm *) wl_registry_bind(registry, name, &wl_shm_interface, 1);
                app->setCachedShm(ptr);
            } },

            { wl_seat_interface.name, [&] {
                if (app->inputs->hasSeat())
                    app->log("reinitializing inputs_ since we binded to another wl_seat instance");

                auto *seat = (wl_seat *) wl_registry_bind(registry, name, &wl_seat_interface, 9); 
                app->inputs->setSeat(seat);
            } },

            { wp_cursor_shape_manager_v1_interface.name, [&] {
                if (!app->inputs)
                    app->log("ignoring wp_cursor_shape_manager event since inputs_ has not been initialized yet");
                
                auto *ptr = (CursorShapeManager *) wl_registry_bind(registry, name, &wp_cursor_shape_manager_v1_interface, 1);
                app->inputs->setCursorShapeManager(ptr);
            } },

            { wl_output_interface.name, [&] {
                auto *ptr = (wl_output *) wl_registry_bind(registry, name, &wl_output_interface, 3);
                app->addWaitingOutput(ptr);
            } },

            { zxdg_output_manager_v1_interface.name, [&] {
                auto *ptr = (XdgOutputManager *) wl_registry_bind(registry, name, &zxdg_output_manager_v1_interface, 3);
                /* wait until we know that we have satisfied all dependencies 
                 * for HkmLayer creation (in HkmApp::run) before
                 * attempting construction
                 */
                app->setCachedXdgOutputManager(ptr);
            } },

            { zwlr_layer_shell_v1_interface.name, [&] {
                auto *ptr = (zwlr_layer_shell_v1 *) wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 5);
                /* wait until we know that we have satisfied all dependencies 
                 * for HkmLayer creation (in HkmApp::run) before
                 * attempting construction
                 */
                app->setCachedLayerShell(ptr);
            } },

            { zwlr_virtual_pointer_manager_v1_interface.name, [&] {
                auto *ptr = (VirtualPointerManager *) wl_registry_bind(registry, name, &zwlr_virtual_pointer_manager_v1_interface, 2);
                app->inputs->setVirtualPointerManager(ptr);
            } },
    };

    auto iter = handlers.find(interface);

    if (iter == handlers.end())
        return;
    else
        iter->second();
}

void HkmApp::addWaitingOutput(wl_output *output)
{
    waitingOutputs_.push_back(output);
}

void HkmApp::setCachedLayerShell(LayerShell *layerShell)
{
    layerDependencies_.layerShell = std::shared_ptr<LayerShell>(layerShell, zwlr_layer_shell_v1_destroy);
}

void HkmApp::setCachedXdgOutputManager(XdgOutputManager *outputManager)
{
    layerDependencies_.outputManager = std::shared_ptr<XdgOutputManager>(outputManager, zxdg_output_manager_v1_destroy);
}

void HkmApp::setCachedShm(wl_shm *shm)
{
    layerDependencies_.shm = std::shared_ptr<wl_shm>(shm, wl_shm_destroy);
}

HkmApp::HkmApp()
    : Loggable{ "HkmApp" }
{
    display_ = wl_display_connect(nullptr);
    inputs = std::make_unique<Inputs>();

    registry_ = wl_display_get_registry(display_);
    registryListener_ = wl_registry_listener {
        .global = registryGlobal,
        .global_remove = registryRemove,
    };

    wl_registry_add_listener(registry_, &registryListener_, this);
    wl_display_roundtrip(display_);
}

void HkmApp::run()
{
    for (auto *output : waitingOutputs_)
    {
        auto layer = std::make_unique<HkmLayer>(
            layerDependencies_.shm,
            output,
            wl_compositor_create_surface(compositor_),
            layerDependencies_.layerShell,
            layerDependencies_.outputManager
        );
        layers.push_back(std::move(layer));
    }

    wl_display_roundtrip(display_);

    for (auto &layer : layers)
        layer->sendSize();

    while (wl_display_dispatch(display_)) {
        // event loop
    }

    wl_display_disconnect(display_);
}

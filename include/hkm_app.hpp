#pragma once

#include "hkm_layer.hpp"
#include "inputs.hpp"
#include <wayland-client.h>
#include <memory>
#include <vector>

class HkmApp : public Loggable
{
public:
    HkmApp();
    
    ~HkmApp()
    {
    }

    void run();

    void addWaitingOutput(wl_output *output);
    void setCachedLayerShell(LayerShell *layerShell);
    void setCachedXdgOutputManager(XdgOutputManager *outputManager);
    void setCachedShm(wl_shm* shm);

    std::vector<std::unique_ptr<HkmLayer>> layers{};

    std::unique_ptr<Inputs> inputs{};

    wl_display *display_{ nullptr };
    wl_compositor *compositor_{ nullptr };
    std::unique_ptr<wl_registry. registry_{ nullptr , [](wl_registry * reg) { wl_registry_destroy(reg); } };
    wl_registry_listener registryListener_;

private:

    /* contains all of the objects that are being cached
     * while we wait for all of them to be satisfied
     * (by the global registry events)
     */
    struct LayerDeps {

        std::shared_ptr<LayerShell> layerShell{ nullptr };
        std::shared_ptr<XdgOutputManager> outputManager{ nullptr };
        std::shared_ptr<wl_shm> shm{ nullptr };

    } layerDependencies_;

    /* these are the outputs that we received from the global registry
     * that are waiting for the wl_compositor and cachedLayerShell_
     * to be valid so that they can be used to create a HkmLayer
     */
    std::vector<wl_output *> waitingOutputs_{};
};

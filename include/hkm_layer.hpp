#pragma once

#include "color.hpp"
#include "loggable.hpp"
#include "shm_buffer.hpp"
#include "wlr-layer-shell-unstable-v1.h"
#include "xdg-output-unstable-v1.h"
#include <wayland-client.h>
#include <functional>
#include <memory>

using XdgOutputManager = zxdg_output_manager_v1;
using XdgOutput = zxdg_output_v1;
using LayerShell = zwlr_layer_shell_v1;
using LayerSurface = zwlr_layer_surface_v1;

template <typename T>
using DeletingUniquePtr = std::unique_ptr<T, void(*)(T*)>;

/* No template for shared_ptr because
 * it doesn't need the template parameter
 * for the function pointer type of the
 * passed destructor
*/

/* this class allows us to use the shared_ptr
 * API for managing allocation of objects that
 * we get from reistry events. We can use std::make_shared
 * to construct Pointer objects and use the semantics of
 * shared_ptr to manage the resource
 */
template <typename T>
class Pointer
{
public:

    Pointer(T* ptr, std::function<void(T*)> destroy)
        : ptr_{ ptr }
        , destroy_{ destroy }
    { }

    ~Pointer() { destroy_(ptr_); };

    operator T*() const 
    {
        return ptr_;
    }

    T* get() { return ptr_; }

private:
    T *ptr_{ nullptr };
    std::function<void()> destroy_;
};

/*
 * Implements the surface interfacing with Wayland compositor.
 * Each HkmLayer is associated with a corresponding wl_output object,
 * (i.e. a monitor). We implement the compositor-wide layering functionality
 * by activating the HkmLayer for each monitor on-demand.
 */
class HkmLayer : public Loggable
{
  public:
    HkmLayer(
        std::shared_ptr<wl_shm> shm,

        wl_output *output,
        wl_surface *surface,

        std::shared_ptr<LayerShell> layerShell,
        std::shared_ptr<XdgOutputManager> outputManager
    );

    ~HkmLayer();

    Color layerColor_{ 0x00000000 };
    std::unique_ptr<ShmBuffer> shmBuffer_{ nullptr };
    
    // holds the info received from XdgOutput events
    struct {
        unsigned int width{ 0 };
        unsigned int height{ 0 };
        unsigned int xPosition{ 0 };
        unsigned int yPosition{ 0 };
        std::string_view name{ "" };
    } logicalMonitorInfo;

    void setXdgOutputManager(XdgOutputManager *outputManager);
    void setShmBuffer(ShmBuffer* shmBuffer);

    void sendSize() const;
    void commitWaylandSurface() const;

    void attachShmBuffer();
    void renderFrame();

  private:
    DeletingUniquePtr<wl_output> waylandOutput_{ nullptr, wl_output_destroy };
    DeletingUniquePtr<wl_surface> waylandSurface_{ nullptr, wl_surface_destroy };

    std::shared_ptr<LayerShell> layerShell_{ nullptr };
    DeletingUniquePtr<LayerSurface> layerSurface_{ nullptr, zwlr_layer_surface_v1_destroy };

    std::shared_ptr<XdgOutputManager> outputManager_{ nullptr };
    DeletingUniquePtr<XdgOutput> xdgOutput_{ nullptr, zxdg_output_v1_destroy };

    // not going to use RAII for this one because we should
    // be handling allocation/destruction manually in event handlers
    wl_callback *waylandCallback_{ nullptr };

    // event listeners
    zwlr_layer_surface_v1_listener layerSurfaceListener_;
    wl_callback_listener callbackListener_;
    zxdg_output_v1_listener outputListener_;
};

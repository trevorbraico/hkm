#include "hkm_layer.hpp"
#include "wlr-layer-shell-unstable-v1.h"
#include "xdg-output-unstable-v1.h"
#include <wayland-client-protocol.h>
#include <wayland-client.h>

void xdgOutputLogicalPosition(void *data, zxdg_output_v1 *output, int32_t x,
                              int32_t y)
{
    HkmLayer *layer = (HkmLayer *)data;
    layer->logicalMonitorInfo.xPosition = x;
    layer->logicalMonitorInfo.yPosition = y;
}

void xdgOutputLogicalSize(void *data, zxdg_output_v1 *output, int32_t width,
                          int32_t height)
{
    HkmLayer *layer = (HkmLayer *)data;
    layer->logicalMonitorInfo.width = width;
    layer->logicalMonitorInfo.height = height;
}

void xdgOutputDone(void *data, zxdg_output_v1 *output) { /* noop */ }
void xdgOutputName(void *data, zxdg_output_v1 *output, const char *name) { /* noop */ }

void xdgOutputDescription(void *data, zxdg_output_v1 *output,
                          const char *description)
{
    HkmLayer *layer = (HkmLayer *)data;
    layer->log("Registered XdgOutput for {}", description);
}

void layerSurfaceConfigure(void *data, zwlr_layer_surface_v1 *layer_surface,
                           uint32_t serial, uint32_t width, uint32_t height)
{
    HkmLayer *layer = (HkmLayer *)data;

    layer->shmBuffer_->setWidth(width);
    layer->shmBuffer_->setHeight(height);

    zwlr_layer_surface_v1_ack_configure(layer_surface, serial);

    layer->shmBuffer_->nextFrame();

    // layer->shmBuffer->attachToSurface(layer->waylandSurface_);
    layer->attachShmBuffer();
}

void layerSurfaceClose(void *data, zwlr_layer_surface_v1 *layer_surface)
{ /* noop */ }

void surfaceFrameDone(void *data, wl_callback *callback, uint32_t time)
{
    HkmLayer *layer = (HkmLayer *)data;

    wl_callback_destroy(callback);
    layer->renderFrame();
}

HkmLayer::HkmLayer(std::shared_ptr<wl_shm> shm,
                               wl_output *output,
                               wl_surface *surface,
                               std::shared_ptr<LayerShell> layerShell,
                               std::shared_ptr<XdgOutputManager> outputManager)
    : Loggable{ "HkmLayer" },
      shmBuffer_{ std::make_unique<ShmBuffer>(shm) },
      waylandOutput_{ DeletingUniquePtr<wl_output>{ output, wl_output_destroy } },
      waylandSurface_{ DeletingUniquePtr<wl_surface>{ surface, wl_surface_destroy  } },
      layerShell_{ layerShell },
      outputManager_{ outputManager }
{
    if (!waylandOutput_)
        throw std::runtime_error(
            "Cannot initialize layerSurface_ without a valid wl_output");
    if (!waylandSurface_)
        throw std::runtime_error(
            "Cannot initialize layerSurface_ without a valid wl_surface");
    if (!layerShell_)
        throw std::runtime_error(
            "Cannot initialize layerSurface_ without a valid LayerShell");
    if (!outputManager_)
        throw std::runtime_error(
            "Cannot initialize layerSurface_ without a valid XdgOutputManager");

    layerSurface_.reset(zwlr_layer_shell_v1_get_layer_surface(
        layerShell_.get(),
        waylandSurface_.get(),
        waylandOutput_.get(),
        ZWLR_LAYER_SHELL_V1_LAYER_OVERLAY,
        "Hkm"
    ));

    zwlr_layer_surface_v1_set_anchor(layerSurface_.get(),
                                     ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP
                                     | ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT
                                     | ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM
                                     | ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT);

    zwlr_layer_surface_v1_set_keyboard_interactivity(
        layerSurface_.get(),
        ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_ON_DEMAND
    );

    // What does this line even do?
    zwlr_layer_surface_v1_set_exclusive_zone(layerSurface_.get(), -1);

    layerSurfaceListener_ = {
        .configure = layerSurfaceConfigure,
        .closed = layerSurfaceClose,
    };

    zwlr_layer_surface_v1_add_listener(layerSurface_.get(), &layerSurfaceListener_, this);

    outputListener_ = zxdg_output_v1_listener{
        .logical_position = xdgOutputLogicalPosition,
        .logical_size = xdgOutputLogicalSize,
        .done = xdgOutputDone,
        .name = xdgOutputName,
        .description = xdgOutputDescription,
    };
    xdgOutput_.reset(zxdg_output_manager_v1_get_xdg_output(outputManager_.get(), waylandOutput_.get()));
    zxdg_output_v1_add_listener(xdgOutput_.get(), &outputListener_, this);
}

HkmLayer::~HkmLayer()
{
    if (waylandCallback_)
        wl_callback_destroy(waylandCallback_);
}

void HkmLayer::sendSize() const
{
    zwlr_layer_surface_v1_set_size(
        layerSurface_.get(),
        logicalMonitorInfo.width,
        logicalMonitorInfo.height
    );
    commitWaylandSurface();
}

void HkmLayer::commitWaylandSurface() const
{
    wl_surface_commit(waylandSurface_.get());
}

void HkmLayer::attachShmBuffer()
{
    callbackListener_ = wl_callback_listener{ .done = surfaceFrameDone };

    waylandCallback_ = wl_surface_frame(waylandSurface_.get());

    wl_callback_add_listener(waylandCallback_, &callbackListener_, this);

    shmBuffer_->attachToSurface(waylandSurface_.get());
}

void HkmLayer::renderFrame()
{
    if (!shmBuffer_)
        throw std::runtime_error(
            "Cannot render frames with a shmBuffer of nullptr");

    shmBuffer_->nextFrame();
    attachShmBuffer();
}

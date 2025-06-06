#pragma once

#include "color.hpp"
#include <cairo/cairo.h>
#include <wayland-client.h>
#include <time.h>
#include <memory.h>
#include <memory>

void bufferRelease(void *data, wl_buffer* buffer);

class ShmBuffer
{
public:
    ShmBuffer(std::shared_ptr<wl_shm> shm)
        : shm_{ shm }
    { }

    void setWidth(size_t width  ) { width_ = width; }
    void setHeight(size_t height) { height_ = height; }

    auto getWidth() { return width_; }
    auto getHeight() { return height_; }

    void setColor(uint32_t rgba)  { color_ = Color{ rgba }; }
    void setColor(Color color)  { color_ = color; }

    void nextFrame();
    void attachToSurface(wl_surface* surface);


private:
    static const Color BACKGROUND_COLOR;
    static const Color SELECTION_COLOR;
    
    cairo_t *cairo_{ nullptr };
    cairo_surface_t *cairoSurface_{ nullptr };

    static const cairo_format_t cairoFormat_{ CAIRO_FORMAT_ARGB32 };
    static const wl_shm_format waylandFormat_{ WL_SHM_FORMAT_ARGB8888 };

    std::shared_ptr<wl_shm> shm_{ nullptr };
    wl_buffer *buffer_{ nullptr };
    wl_buffer_listener bufferListener_ = wl_buffer_listener{ .release = bufferRelease };

    unsigned char *data_{ nullptr };
    size_t size_{};
    size_t height_{};
    size_t width_{};
    uint32_t scale_{ 1 };
    Color color_{};

    void randname(char *buf);

    int create_shm_file();

    int allocate_shm_file(size_t size);
};

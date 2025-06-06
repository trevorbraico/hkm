#include "shm_buffer.hpp"
#include <cairo.h>
#include <cstring>
#include <stdexcept>
#include <fcntl.h>
#include <memory.h>
#include <stdexcept>
#include <sys/mman.h>
#include <unistd.h>
#include <wayland-client-protocol.h>

const Color ShmBuffer::BACKGROUND_COLOR{ 0x3d3066, 50 };
const Color ShmBuffer::SELECTION_COLOR{ 0x959595, 25 };

void bufferRelease(void *data, wl_buffer* buffer)
{
    wl_buffer_destroy(buffer);
}

void ShmBuffer::randname(char* buf)
{
    struct timespec ts;
    clock_gettime(CLOCK_REALTIME, &ts);
    long r = ts.tv_nsec;
    for (size_t i = 0; i < strlen(buf); ++i)
    {
        buf[i] = 'A' + (r & 15) + (r & 16) * 2;
        r >>= 5;
    }
}

int ShmBuffer::create_shm_file()
{
    int retries = 100;
    do
    {
        char name[] = "/wl_shm-XXXXXX";
        randname(name + sizeof(name) - 7);
        --retries;
        int fd = shm_open(name, O_RDWR | O_CREAT | O_EXCL, 0600);
        if (fd >= 0)
        {
            shm_unlink(name); // queue for cleanup once we are done with it
            return fd;
        }
    }
    while (retries > 0 && errno == EEXIST); // keep trying 
    return -1;
}


int ShmBuffer::allocate_shm_file(size_t size)
{
    int fd = create_shm_file();
    if (fd < 0)
        return -1;
    int ret;
    do
        ret = ftruncate(fd, size);
    while (ret < 0 && errno == EINTR); // keep trying if we are getting interrupted
                                       
    if (ret < 0)
    {
        close(fd);
        return -1;
    }
    return fd;
}

void ShmBuffer::nextFrame()
{
    size_t stride = cairo_format_stride_for_width(cairoFormat_, width_);
    size_ = height_ * stride;

    int fd = allocate_shm_file(size_);
    if (fd == -1)
        throw std::runtime_error("Error allocating shared memory file");

    if (data_)
    {
        munmap(data_, size_);
    }
     
    data_ = (unsigned char *) mmap(nullptr, size_, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (data_ == MAP_FAILED)
    {
        close(fd);
        throw std::runtime_error("Error mapping shared memory file to process's memory");
    }

    wl_shm_pool *pool = wl_shm_create_pool(shm_.get(), fd, size_);

    buffer_ = wl_shm_pool_create_buffer(pool, 0, width_, height_, stride, waylandFormat_);
    wl_buffer_add_listener(buffer_, &bufferListener_, nullptr);

    wl_shm_pool_destroy(pool);
    close(fd);


    if (cairoSurface_)
        cairo_surface_destroy(cairoSurface_);
    
    if (cairo_)
        cairo_destroy(cairo_);

    cairoSurface_ = cairo_image_surface_create_for_data(data_, cairoFormat_, width_, height_, stride);
	cairo_ = cairo_create(cairoSurface_);

    cairo_identity_matrix(cairo_);
    cairo_scale(cairo_, scale_, scale_);
    cairo_set_operator(cairo_, CAIRO_OPERATOR_SOURCE);
    
    // render shit (just fill with a color)
    setColor(BACKGROUND_COLOR);
    cairo_set_source_rgba(
        cairo_,
        color_.red   / 255.0,
        color_.green / 255.0,
        color_.blue  / 255.0,
        color_.alpha / 255.0
    );
    cairo_paint(cairo_);
}

void ShmBuffer::attachToSurface(wl_surface* surface)
{
    wl_surface_attach(surface, buffer_, 0, 0);
    wl_surface_damage(surface, 0, 0, width_, height_);
    wl_surface_set_buffer_scale(surface, scale_);
    wl_surface_commit(surface);
}

#include "inputs.hpp"
#include "wlr-virtual-pointer-unstable-v1.h"
#include <wayland-client.h>
#include <boost/asio.hpp>
#include <linux/input-event-codes.h>
#include <sys/mman.h>
#include <sys/select.h>
#include <cassert>
#include <chrono>
#include <ctime>
#include <memory>
#include <thread>
#include <unordered_map>

/* returns the current Unix epoch time in ms */
unsigned long currentTime()
{
    auto t = std::chrono::system_clock::now().time_since_epoch();
    return std::chrono::duration_cast<std::chrono::milliseconds>(t).count();
}

void seatCapabilities(void *data, wl_seat *seat, uint32_t capabilities)
{
    Inputs *inputs = (Inputs *)data;

    bool has_mouse = capabilities & WL_SEAT_CAPABILITY_POINTER;
    if (has_mouse && !inputs->hasPointer())
        inputs->setPointer(wl_seat_get_pointer(seat));

    else if (!has_mouse && inputs->hasPointer())
        inputs->unsetPointer();

    bool has_keyboard = capabilities & WL_SEAT_CAPABILITY_KEYBOARD;
    if (has_keyboard && !inputs->hasKeyboard())
        inputs->setKeyboard(wl_seat_get_keyboard(seat));

    else if (!has_keyboard && inputs->hasKeyboard())
        inputs->unsetKeyboard();
}

void seatName(void *data, wl_seat *seat, const char *name) { /* nop */ }

void pointerEnter(void *data, wl_pointer *pointer, uint32_t serial,
                  wl_surface *surface, wl_fixed_t x_fixed, wl_fixed_t y_fixed)
{
    Inputs *inputs = (Inputs *)data;

    inputs->setCursorShape(serial);
}

void pointerLeave(void *data, wl_pointer *pointer, uint32_t serial,
                  wl_surface *surface)
{
    Inputs *inputs = (Inputs *)data;
    (void)inputs;
}

void pointerButton(void *data, wl_pointer *pointer, uint32_t serial,
                   uint32_t time, uint32_t button, uint32_t button_state)
{
    Inputs *inputs = (Inputs *)data;
    inputs->setWaylandTime(time);

    if (button == BTN_LEFT && button_state == WL_POINTER_BUTTON_STATE_PRESSED)
    {
        inputs->log("you clicked the left button");
    }
}

void pointerMotion(void *data, wl_pointer *pointer, uint32_t time,
                   wl_fixed_t x_fixed, wl_fixed_t y_fixed)
{
    Inputs *inputs = (Inputs *)data;
    inputs->setWaylandTime(time);

    uint32_t x = wl_fixed_to_int(x_fixed);
    uint32_t y = wl_fixed_to_int(y_fixed);

    auto count = inputs->getPointerUpdateCount();

    if (count == 0)
        inputs->log("updating pointer position to ({}, {})", x, y);

    inputs->setCursorPosition(x, y);
    inputs->incPointerUpdateCount();
}

void pointerFrame(void *data, wl_pointer *pointer) { /* nop */ }
void pointerAxis(void *data, wl_pointer *pointer, uint32_t time, uint32_t axis, wl_fixed_t value) { /* nop */ }
void pointerAxisSource(void *data, wl_pointer *pointer, uint32_t axis_source) { /* nop */ }
void pointerAxisStop(void *data, wl_pointer *pointer, uint32_t time, uint32_t axis) { /* nop */ }
void pointerAxisDiscrete(void *data, wl_pointer *pointer, uint32_t axis, int32_t discrete) { /* nop */ }

void keyboardKeymap(void *data, wl_keyboard *kb, uint32_t keymap_format,
                    int32_t fd, uint32_t size)
{
    Inputs *inputs = (Inputs *)data;

    assert(keymap_format == WL_KEYBOARD_KEYMAP_FORMAT_XKB_V1
           && "Recieved invalid keymap format from wl_keyboard keymap event");

    char *map_shm = (char *)mmap(NULL, size, PROT_READ, MAP_PRIVATE, fd,
                                 0); // must use MAP_PRIVATE
    if (map_shm == MAP_FAILED)
        throw std::runtime_error(
            "Could not map keymap fd to this process's address space");

    inputs->keyboardTranslator_ = std::make_unique<KeyboardTranslator>(map_shm);

    munmap(map_shm, size);
    close(fd);
}

void keyboardEnter(void *data, wl_keyboard *kb, uint32_t serial,
                   wl_surface *surface, wl_array *keys)
{
    Inputs *inputs = (Inputs *)data;
    (void)inputs;
}

void keyboardLeave(void *data, wl_keyboard *kb, uint32_t serial,
                   wl_surface *surface)
{
    Inputs *inputs = (Inputs *)data;
    (void)inputs;
}

void keyboardKey(void *data, wl_keyboard *kb, uint32_t serial, uint32_t time,
                 uint32_t scancode, uint32_t key_state)
{
    Inputs *inputs = (Inputs *)data;
    inputs->setWaylandTime(time);

    // add 8 to convert from Linux keycode to XKBcommon keycode
    std::string decoded_key = inputs->keyboardTranslator_->getKeyUTF8(scancode + 8);
    if (decoded_key.length() > 1)
        throw std::runtime_error("Got decoded key press of length > 1. Idk what to do with this");

    char key = decoded_key[0];

    if (key_state == WL_KEYBOARD_KEY_STATE_PRESSED)
    {
        inputs->moveCursorForKey(key);
        inputs->addHeldKey(key);
    } else if (key_state == WL_KEYBOARD_KEY_STATE_RELEASED)
    {
        inputs->removeKey(key);
    }
}

void keyboardModifiers(void *data, wl_keyboard *kb, uint32_t serial,
                       uint32_t depressed, uint32_t latched, uint32_t locked,
                       uint32_t group)
{
    Inputs *inputs = (Inputs *)data;
    inputs->keyboardTranslator_->updateMask(depressed, latched, locked, group);
}

void keyboardRepeatInfo(void *data, wl_keyboard *keyboard, int32_t rate,
                        int32_t delay)
{
    Inputs *inputs = (Inputs *)data;

    inputs->log("Got repeat info of (delay={}, rate={})", delay, rate);
    inputs->setRepeatInfo(delay, rate);

    inputs->initKeyRepeatingThread();
}

bool Inputs::getKeyBeingRepeatedAtomic(char key)
{
    std::lock_guard lock(repeatingKeysLock_);
    return repeatingKeys_.contains(key);
}

struct RepeatKeyArgs
{
    Inputs *inputs{ nullptr };
    char key{ '\0' };
};

void repeatKey(void *args)
{
    RepeatKeyArgs *repeat_args = (RepeatKeyArgs *)args;
    Inputs *inputs = repeat_args->inputs;
    char key= repeat_args->key;

    inputs->log("repeating {}", key);

    long wait_time = (1 / static_cast<double>(inputs->repeatDelay_)) * 1'000;  // convert from Hz to ms
    bool repeat_key = true;
    while (repeat_key)
    {
        inputs->moveCursorForKey(key);
        repeat_key = inputs->getKeyBeingRepeatedAtomic(key);
        std::this_thread::sleep_for(std::chrono::milliseconds(wait_time));
    }

    inputs->log("stopped repeating {}", key);
    delete repeat_args;
}

void Inputs::initKeyRepeatingThread()
{
    log("Initializing keyRepeatingManager_");
    keyRepeatingManager_ = std::thread{ [this] {
        while (true)  // check through heldKeys_ for the lifetime of the program
        {
            for (auto &e : heldKeys_)
            {
                if (currentTime() >= e.press_time + repeatDelay_)
                {
                    auto key = e.key;
                    RepeatKeyArgs *args = new RepeatKeyArgs{ this, key };
                    if (!getKeyBeingRepeatedAtomic(key))
                    {
                        repeatingKeys_.insert(key);
                        keyRepeaters_.run(repeatKey, args);
                    }
                }
            }
        }
    } };
}

void Inputs::addHeldKey(char key)
{
    heldKeys_.push_back({ key, currentTime() });
}

void Inputs::removeKey(char key)
{
    // the key will for sure be in heldKeys_
    std::erase_if(heldKeys_, [key] (const HeldKey &e) { return e.key == key; } );
    
    // it may or may not be in repeatingKeys (if we have been holding it down), atomically remove it if it is
    std::lock_guard lock{ repeatingKeysLock_ };
    if (repeatingKeys_.contains(key))
        repeatingKeys_.erase(key);
}

void Inputs::setSeat(wl_seat *seat)
{
    seat_ = seat;
    seatListener_ = { .capabilities = seatCapabilities, .name = seatName };
    wl_seat_add_listener(seat_, &seatListener_, this);
}

void Inputs::setPointer(wl_pointer *pointer)
{
    pointer_ = pointer;
    pointerListener_ = wl_pointer_listener{
        .enter = pointerEnter,
        .leave = pointerLeave,
        .motion = pointerMotion,
        .button = pointerButton,
        .axis = pointerAxis,
        .frame = pointerFrame,
        .axis_source = pointerAxisSource,
        .axis_stop = pointerAxisStop,
        .axis_discrete = pointerAxisDiscrete,
    };
    wl_pointer_add_listener(pointer_, &pointerListener_, this);
}

void Inputs::setCursorShapeManager(CursorShapeManager *cursorShapeManager)
{
    shapeManager_ = cursorShapeManager;
}

void Inputs::setKeyboard(wl_keyboard *keyboard)
{
    keyboard_ = keyboard;
    keyboardListener_
        = wl_keyboard_listener{ .keymap = keyboardKeymap,
                                .enter = keyboardEnter,
                                .leave = keyboardLeave,
                                .key = keyboardKey,
                                .modifiers = keyboardModifiers,
                                .repeat_info = keyboardRepeatInfo };

    wl_keyboard_add_listener(keyboard_, &keyboardListener_, this);
}

void Inputs::setVirtualPointerManager(VirtualPointerManager *pointerManager)
{
    pointerManager_ = pointerManager;
    if (!pointerManager_)
        log("pointerManager_ set to nullptr. Not going to proceed with "
            "creating virtualPointer_");
    else if (!seat_)
        log("pointerManager_ set to a valid address, but seat is nullptr. Not "
            "going to proceed with creating virtualPointer_");
    else
    {
        log("creating virtualPointer_ from pointerManager_ (without wl_output "
            "option)");

        // create the virtual pointer for the entire seat (i.e. global to the
        // compositor) not just for a specific monitor
        virtualPointer_
            = zwlr_virtual_pointer_manager_v1_create_virtual_pointer(
                pointerManager_, seat_);
    }
}

void Inputs::setCursorShape(uint32_t serial)
{
    shapeDevice_
        = wp_cursor_shape_manager_v1_get_pointer(shapeManager_, pointer_);
    wp_cursor_shape_device_v1_set_shape(
        shapeDevice_, serial, WP_CURSOR_SHAPE_DEVICE_V1_SHAPE_CROSSHAIR);
}

void Inputs::moveCursorForKey(char key)
{
    auto iter = Inputs::movements.find(key);
    if (iter != Inputs::movements.end())
    {
        zwlr_virtual_pointer_v1_motion(virtualPointer_, waylandTime_,
                                       wl_fixed_from_int(iter->second.dx),
                                       wl_fixed_from_int(iter->second.dy));
    }
}

void Inputs::virtualPointerFrame()
{
    zwlr_virtual_pointer_v1_frame(virtualPointer_);
}

const std::unordered_map<char, Inputs::Displacement> Inputs::movements{ {
    { 'h', { -Inputs::change, 0 } },
    { 'j', { 0, Inputs::change } }, // positive is down on the screen to the compostior
    { 'k', { 0, -Inputs::change } },
    { 'l', { Inputs::change, 0 } }, // negative is up on the screen
} };

Inputs::Inputs() : Loggable{ "Inputs" }
{ }

Inputs::~Inputs()
{
    // this needs to be fixed (i.e. use RAII)
    if (seat_)
        wl_seat_destroy(seat_);
    if (pointer_)
        wl_pointer_destroy(pointer_);
    if (shapeManager_)
        wp_cursor_shape_manager_v1_destroy(shapeManager_);
    if (shapeDevice_)
        wp_cursor_shape_device_v1_destroy(shapeDevice_);
    if (keyboard_)
        wl_keyboard_destroy(keyboard_);
    if (pointerManager_)
        zwlr_virtual_pointer_manager_v1_destroy(pointerManager_);
    if (virtualPointer_)
        zwlr_virtual_pointer_v1_destroy(virtualPointer_);
}

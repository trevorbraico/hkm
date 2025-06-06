#pragma once 

#include "loggable.hpp"
#include "keyboard_translator.hpp"
#include "thread_pool.hpp"
#include <condition_variable>
#include <thread>
#include <wayland-client-protocol.h>
#include <wlr-virtual-pointer-unstable-v1.h>
#include <cursor-shape-v1.h>
#include <wayland-cursor.h>
#include <wayland-client.h>
#include <memory>
#include <vector>
#include <unordered_set>
#include <unordered_map>

using CursorShapeManager = wp_cursor_shape_manager_v1;
using CursorShapeDevice = wp_cursor_shape_device_v1;

using VirtualPointerManager = zwlr_virtual_pointer_manager_v1;
using VirtualPointer = zwlr_virtual_pointer_v1;

using EpochTime = unsigned long;

class Inputs : public Loggable
{
public:

    Inputs();

    ~Inputs();

    struct Position
    {
        uint32_t x{ 0 };
        uint32_t y{ 0 };
    };

    struct RepeatInfo
    {
        uint32_t delay{ 0 };
        uint32_t rate{ 0 };
    };

    struct Displacement
    {
        int dx{ 0 };
        int dy{ 0 };
    };

    struct HeldKey
    {
        char key{ '\0' };
        EpochTime press_time{ 0 };
    };

    struct KeyRepeater
    {
        unsigned char key{ '\0' };
    };

    static constexpr int change = 100;

    std::unique_ptr<KeyboardTranslator> keyboardTranslator_{ nullptr };

    void setSeat(wl_seat *seat);
    void setPointer(wl_pointer *pointer);
    void setCursorShapeManager(CursorShapeManager *cursorShapeManager);
    void setKeyboard(wl_keyboard *keyboard);
    void setVirtualPointerManager(VirtualPointerManager *pointerManager);
    void setCursorShape(uint32_t serial);

    void setRepeatInfo(uint32_t delay, uint32_t rate) {
        repeatDelay_ = delay;
        repeatRate_ = rate;
    }

    void setCursorPosition(uint32_t x, uint32_t y) { cursorPosition_ = { x, y }; } 

    void setWaylandTime(uint32_t waylandTime) { waylandTime_ = waylandTime; }

    bool hasSeat() { return seat_ != nullptr; }
    bool hasKeyboard() { return keyboard_ != nullptr; }
    bool hasPointer() { return pointer_ != nullptr; }
    bool hasVirtualPointer() { return virtualPointer_ != nullptr; }

    void unsetPointer() 
    {
        wl_keyboard_destroy(keyboard_);
        keyboard_ = nullptr;
    }

    void unsetKeyboard() 
    {
        wl_pointer_destroy(pointer_);
        pointer_ = nullptr;
    }

    Position getCursorPosition() { return cursorPosition_; }

    unsigned int getPointerUpdateCount() { return pointerUpdateCount_; }

    void incPointerUpdateCount(){ pointerUpdateCount_ = (pointerUpdateCount_ + 1) % 25; }
    
    // only call once we have recieved repeat info about the keyboard from the compositor 
    void initKeyRepeatingThread();

    bool getKeyBeingRepeatedAtomic(char key);

    void moveCursorForKey(char key);
    void addHeldKey(char key);  // called on first press
    void removeKey(char key);  // called when key released

    void virtualPointerFrame();
    
    // we make a non-member function (with explicit this pointer as arg)
    // a friend since we apparently cannot call non-static member functions
    // from inside of a thread??
    friend void repeatKey(void *args);

private:
    wl_seat *seat_{ nullptr };
    wl_seat_listener seatListener_;

    wl_pointer *pointer_{ nullptr };
    wl_pointer_listener pointerListener_;

    CursorShapeManager *shapeManager_{ nullptr };
    CursorShapeDevice *shapeDevice_{ nullptr };

    wl_keyboard *keyboard_{ nullptr };
    wl_keyboard_listener keyboardListener_;

    VirtualPointer *virtualPointer_{ nullptr };
    VirtualPointerManager *pointerManager_{ nullptr };

    Position cursorPosition_{ -1, -1 };
    int repeatDelay_{ -1 };
    int repeatRate_{ -1 };
    uint32_t waylandTime_;

    unsigned int pointerUpdateCount_{ 0 };

    static const std::unordered_map<char, Displacement> movements;

    std::vector<HeldKey> heldKeys_;
    std::thread keyRepeatingManager_;

    std::mutex repeatingKeysLock_;
    std::unordered_set<char> repeatingKeys_;
    ThreadPool keyRepeaters_{ std::thread::hardware_concurrency() };
};

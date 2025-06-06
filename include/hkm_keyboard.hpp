#pragma once

#include "keyboard_translator.hpp"
#include "loggable.hpp"
#include <wayland-client.h>
#include <xkbcommon/xkbcommon.h>
#include <memory>

class HkmKeyboard : public Loggable
{
public:
    std::unique_ptr<KeyboardTranslator> translator{ nullptr };

    HkmKeyboard(wl_keyboard *keyboard); 

    ~HkmKeyboard()
    {
        if (keyboard_) wl_keyboard_release(keyboard_);
    }

private:
    wl_keyboard *keyboard_{ nullptr };
    wl_keyboard_listener keyboardListener_;
};

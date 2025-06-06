#pragma once

#include <xkbcommon/xkbcommon.h>
#include <wayland-client.h>
#include <string>

class KeyboardTranslator
{
public:
    KeyboardTranslator(char* keymap_str);

    ~KeyboardTranslator();

    std::string getKeyUTF8(uint32_t keycode);
    void updateMask(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group);

private:
    xkb_context *context_{ nullptr };   
    xkb_keymap *keymap_{ nullptr };
    xkb_state *state_{ nullptr };
};

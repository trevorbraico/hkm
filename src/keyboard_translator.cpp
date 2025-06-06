#include "keyboard_translator.hpp"
#include <wayland-client-protocol.h>
#include <xkbcommon/xkbcommon.h>
#include <sys/mman.h>
#include <cassert>
#include <unistd.h>
#include <memory.h>

KeyboardTranslator::KeyboardTranslator(char* keymap_str)
{
    context_ = xkb_context_new(XKB_CONTEXT_NO_FLAGS);
    keymap_ = xkb_keymap_new_from_string(context_, keymap_str, XKB_KEYMAP_FORMAT_TEXT_V1, XKB_KEYMAP_COMPILE_NO_FLAGS);
    state_ = xkb_state_new(keymap_);
}

KeyboardTranslator::~KeyboardTranslator()
{
    if (context_) xkb_context_unref(context_);
    if (keymap_) xkb_keymap_unref(keymap_);
    if (state_) xkb_state_unref(state_);
}

std::string KeyboardTranslator::getKeyUTF8(uint32_t keycode)
{
    std::string key_utf8{ 128 };
    xkb_state_key_get_utf8(state_, keycode, key_utf8.data(), sizeof(key_utf8));
    return key_utf8;
}

void KeyboardTranslator::updateMask(uint32_t depressed, uint32_t latched, uint32_t locked, uint32_t group)
{
    xkb_state_update_mask(state_, depressed, latched, locked, 0, 0, group);
}

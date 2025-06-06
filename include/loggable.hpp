#pragma once

#include <iostream>
#include <format>
#include <utility>

class Loggable
{
public:

    Loggable() = delete;

    Loggable(std::string_view description)
        : description_{ description }
    { }

    template <typename... Args>
    void log(std::string_view message, Args&&... args) const
    {
        auto format_args = std::make_format_args(std::forward<Args&>(args)...);
        std::cout << ("[" + description_ + "] ") << std::vformat(message, format_args) << std::endl;
    }
     
private:
    std::string description_{ };
};

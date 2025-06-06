#include "compositor_messenger.hpp"
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <filesystem>
#include <stdexcept>
#include <format>
#include <sys/socket.h>
#include <unistd.h>

std::string env(const char *envvar)
{
    char * val = getenv(envvar);
    if (val == nullptr)
        throw std::runtime_error(std::format("Could not find value of {}", envvar));
    return val;
}

CompositorMessenger::CompositorMessenger()
{
    const std::string instance_sig = env("HYPRLAND_INSTANCE_SIGNATURE");
    const std::string runtime_dir = env("XDG_RUNTIME_DIR");

    const std::string socket_path{ std::format("{}/hypr/{}/.socket.sock", runtime_dir, instance_sig) };
    if (!std::filesystem::exists(socket_path)) 
        throw std::runtime_error(std::format("Hyprland IPC socket not found at {}", socket_path));

    socketfd_ = socket(AF_UNIX, SOCK_STREAM, 0);
    if (socketfd_ < 0)
        throw std::runtime_error("Could not open socket for IPC");

    socketAddress_.sun_family = AF_UNIX;
    strcpy(socketAddress_.sun_path, socket_path.data());

    if (connect(socketfd_, (sockaddr*) &socketAddress_, sizeof(socketAddress_)) < 0)
    {
        close(socketfd_);
        throw std::runtime_error("Could not connect to socket for IPC");
    }
}

std::string CompositorMessenger::sendMessage(const std::string& message)
{
    if (write(socketfd_, message.c_str(), message.size()) < 0)
    {
        close(socketfd_);
        throw std::runtime_error("Failed writing message to socket");
    }

    // read 200 bytes at a time
    char buffer[200];
    std::string response{ "" };
    int n = 0;
    do {
        n = read(socketfd_, buffer, sizeof(buffer));

        if (n < 0)
        {
            close(socketfd_);
            throw std::runtime_error("Failed to read response from socket");
        }

        response += static_cast<std::string>(buffer);

    } while (n > 0);

    return response;
}

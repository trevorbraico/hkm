#pragma once

#include <string>
#include <cstdlib>
#include <unistd.h>
#include <sys/un.h>

class CompositorMessenger
{
public:
    CompositorMessenger();

    ~CompositorMessenger()
    {
        close(socketfd_);
    }

    /*
     * Sends the given message to the Hyprland socket connection
     * and returns the response to the message
     */
    std::string sendMessage(const std::string&);

private:
    sockaddr_un socketAddress_{};
    int socketfd_{ -1 };
};

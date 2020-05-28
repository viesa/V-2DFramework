#pragma once

#include <SFML/Config.hpp>
#include <SFML/Network/TcpListener.hpp>
#include <SFML/Network/TcpSocket.hpp>
#include <SFML/Network/UdpSocket.hpp>

enum Protocol : sf::Uint8
{
    UDP,
    TCP,
    None // Used ONLY as fallback. Attempting to use this will be treated as using undefined protocol
};

template <typename S>
static constexpr Protocol ProtocolFrom() noexcept
{
    if constexpr (std::is_same<S, sf::TcpListener>::value)
    {
        return Protocol::TCP;
    }
    else if constexpr (std::is_same<S, sf::TcpSocket>::value)
    {
        return Protocol::TCP;
    }
    else if constexpr (std::is_same<S, sf::UdpSocket>::value)
    {
        return Protocol::UDP;
    }
    return Protocol::None;
}

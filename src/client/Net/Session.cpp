//////////////////////////////////////////////////////////////////////////////
// This file is part of the Journey MMORPG client                           //
// Copyright © 2015-2016 Daniel Allendorf                                   //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful,          //
// but WITHOUT ANY WARRANTY; without even the implied warranty of           //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
#include "Session.h"

#include "../Configuration.h"
#include "../Console.h"
#ifdef MS_PLATFORM_WASM
#include "../Util/Misc.h"
#endif

#include <algorithm>
#include <cstring>
#include <string>

namespace jrc
{
    namespace
    {
        bool is_local_channel_address(const std::string& address)
        {
            return address == "127.0.0.1" || address == "0.0.0.0" || address == "localhost";
        }

        std::string resolve_channel_address(const char* address)
        {
            std::string target_address = (address != nullptr) ? address : "";
            if (!is_local_channel_address(target_address))
            {
                return target_address;
            }

            std::string configured_server_ip = Setting<MapleStoryServerIp>::get().load();
            if (configured_server_ip.empty())
            {
                return target_address;
            }

            Console::get().print(
                "Intercepted local channel IP " + target_address +
                ", substituting with " + configured_server_ip
            );
            return configured_server_ip;
        }
    }

    Session::Session()
    {
        connected         = false;
        connection_changed = false;
        header_pos        = 0;
        length            = 0;
        pos               = 0;
    }

    Session::~Session()
    {
        disconnect();
    }

    bool Session::init(const char* host, const char* port)
    {
#ifdef MS_PLATFORM_WASM
        std::string final_host = host;
        if (final_host == "0.0.0.0")
        {
            final_host = getBrowserHostname();
            Console::get().print("Host is 0.0.0.0, resolving to: " + final_host);
        }
        Console::get().print("Connecting to " + final_host + ":" + port);
        connected = socket.open(final_host.c_str(), port);
#else
        // Connect to the server.
        connected = socket.open(host, port);
#endif

        if (connected)
        {
            // Read keys neccessary for communicating with the server.
            cryptography = { socket.get_buffer() };
        }
#ifdef MS_PLATFORM_WASM
        else
        {
            Console::get().print("Failed to connect");
        }
#endif

        return connected;
    }

    Error Session::init()
    {
        std::string HOST = Setting<MapleStoryServerIp>::get().load();
        std::string PORT = Setting<MapleStoryServerPort>::get().load();

        if (!init(HOST.c_str(), PORT.c_str()))
        {
            return Error::CONNECTION;
        }

        return Error::NONE;
    }

    void Session::reconnect(const char* address, const char* port)
    {
        std::string target_address = resolve_channel_address(address);
        const char* target_port = (port != nullptr) ? port : "";
        Console::get().print(
            "Reconnecting to channel server " + target_address + ":" + target_port
        );

        // Close the current connection and open a new one.
        bool success = socket.close();

        header_pos = 0;
        length = 0;
        pos = 0;

        if (success)
        {
            init(target_address.c_str(), target_port);
        }
        else
        {
            connected = false;
        }

        // Signal that the socket/crypto were swapped so process() can stop
        // consuming bytes that belonged to the old connection.
        connection_changed = true;
    }

    void Session::disconnect()
    {
        if (connected)
        {
            socket.close();
        }

        connected = false;
        header_pos = 0;
        length = 0;
        pos = 0;
        connection_changed = true;
    }

    void Session::process(const int8_t* bytes, size_t available)
    {
        while (available > 0)
        {
            if (length == 0)
            {
                const size_t header_remaining = HEADER_LENGTH - header_pos;
                const size_t header_bytes = std::min(header_remaining, available);
                memcpy(header + header_pos, bytes, header_bytes);
                header_pos += header_bytes;
                bytes += header_bytes;
                available -= header_bytes;

                if (header_pos < HEADER_LENGTH)
                {
                    return;
                }

                length = cryptography.check_length(header);
                header_pos = 0;
                if (length < OPCODE_LENGTH || length > MAX_PACKET_LENGTH)
                {
                    Console::get().print("Invalid packet length: " + std::to_string(length));
                    disconnect();
                    return;
                }
            }

            const size_t body_bytes = std::min(length - pos, available);
            memcpy(buffer + pos, bytes, body_bytes);
            pos += body_bytes;
            bytes += body_bytes;
            available -= body_bytes;

            if (pos < length)
            {
                return;
            }

            cryptography.decrypt(buffer, length);
            try
            {
                packetswitch.forward(buffer, length);
            }
            catch (const PacketError& err)
            {
                Console::get().print(err.what());
            }

            pos    = 0;
            length = 0;

            // If reconnect() swapped the connection during forward(), the
            // remaining bytes belong to the old stream and the new crypto
            // would misread them. Discard and reset.
            if (connection_changed)
            {
                connection_changed = false;
                return;
            }
        }
    }

    void Session::write(int8_t* packet_bytes, size_t packet_length)
    {
        if (!connected)
        {
            return;
        }

        int8_t header[HEADER_LENGTH];
        cryptography.create_header(header, packet_length);
        cryptography.encrypt(packet_bytes, packet_length);

        socket.dispatch(header, HEADER_LENGTH);
        socket.dispatch(packet_bytes, packet_length);
    }

    void Session::read()
    {
        // Check if a packet has arrived. Handle if data is sufficient:
        //     4 bytes(header) + 2 bytes(opcode) = 6.
        size_t result = socket.receive(&connected);

        if (result > 0)
        {
            // Retrieve buffer from the socket and process it.
            const int8_t* bytes = socket.get_buffer();
            process(bytes, result);
        }
    }

    bool Session::is_connected() const
    {
        return connected;
    }
}

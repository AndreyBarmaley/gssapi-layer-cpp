/***************************************************************************
 *   Copyright © 2023 by Andrey Afletdinov <public.irkutsk@gmail.com>      *
 *                                                                         *
 *   https://github.com/AndreyBarmaley/gssapi-layer-cpp                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 3 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <sstream>
#include <cstring>
#include <iomanip>
#include <iostream>

#include "gsslayer.h"
#include "tools.h"

std::string buffer2hexstring(const uint8_t* data, size_t length, std::string_view sep = ",", bool prefix = true)
{
    std::ostringstream os;
    for(size_t it = 0; it != length; ++it)
    {
       if(prefix)
           os << "0x";
       os << std::setw(2) << std::setfill('0') << std::uppercase << std::hex << static_cast<int>(data[it]);
       if(sep.size() && it + 1 != length) os << sep;
    }
    return os.str();
}

class GssApiServer : public Gss::ServiceContext
{
    int sock = 0;

public:
    GssApiServer() = default;

    // ServiceContext override
    std::vector<uint8_t> recvToken(void) override
    {
        auto len = TCPSocket::readIntBE32(sock);
        std::cout << "token recv: " << len << std::endl;
        return TCPSocket::readData(sock, len);
    }

    // ServiceContext override
    void sendToken(const void* buf, size_t len) override
    {
        std::cout << "token send: " << len << std::endl;
        TCPSocket::writeIntBE32(sock, len);
        TCPSocket::writeData(sock, buf, len);
    }

    // ServiceContext override
    void error(const char* func, const char* subfunc, OM_uint32 code1, OM_uint32 code2) const override
    {
        std::cerr << func << ": " << subfunc << " failed, " << Gss::error2str(code1, code2) << std::endl;
    }

    int start(int port, std::string_view service)
    {
        std::cout << "service id: " << service.data() << std::endl;

        if(! acquireCredential(service, Gss::NameType::NtHostService))
            return -1;

        int srvfd = TCPSocket::listen("any", port);
        std::cout << "srv fd: " << srvfd << std::endl;

        sock = TCPSocket::accept(srvfd);
        std::cout << "sock fd: " << sock << std::endl;

        if(! acceptClient())
            return -1;

        // client info
        std::string name1 = Gss::exportName(srcName());
        std::cout << "client id: " << name1 << std::endl;

        // mech types
        auto names = mechNames();
        auto mech = Gss::exportOID(mechTypes());

        std::cout << "mechanism " << mech << " supports " << names.size() << " names" << std::endl;

        for(auto & name : mechNames())
        {
            std::cout << " - mech name: " << name << std::endl;
        }

        // flags
        for(auto & f : Gss::exportFlags(supportFlags()))
        {
            std::cout << "supported flag: " << flagName(f) << std::endl;
        }

        auto buf = recvMessage();
        std::cout << "recv data: " << buffer2hexstring(buf.data(), buf.size()) << std::endl;

        auto res = sendMIC(buf.data(), buf.size());
        std::cout << "send mic: " << (res ? "success" : "failed") << std::endl;

        return 0;
    }
};

int main(int argc, char **argv)
{
    int res = 0;
    int port = 44444;
    std::string service = "TestService";

    for(int it = 1; it < argc; ++it)
    {
        if(0 == std::strcmp(argv[it], "--port") && it + 1 < argc)
        {
            try
            {
                port = std::stoi(argv[it + 1]);
            }
            catch(const std::invalid_argument &)
            {
                std::cerr << "incorrect port number" << std::endl;
            }
            it = it + 1;
        }
        else
        if(0 == std::strcmp(argv[it], "--service") && it + 1 < argc)
        {
            service.assign(argv[it + 1]);
            it = it + 1;
        }
        else
        {
            std::cout << "usage: " << argv[0] << " --port 44444" << " --service <" << service << ">" << std::endl;
            return 0;
        }
    }

    try
    {
        res = GssApiServer().start(port, service);
    }
    catch(const std::exception & err)
    {
        std::cerr << "exception: " << err.what() << std::endl;
    }

    return res;
}

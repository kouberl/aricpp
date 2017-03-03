/*******************************************************************************
 * ARICPP - ARI interface for C++
 * Copyright (C) 2017 Daniele Pallastrelli
 *
 * This file is part of aricpp.
 * For more information, see http://github.com/daniele77/aricpp
 *
 * Boost Software License - Version 1.0 - August 17th, 2003
 *
 * Permission is hereby granted, free of charge, to any person or organization
 * obtaining a copy of the software and accompanying documentation covered by
 * this license (the "Software") to use, reproduce, display, distribute,
 * execute, and transmit the Software, and to prepare derivative works of the
 * Software, and to permit third-parties to whom the Software is furnished to
 * do so, all subject to the following:
 *
 * The copyright notices in the Software and this entire statement, including
 * the above license grant, this restriction and the following disclaimer,
 * must be included in all copies of the Software, in whole or in part, and
 * all derivative works of the Software, unless such copies or derivative
 * works are solely in the form of machine-executable object code generated by
 * a source language processor.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, TITLE AND NON-INFRINGEMENT. IN NO EVENT
 * SHALL THE COPYRIGHT HOLDERS OR ANYONE DISTRIBUTING THE SOFTWARE BE LIABLE
 * FOR ANY DAMAGES OR OTHER LIABILITY, WHETHER IN CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************/


#ifndef ARICPP_BRIDGE_H_
#define ARICPP_BRIDGE_H_

#include <string>
#include "client.h"
#include "proxy.h"
#include "terminationdtmf.h"

namespace aricpp
{

class Bridge
{
public:

    ///////////////////////////////////////////////////////////////
    // Role smart enum

    // all this machinery to initialize static members in the header file

    class Role; // forward declaration

    template<class Dummy>
    struct RoleBase
    {
        static const Role announce;
        static const Role partecipant;
    };

    class Role : public RoleBase<void>
    {
    public:
        operator std::string() const { return value; }
    private:
        friend struct RoleBase<void>;
        Role(const char* v) : value(v) {}
        const std::string value;
    };

    ///////////////////////////////////////////////////////////////

    Bridge(const Bridge& rhs) = delete;
    Bridge& operator=(const Bridge& rhs) = delete;
    Bridge(Bridge&& rhs) : id(rhs.id), client(rhs.client) { rhs.id.clear(); }
    Bridge& operator=(Bridge&& rhs)
    {
        if ( &rhs != this )
        {
            id = rhs.id;
            client = rhs.client;
            rhs.id.clear();
        }
        return *this;
    }

    // TODO: shouldn't destroy the physical resource,
    // because we're calling the dtor when the BridgeDestroyed event is received
    ~Bridge() { Destroy(); }

    /// Create an object handling a asterisk bridge already existing
    Bridge(const std::string& _id, Client& _client) : id(_id), client(&_client) {}

    /// Create a new bridge on asterisk
    template<typename CreationHandler>
    Bridge(Client& _client, CreationHandler h) : client(&_client)
    {
        client->RawCmd(
            "POST",
            "/ari/bridges?type=mixing",
            [this,h](auto,auto,auto,auto body)
            {
                auto tree = FromJson(body);
                id = Get<std::string>(tree, {"id"});
                h();
            }
        );
    }

    Proxy& Add(const Channel& ch, Role role=Role::partecipant)
    {
        return Proxy::Command(
            "POST",
            "/ari/bridges/" + id +
            "/addChannel?channel=" + ch.Id() +
            "&role=" + static_cast<std::string>(role),
            client
        );
    }

    Proxy& Add(std::initializer_list<const Channel*> chs)
    {
        std::string req = "/ari/bridges/" + id + "/addChannel?channel=";
        for (auto ch=chs.begin(); ch!=chs.end(); ++ch)
            req += (*ch)->Id() + ',';
        req.pop_back(); // removes trailing ','
        return Proxy::Command("POST", std::move(req), client);
    }

    Proxy& Remove(const Channel& ch)
    {
        return Proxy::Command(
            "POST",
            "/ari/bridges/" + id +
            "/removeChannel?channel=" + ch.Id(),
            client
        );
    }

    Proxy& StartMoh(const std::string& mohClass={})
    {
        std::string query = "/ari/bridges/" + id + "/moh";
        if ( !mohClass.empty() ) query += "?mohClass" + mohClass;
        return Proxy::Command("POST", std::move(query), client);
    }

    Proxy& StopMoh()
    {
        return Proxy::Command("DELETE", "/ari/bridges/" + id + "/moh", client);
    }

    Proxy& Play(const std::string& media, const std::string& lang={},
                const std::string& playbackId={}, int offsetms=-1, int skipms=-1) const
    {
        return Proxy::Command(
            "POST",
            "/ari/bridges/"+id+"/play?"
            "media=" + media +
            ( lang.empty() ? "" : "&lang=" + lang ) +
            ( playbackId.empty() ? "" : "&playbackId=" + playbackId ) +
            ( offsetms < 0 ? "" : "&offsetms=" + std::to_string(offsetms) ) +
            ( skipms < 0 ? "" : "&skipms=" + std::to_string(skipms) ),
            client
        );
    }

    Proxy& Record(const std::string& name, const std::string& format,
                  int maxDurationSeconds=-1, int maxSilenceSeconds=-1,
                  const std::string& ifExists={}, bool beep=false, TerminationDtmf terminateOn=TerminationDtmf::none) const
    {
        return Proxy::Command(
            "POST",
            "/ari/bridges/"+id+"/record?"
            "name=" + name +
            "&format=" + format +
            "&terminateOn=" + static_cast<std::string>(terminateOn) +
            ( beep ? "&beep=true" : "&beep=false" ) +
            ( ifExists.empty() ? "" : "&ifExists=" + ifExists ) +
            ( maxDurationSeconds < 0 ? "" : "&maxDurationSeconds=" + std::to_string(maxDurationSeconds) ) +
            ( maxSilenceSeconds < 0 ? "" : "&maxSilenceSeconds=" + std::to_string(maxSilenceSeconds) ),
            client
        );
    }

    Proxy& Destroy()
    {
        if ( IsDead() ) return Proxy::CreateEmpty();
        id.clear();
        return Proxy::Command("DELETE", "/ari/bridges/"+id, client);
    }

    bool IsDead() const { return id.empty(); }

private:

    std::string id;
    Client* client;
};

template<class Dummy> const Bridge::Role Bridge::RoleBase<Dummy>::announce{"announce"};
template<class Dummy> const Bridge::Role Bridge::RoleBase<Dummy>::partecipant{"partecipant"};

} // namespace aricpp

#endif

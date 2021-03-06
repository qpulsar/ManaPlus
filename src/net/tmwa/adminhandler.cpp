/*
 *  The ManaPlus Client
 *  Copyright (C) 2004-2009  The Mana World Development Team
 *  Copyright (C) 2009-2010  The Mana Developers
 *  Copyright (C) 2011-2015  The ManaPlus Developers
 *
 *  This file is part of The ManaPlus Client.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "net/tmwa/adminhandler.h"

#include "gui/chatconsts.h"

#include "net/chathandler.h"

#include "net/tmwa/messageout.h"
#include "net/tmwa/protocol.h"

#include "utils/stringutils.h"

#include "debug.h"

extern Net::AdminHandler *adminHandler;

namespace TmwAthena
{

AdminHandler::AdminHandler() :
    MessageHandler(),
    Ea::AdminHandler()
{
    static const uint16_t _messages[] =
    {
        SMSG_ADMIN_KICK_ACK,
        0
    };
    handledMessages = _messages;
    adminHandler = this;
}

void AdminHandler::handleMessage(Net::MessageIn &msg)
{
    BLOCK_START("AdminHandler::handleMessage")
    switch (msg.getId())
    {
        case SMSG_ADMIN_KICK_ACK:
            processKickAck(msg);
            break;

        default:
            break;
    }
    BLOCK_END("AdminHandler::handleMessage")
}

void AdminHandler::announce(const std::string &text) const
{
    createOutPacket(CMSG_ADMIN_ANNOUNCE);
    outMsg.writeInt16(static_cast<int16_t>(text.length() + 4), "len");
    outMsg.writeString(text, static_cast<int>(text.length()), "message");
}

void AdminHandler::localAnnounce(const std::string &text) const
{
    createOutPacket(CMSG_ADMIN_LOCAL_ANNOUNCE);
    outMsg.writeInt16(static_cast<int16_t>(text.length() + 4), "len");
    outMsg.writeString(text, static_cast<int>(text.length()), "message");
}

void AdminHandler::hide(const bool h A_UNUSED) const
{
    createOutPacket(CMSG_ADMIN_HIDE);
    outMsg.writeInt32(0, "unused");
}

void AdminHandler::kick(const BeingId playerId) const
{
    createOutPacket(CMSG_ADMIN_KICK);
    outMsg.writeBeingId(playerId, "account id");
}

void AdminHandler::kickAll() const
{
}

void AdminHandler::warp(const std::string &map, const int x, const int y) const
{
    chatHandler->talk(strprintf(
        "@warp %s %d %d", map.c_str(), x, y), GENERAL_CHANNEL);
}

void AdminHandler::resetStats() const
{
}

void AdminHandler::resetSkills() const
{
}

void AdminHandler::gotoName(const std::string &name) const
{
    chatHandler->talk("@goto " + name, GENERAL_CHANNEL);
}

void AdminHandler::recallName(const std::string &name) const
{
    chatHandler->talk("@recall " + name, GENERAL_CHANNEL);
}

void AdminHandler::mute(const Being *const being A_UNUSED,
                        const int type A_UNUSED,
                        const int limit A_UNUSED) const
{
    return;
}

void AdminHandler::muteName(const std::string &name A_UNUSED) const
{
}

void AdminHandler::requestLogin(const Being *const being A_UNUSED) const
{
}

void AdminHandler::setTileType(const int x A_UNUSED, const int y A_UNUSED,
                               const int type A_UNUSED) const
{
}

void AdminHandler::unequipAll(const Being *const being A_UNUSED) const
{
}

void AdminHandler::requestStats(const std::string &name A_UNUSED)
{
}

}  // namespace TmwAthena

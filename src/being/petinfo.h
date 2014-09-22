/*
 *  The ManaPlus Client
 *  Copyright (C) 2011-2014  The ManaPlus Developers
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

#ifndef BEING_PETINFO_H
#define BEING_PETINFO_H

#include "localconsts.h"

struct PetInfo final
{
    PetInfo() :
        name(),
        id(0),
        level(0),
        hungry(0),
        intimate(0),
        race(0)
    { }

    A_DELETE_COPY(PetInfo)

    std::string name;
    int id;
    int level;
    int hungry;
    int race;
};

#endif  // BEING_PETINFO_H
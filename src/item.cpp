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

#include "item.h"

#include "dragdrop.h"

#include "gui/theme.h"

#include "resources/iteminfo.h"
#include "resources/resourcemanager.h"
#include "configuration.h"

#include "net/serverfeatures.h"

#include "debug.h"

DragDrop dragDrop(nullptr, DRAGDROP_SOURCE_EMPTY);

Item::Item(const int id,
           const int type,
           const int quantity,
           const uint8_t refine,
           const unsigned char color,
           const Identified identified,
           const Damaged damaged,
           const Favorite favorite,
           const Equipm equipment,
           const Equipped equipped) :
    mId(0),
    mColor(0),
    mQuantity(quantity),
    mImage(nullptr),
    mDescription(),
    mTags(),
    mCards(),
    mRefine(refine),
    mInvIndex(0),
    mType(type),
    mEquipment(equipment),
    mEquipped(equipped),
    mInEquipment(false),
    mIdentified(identified),
    mDamaged(damaged),
    mFavorite(favorite)
{
    setId(id, color);
    for (int f = 0; f < 4; f ++)
        mCards[f] = 0;
}

Item::~Item()
{
    if (mImage)
    {
        mImage->decRef();
        mImage = nullptr;
    }
    dragDrop.clearItem(this);
}

void Item::setId(const int id, const unsigned char color)
{
    mId = id;
    mColor = color;

    // Types 0 and 1 are not equippable items.
    mEquipment = fromBool(id && static_cast<int>(getInfo().getType())
        >= 2, Equipm);

    if (mImage)
        mImage->decRef();

    ResourceManager *const resman = ResourceManager::getInstance();
    const ItemInfo &info = getInfo();
    mTags = info.getTags();

    const std::string dye = combineDye2(paths.getStringValue(
        "itemIcons").append(info.getDisplay().image),
        info.getDyeColorsString(color));
    mImage = resman->getImage(dye);

    if (!mImage)
    {
        mImage = Theme::getImageFromTheme(paths.getValue("unknownItemFile",
                                          "unknown-item.png"));
    }
}

bool Item::isHaveTag(const int tagId) const
{
    const std::map <int, int>::const_iterator it = mTags.find(tagId);
    if (it == mTags.end())
        return false;
    return (*it).second > 0;
}

Image *Item::getImage(const int id, const unsigned char color)
{
    ResourceManager *const resman = ResourceManager::getInstance();
    const ItemInfo &info = ItemDB::get(id);
    Image *image = resman->getImage(combineDye2(paths.getStringValue(
        "itemIcons").append(info.getDisplay().image),
        info.getDyeColorsString(color)));

    if (!image)
        image = Theme::getImageFromTheme("unknown-item.png");
    return image;
}

std::string Item::getName() const
{
    const ItemInfo &info = ItemDB::get(mId);
    if (serverFeatures->haveItemColors())
        return info.getName(mColor);
    else
        return info.getName();
}

void Item::setCard(const int index, const int id)
{
    if (index < 0 || index >= maxCards)
        return;
    mCards[index] = id;
}

int Item::getCard(const int index) const
{
    if (index < 0 || index >= maxCards)
        return 0;
    return mCards[index];
}

void Item::setCards(const int *const cards, const int size)
{
    if (size < 0 || !cards)
        return;
    int sz = size;
    if (sz > maxCards)
        sz = maxCards;
    for (int f = 0; f < sz; f ++)
        mCards[f] = cards[f];
}

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

#include "resources/map/map.h"

#include "configuration.h"
#include "render/graphics.h"
#include "notifymanager.h"
#include "settings.h"

#include "resources/map/mapheights.h"
#include "resources/map/maplayer.h"
#include "resources/map/mapitem.h"
#include "resources/map/objectslayer.h"
#include "resources/map/speciallayer.h"
#include "resources/map/tileset.h"
#include "resources/map/walklayer.h"

#include "being/localplayer.h"

#include "particle/particle.h"

#include "resources/ambientlayer.h"
#include "resources/mapitemtype.h"
#include "resources/notifytypes.h"
#include "resources/resourcemanager.h"
#include "resources/subimage.h"

#include "resources/map/location.h"
#include "resources/map/mapobjectlist.h"
#include "resources/map/tileanimation.h"

#ifdef USE_OPENGL
#include "render/renderers.h"
#endif

#include "utils/delete2.h"
#include "utils/dtor.h"
#include "utils/mkdir.h"
#include "utils/physfstools.h"
#include "utils/timer.h"

#include <queue>

#include <sys/stat.h>

#include <climits>

#include "debug.h"

class ActorFunctuator final
{
    public:
        bool operator()(const Actor *const a, const Actor *const b) const
        {
            if (!a || !b)
                return false;
            return a->getSortPixelY() < b->getSortPixelY();
        }
} actorCompare;

Map::Map(const int width, const int height,
         const int tileWidth, const int tileHeight) :
    Properties(),
    mWidth(width), mHeight(height),
    mTileWidth(tileWidth), mTileHeight(tileHeight),
    mMaxTileHeight(height),
    mMetaTiles(new MetaTile[mWidth * mHeight]),
    mWalkLayer(nullptr),
    mLayers(),
    mDrawUnderLayers(),
    mDrawOverLayers(),
    mTilesets(),
    mActors(),
    mHasWarps(false),
    mDrawLayersFlags(MapType::NORMAL),
    mOnClosedList(1),
    mOnOpenList(2),
    mBackgrounds(),
    mForegrounds(),
    mLastAScrollX(0.0F),
    mLastAScrollY(0.0F),
    mParticleEffects(),
    mMapPortals(),
    mTileAnimations(),
    mOverlayDetail(config.getIntValue("OverlayDetail")),
    mOpacity(config.getFloatValue("guialpha")),
#ifdef USE_OPENGL
    mOpenGL(intToRenderType(config.getIntValue("opengl"))),
#else
    mOpenGL(RENDER_SOFTWARE),
#endif
    mPvp(0),
    mTilesetsIndexed(false),
    mIndexedTilesets(nullptr),
    mIndexedTilesetsSize(0),
    mActorFixX(0),
    mActorFixY(0),
    mVersion(0),
    mSpecialLayer(new SpecialLayer(width, height)),
    mTempLayer(new SpecialLayer(width, height)),
    mObjects(new ObjectsLayer(width, height)),
    mFringeLayer(nullptr),
    mLastX(-1),
    mLastY(-1),
    mLastScrollX(-1),
    mLastScrollY(-1),
    mDrawX(-1),
    mDrawY(-1),
    mDrawScrollX(-1),
    mDrawScrollY(-1),
    mMask(1),
    mAtlas(nullptr),
    mHeights(nullptr),
    mRedrawMap(true),
    mBeingOpacity(false),
#ifdef USE_OPENGL
    mCachedDraw(mOpenGL == RENDER_NORMAL_OPENGL
        || mOpenGL == RENDER_GLES_OPENGL
        || mOpenGL == RENDER_MODERN_OPENGL),
#else
    mCachedDraw(false),
#endif
    mCustom(false),
    mDrawOnlyFringe(false)
{
    const int size = mWidth * mHeight;
    for (int i = 0; i < BlockType::NB_BLOCKTYPES; i++)
    {
        mOccupation[i] = new unsigned[static_cast<size_t>(size)];
        memset(mOccupation[i], 0, static_cast<size_t>(size)
            * sizeof(unsigned));
    }

    config.addListener("OverlayDetail", this);
    config.addListener("guialpha", this);
    config.addListener("beingopacity", this);

    if (mOpacity != 1.0F)
        mBeingOpacity = config.getBoolValue("beingopacity");
    else
        mBeingOpacity = false;
}

Map::~Map()
{
    config.removeListeners(this);
    CHECKLISTENERS

    delete [] mMetaTiles;
    for (int i = 0; i < BlockType::NB_BLOCKTYPES; i++)
        delete [] mOccupation[i];

    if (mWalkLayer)
    {
        mWalkLayer->decRef();
        mWalkLayer = nullptr;
    }
    mFringeLayer = nullptr;
    delete_all(mLayers);
    delete_all(mTilesets);
    delete_all(mForegrounds);
    delete_all(mBackgrounds);
    delete_all(mTileAnimations);
    delete2(mSpecialLayer);
    delete2(mTempLayer);
    delete2(mObjects);
    delete_all(mMapPortals);
    if (mAtlas)
    {
        mAtlas->decRef();
        mAtlas = nullptr;
    }
    delete2(mHeights);
}

void Map::optionChanged(const std::string &value)
{
    if (value == "OverlayDetail")
    {
        mOverlayDetail = config.getIntValue("OverlayDetail");
    }
    else if (value == "guialpha")
    {
        mOpacity = config.getFloatValue("guialpha");
        if (mOpacity != 1.0F)
            mBeingOpacity = config.getBoolValue("beingopacity");
        else
            mBeingOpacity = false;
    }
    else if (value == "beingopacity")
    {
        if (mOpacity != 1.0F)
            mBeingOpacity = config.getBoolValue("beingopacity");
        else
            mBeingOpacity = false;
    }
}

void Map::initializeAmbientLayers()
{
    ResourceManager *const resman = ResourceManager::getInstance();

    // search for "foreground*" or "overlay*" (old term) in map properties
    for (int i = 0; /* terminated by a break */; i++)
    {
        std::string name;
        if (hasProperty(std::string("foreground").append(
            toString(i)).append("image")))
        {
            name = "foreground" + toString(i);
        }
        else if (hasProperty(std::string("overlay").append(
                 toString(i)).append("image")))
        {
            name = "overlay" + toString(i);
        }
        else
        {
            break;  // the FOR loop
        }

        Image *const img = resman->getImage(getProperty(name + "image"));
        if (img)
        {
            int mask = atoi(getProperty(name + "mask").c_str());
            if (!mask)
                mask = 1;
            const float parallax = getFloatProperty(name + "parallax");
            mForegrounds.push_back(new AmbientLayer(img,
                getFloatProperty(name + "parallaxX", parallax),
                getFloatProperty(name + "parallaxY", parallax),
                getFloatProperty(name + "posX"),
                getFloatProperty(name + "posY"),
                getFloatProperty(name + "scrollX"),
                getFloatProperty(name + "scrollY"),
                getBoolProperty(name + "keepratio"),
                mask));

            // The AmbientLayer takes control over the image.
            img->decRef();
        }
    }

    // search for "background*" in map properties
    for (int i = 0; hasProperty(std::string("background").append(
         toString(i)).append("image")); i ++)
    {
        const std::string name("background" + toString(i));
        Image *const img = resman->getImage(getProperty(name + "image"));

        if (img)
        {
            int mask = atoi(getProperty(name + "mask").c_str());
            if (!mask)
                mask = 1;

            const float parallax = getFloatProperty(name + "parallax");
            mBackgrounds.push_back(new AmbientLayer(img,
                getFloatProperty(name + "parallaxX", parallax),
                getFloatProperty(name + "parallaxY", parallax),
                getFloatProperty(name + "posX"),
                getFloatProperty(name + "posY"),
                getFloatProperty(name + "scrollX"),
                getFloatProperty(name + "scrollY"),
                getBoolProperty(name + "keepratio"),
                mask));

            // The AmbientLayer takes control over the image.
            img->decRef();
        }
    }
}

void Map::addLayer(MapLayer *const layer)
{
    if (layer)
    {
        mLayers.push_back(layer);
        if (layer->isFringeLayer() && !mFringeLayer)
            mFringeLayer = layer;
    }
}

void Map::addTileset(Tileset *const tileset)
{
    if (!tileset)
        return;

    mTilesets.push_back(tileset);
    const int height = tileset->getHeight();
    if (height > mMaxTileHeight)
        mMaxTileHeight = height;
}

void Map::update(const int ticks)
{
    // Update animated tiles
    FOR_EACH (TileAnimationMapCIter, iAni, mTileAnimations)
    {
        TileAnimation *const tileAni = iAni->second;
        if (tileAni && tileAni->update(ticks))
            mRedrawMap = true;
    }
}

void Map::draw(Graphics *const graphics, int scrollX, int scrollY)
{
    if (!localPlayer)
        return;

    BLOCK_START("Map::draw")
    // Calculate range of tiles which are on-screen
    const int endPixelY = graphics->mHeight + scrollY + mTileHeight - 1
        + mMaxTileHeight - mTileHeight;
    const int startX = scrollX / mTileWidth - 2;
    const int startY = scrollY / mTileHeight;
    const int endX = (graphics->mWidth + scrollX + mTileWidth - 1)
        / mTileWidth + 1;
    const int endY = endPixelY / mTileHeight + 1;

    // Make sure actors are sorted ascending by Y-coordinate
    // so that they overlap correctly
    BLOCK_START("Map::draw sort")
    mActors.sort(actorCompare);
    BLOCK_END("Map::draw sort")

    // update scrolling of all ambient layers
    updateAmbientLayers(static_cast<float>(scrollX),
                        static_cast<float>(scrollY));

    // Draw backgrounds
    drawAmbientLayers(graphics, BACKGROUND_LAYERS, mOverlayDetail);

    if (mDrawLayersFlags == MapType::BLACKWHITE && userPalette)
    {
        graphics->setColor(userPalette->getColorWithAlpha(
            UserColorId::WALKABLE_HIGHLIGHT));

        graphics->fillRectangle(Rect(0, 0,
            graphics->mWidth, graphics->mHeight));
    }

#ifdef USE_OPENGL
    int updateFlag = 0;

    if (mCachedDraw)
    {
        if (mLastX != startX || mLastY != startY || mLastScrollX != scrollX
            || mLastScrollY != scrollY)
        {   // player moving
            mLastX = startX;
            mLastY = startY;
            mLastScrollX = scrollX;
            mLastScrollY = scrollY;
            updateFlag = 2;
        }
        else if (mRedrawMap || startX != mDrawX || startY != mDrawY ||
                 scrollX != mDrawScrollX || scrollY != mDrawScrollY)
        {   // player mode to new position
            mRedrawMap = false;
            mDrawX = startX;
            mDrawY = startY;
            mDrawScrollX = scrollX;
            mDrawScrollY = scrollY;
            updateFlag = 1;
        }
    }
#endif

    if (mDrawOnlyFringe)
    {
        if (mFringeLayer)
        {
            mFringeLayer->setSpecialLayer(mSpecialLayer);
            mFringeLayer->setTempLayer(mTempLayer);
            mFringeLayer->drawFringe(graphics,
                startX, startY,
                endX, endY,
                scrollX, scrollY,
                &mActors);
        }
    }
    else
    {
#ifdef USE_OPENGL
        if (mCachedDraw)
        {
            if (updateFlag)
            {
                FOR_EACH (Layers::iterator, it, mDrawUnderLayers)
                {
                    (*it)->updateOGL(graphics,
                        startX, startY,
                        endX, endY,
                        scrollX, scrollY);
                }
                FOR_EACH (Layers::iterator, it, mDrawOverLayers)
                {
                    (*it)->updateOGL(graphics,
                        startX, startY,
                        endX, endY,
                        scrollX, scrollY);
                }
            }

            FOR_EACH (Layers::iterator, it, mDrawUnderLayers)
                (*it)->drawOGL(graphics);

            if (mFringeLayer)
            {
                mFringeLayer->setSpecialLayer(mSpecialLayer);
                mFringeLayer->setTempLayer(mTempLayer);
                mFringeLayer->drawFringe(graphics,
                    startX, startY,
                    endX, endY,
                    scrollX, scrollY,
                    &mActors);
            }

            FOR_EACH (Layers::iterator, it, mDrawOverLayers)
                (*it)->drawOGL(graphics);
        }
        else
#endif
        {
            FOR_EACH (Layers::iterator, it, mDrawUnderLayers)
            {
                (*it)->draw(graphics,
                    startX, startY,
                    endX, endY,
                    scrollX, scrollY);
            }

            if (mFringeLayer)
            {
                mFringeLayer->setSpecialLayer(mSpecialLayer);
                mFringeLayer->setTempLayer(mTempLayer);
                mFringeLayer->drawFringe(graphics,
                    startX, startY,
                    endX, endY,
                    scrollX, scrollY,
                    &mActors);
            }

            FOR_EACH (Layers::iterator, it, mDrawOverLayers)
            {
                (*it)->draw(graphics, startX, startY,
                    endX, endY,
                    scrollX, scrollY);
            }
        }
    }

    // Don't draw if gui opacity == 1
    if (mBeingOpacity)
    {
        // Draws beings with a lower opacity to make them visible
        // even when covered by a wall or some other elements...
        ActorsCIter ai = mActors.begin();
        const ActorsCIter ai_end = mActors.end();

        if (mOpenGL == RENDER_SOFTWARE)
        {
            while (ai != ai_end)
            {
                if (Actor *const actor = *ai)
                {
                    const int x = actor->getTileX();
                    const int y = actor->getTileY();
                    if (x < startX || x > endX || y < startY || y > endY)
                    {
                        ++ai;
                        continue;
                    }
                    // For now, just draw actors with only one layer.
                    if (actor->getNumberOfLayers() == 1)
                    {
                        actor->setAlpha(0.3F);
                        actor->draw(graphics, -scrollX, -scrollY);
                        actor->setAlpha(1.0F);
                    }
                }
                ++ai;
            }
        }
        else
        {
            while (ai != ai_end)
            {
                if (Actor *const actor = *ai)
                {
                    // For now, just draw actors with only one layer.
                    if (actor->getNumberOfLayers() == 1)
                    {
                        actor->setAlpha(0.3F);
                        actor->draw(graphics, -scrollX, -scrollY);
                        actor->setAlpha(1.0F);
                    }
                }
                ++ai;
            }
        }
    }

    drawAmbientLayers(graphics, FOREGROUND_LAYERS, mOverlayDetail);
    BLOCK_END("Map::draw")
}

#define fillCollision(collision, color) \
    if (x < endX && mMetaTiles[tilePtr].blockmask & collision)\
    {\
        width = mapTileSize;\
        for (int x2 = tilePtr + 1; x < endX; x2 ++)\
        {\
            if (!(mMetaTiles[x2].blockmask & collision))\
                break;\
            width += mapTileSize;\
            x ++;\
            tilePtr ++;\
        }\
        if (width && userPalette)\
        {\
            graphics->setColor(userPalette->getColorWithAlpha(\
                UserColorId::color));\
            graphics->fillRectangle(Rect(\
                x0 * mTileWidth - scrollX, \
                y * mTileHeight - scrollY, \
                width, mapTileSize));\
        }\
    }\

void Map::drawCollision(Graphics *const graphics,
                        const int scrollX, const int scrollY,
                        const MapType::MapType drawFlags) const
{
    const int endPixelY = graphics->mHeight + scrollY + mTileHeight - 1;
    int startX = scrollX / mTileWidth;
    int startY = scrollY / mTileHeight;
    int endX = (graphics->mWidth + scrollX + mTileWidth - 1) / mTileWidth;
    int endY = endPixelY / mTileHeight;

    if (startX < 0)
        startX = 0;
    if (startY < 0)
        startY = 0;
    if (endX > mWidth)
        endX = mWidth;
    if (endY > mHeight)
        endY = mHeight;

    if (drawFlags < MapType::SPECIAL)
    {
        graphics->setColor(userPalette->getColorWithAlpha(UserColorId::NET));
        graphics->drawNet(
            startX * mTileWidth - scrollX,
            startY * mTileHeight - scrollY,
            endX * mTileWidth - scrollX,
            endY * mTileHeight - scrollY,
            mapTileSize, mapTileSize);
    }

    for (int y = startY; y < endY; y++)
    {
        const int yWidth = y * mWidth;
        int tilePtr = startX + yWidth;
        for (int x = startX; x < endX; x++, tilePtr++)
        {
            int width = 0;
            const int x0 = x;

            fillCollision(BlockMask::WALL, COLLISION_HIGHLIGHT);
            fillCollision(BlockMask::AIR, AIR_COLLISION_HIGHLIGHT);
            fillCollision(BlockMask::WATER, WATER_COLLISION_HIGHLIGHT);
            fillCollision(BlockMask::GROUNDTOP, GROUNDTOP_COLLISION_HIGHLIGHT);
        }
    }
}

void Map::updateAmbientLayers(const float scrollX, const float scrollY)
{
    BLOCK_START("Map::updateAmbientLayers")
    static int lastTick = tick_time;

    if (mLastAScrollX == 0.0F && mLastAScrollY == 0.0F)
    {
        // First call - initialisation
        mLastAScrollX = scrollX;
        mLastAScrollY = scrollY;
    }

    // Update Overlays
    const float dx = scrollX - mLastAScrollX;
    const float dy = scrollY - mLastAScrollY;
    const int timePassed = get_elapsed_time(lastTick);

    // need check mask to update or not to update

    FOR_EACH (AmbientLayerVectorIter, i, mBackgrounds)
    {
        AmbientLayer *const layer = *i;
        if (layer && (layer->mMask & mMask))
            layer->update(timePassed, dx, dy);
    }

    FOR_EACH (AmbientLayerVectorIter, i, mForegrounds)
    {
        AmbientLayer *const layer = *i;
        if (layer && (layer->mMask & mMask))
            layer->update(timePassed, dx, dy);
    }

    mLastAScrollX = scrollX;
    mLastAScrollY = scrollY;
    lastTick = tick_time;
    BLOCK_END("Map::updateAmbientLayers")
}

void Map::drawAmbientLayers(Graphics *const graphics,
                            const LayerType type,
                            const int detail) const
{
    BLOCK_START("Map::drawAmbientLayers")
    // Detail 0 = no ambient effects except background image
    if (detail <= 0 && type != BACKGROUND_LAYERS)
    {
        BLOCK_END("Map::drawAmbientLayers")
        return;
    }

    // find out which layer list to draw
    const AmbientLayerVector *layers = nullptr;
    switch (type)
    {
        case FOREGROUND_LAYERS:
            layers = &mForegrounds;
            break;
        case BACKGROUND_LAYERS:
            layers = &mBackgrounds;
            break;
        default:
            return;
    }

    // Draw overlays
    FOR_EACHP (AmbientLayerVectorCIter, i, layers)
    {
        const AmbientLayer *const layer = *i;
        // need check mask to draw or not to draw
        if (layer && (layer->mMask & mMask))
            (layer)->draw(graphics, graphics->mWidth, graphics->mHeight);

        // Detail 1: only one overlay, higher: all overlays
        if (detail == 1)
            break;
    }
    BLOCK_END("Map::drawAmbientLayers")
}

const Tileset *Map::getTilesetWithGid(const int gid) const
{
    if (gid >= 0 && gid < mIndexedTilesetsSize)
        return mIndexedTilesets[gid];
    else
        return nullptr;
}

void Map::blockTile(const int x, const int y,
                    const BlockType::BlockType type)
{
    if (type == BlockType::NONE || !contains(x, y))
        return;

    const int tileNum = x + y * mWidth;

    if (mOccupation[static_cast<size_t>(type)][tileNum] < UINT_MAX &&
        (++mOccupation[static_cast<size_t>(type)][tileNum]) > 0)
    {
        switch (type)
        {
            case BlockType::WALL:
                mMetaTiles[tileNum].blockmask |= BlockMask::WALL;
                break;
            case BlockType::CHARACTER:
                mMetaTiles[tileNum].blockmask |= BlockMask::CHARACTER;
                break;
            case BlockType::MONSTER:
                mMetaTiles[tileNum].blockmask |= BlockMask::MONSTER;
                break;
            case BlockType::AIR:
                mMetaTiles[tileNum].blockmask |= BlockMask::AIR;
                break;
            case BlockType::WATER:
                mMetaTiles[tileNum].blockmask |= BlockMask::WATER;
                break;
            case BlockType::GROUND:
                mMetaTiles[tileNum].blockmask |= BlockMask::GROUND;
                break;
            case BlockType::GROUNDTOP:
                mMetaTiles[tileNum].blockmask |= BlockMask::GROUNDTOP;
                break;
            default:
            case BlockType::NONE:
            case BlockType::NB_BLOCKTYPES:
                // Do nothing.
                break;
        }
    }
}

bool Map::getWalk(const int x, const int y,
                  const unsigned char blockWalkMask) const
{
    // You can't walk outside of the map
    if (x < 0 || y < 0 || x >= mWidth || y >= mHeight)
        return false;

    // Check if the tile is walkable
    return !(mMetaTiles[x + y * mWidth].blockmask & blockWalkMask);
}

unsigned char Map::getBlockMask(const int x, const int y) const
{
    // You can't walk outside of the map
    if (x < 0 || y < 0 || x >= mWidth || y >= mHeight)
        return 0;

    // Check if the tile is walkable
    return mMetaTiles[x + y * mWidth].blockmask;
}

void Map::setWalk(const int x, const int y, const bool walkable A_UNUSED)
{
    blockTile(x, y, BlockType::GROUNDTOP);
}

bool Map::contains(const int x, const int y) const
{
    return x >= 0 && y >= 0 && x < mWidth && y < mHeight;
}

const MetaTile *Map::getMetaTile(const int x, const int y) const
{
    return &mMetaTiles[x + y * mWidth];
}

Actors::iterator Map::addActor(Actor *const actor)
{
    mActors.push_front(actor);
//    mSpritesUpdated = true;
    return mActors.begin();
}

void Map::removeActor(const Actors::iterator &iterator)
{
    mActors.erase(iterator);
//    mSpritesUpdated = true;
}

const std::string Map::getMusicFile() const
{
    return getProperty("music");
}

const std::string Map::getName() const
{
    if (hasProperty("name"))
        return getProperty("name");

    return getProperty("mapname");
}

const std::string Map::getFilename() const
{
    const std::string fileName = getProperty("_filename");
    const size_t lastSlash = fileName.rfind("/") + 1;
    return fileName.substr(lastSlash, fileName.rfind(".") - lastSlash);
}

Path Map::findPath(const int startX, const int startY,
                   const int destX, const int destY,
                   const unsigned char blockWalkMask,
                   const int maxCost)
{
    BLOCK_START("Map::findPath")
    // The basic walking cost of a tile.
    static const int basicCost = 100;
    const int basicCost2 = 100 * 362 / 256;
    const float basicCostF = 100.0 * 362 / 256;

    // Path to be built up (empty by default)
    Path path;

    if (startX >= mWidth || startY >= mHeight || startX < 0 || startY < 0)
    {
        BLOCK_END("Map::findPath")
        return path;
    }

    // Return when destination not walkable
    if (!getWalk(destX, destY, blockWalkMask))
    {
        BLOCK_END("Map::findPath")
        return path;
    }

    // Reset starting tile's G cost to 0
    MetaTile *const startTile = &mMetaTiles[startX + startY * mWidth];
    if (!startTile)
    {
        BLOCK_END("Map::findPath")
        return path;
    }

    startTile->Gcost = 0;

    // Declare open list, a list with open tiles sorted on F cost
    std::priority_queue<Location> openList;

    // Add the start point to the open list
    openList.push(Location(startX, startY, startTile));

    bool foundPath = false;

    // Keep trying new open tiles until no more tiles to try or target found
    while (!openList.empty() && !foundPath)
    {
        // Take the location with the lowest F cost from the open list.
        const Location curr = openList.top();
        openList.pop();

        const MetaTile *const tile = curr.tile;

        // If the tile is already on the closed list, this means it has already
        // been processed with a shorter path to the start point (lower G cost)
        if (tile->whichList == mOnClosedList)
            continue;

        // Put the current tile on the closed list
        curr.tile->whichList = mOnClosedList;

        const int curWidth = curr.y * mWidth;
        const int tileGcost = tile->Gcost;

        // Check the adjacent tiles
        for (int dy = -1; dy <= 1; dy++)
        {
            const int y = curr.y + dy;
            if (y < 0 || y >= mHeight)
                continue;

            const int yWidth = y * mWidth;
            const int dy1 = std::abs(y - destY);

            for (int dx = -1; dx <= 1; dx++)
            {
                // Calculate location of tile to check
                const int x = curr.x + dx;

                // Skip if if we're checking the same tile we're leaving from,
                // or if the new location falls outside of the map boundaries
                if ((dx == 0 && dy == 0) || x < 0 || x >= mWidth)
                    continue;

                MetaTile *const newTile = &mMetaTiles[x + yWidth];

                // Skip if the tile is on the closed list or is not walkable
                // unless its the destination tile
                // +++ here need check block must depend on player abilities.
                if (newTile->whichList == mOnClosedList ||
                    ((newTile->blockmask & blockWalkMask)
                    && !(x == destX && y == destY))
                    || (newTile->blockmask & BlockMask::WALL))
                {
                    continue;
                }

                // When taking a diagonal step, verify that we can skip the
                // corner.
                if (dx != 0 && dy != 0)
                {
                    const MetaTile *const t1 = &mMetaTiles[curr.x +
                        (curr.y + dy) * mWidth];
                    const MetaTile *const t2 = &mMetaTiles[curr.x +
                        dx + curWidth];

                    // +++ here need check block must depend
                    // on player abilities.
                    if (((t1->blockmask | t2->blockmask) & BlockMask::WALL))
                        continue;
                }

                // Calculate G cost for this route, ~sqrt(2) for moving diagonal
                int Gcost = tileGcost + (dx == 0 || dy == 0
                    ? basicCost : basicCost2);

                /* Demote an arbitrary direction to speed pathfinding by
                   adding a defect (TODO: change depending on the desired
                   visual effect, e.g. a cross-product defect toward
                   destination).
                   Important: as long as the total defect along any path is
                   less than the basicCost, the pathfinder will still find one
                   of the shortest paths! */
                if (dx == 0 || dy == 0)
                {
                    // Demote horizontal and vertical directions, so that two
                    // consecutive directions cannot have the same Fcost.
                    ++Gcost;
                }

                // It costs extra to walk through a being (needs to be enough
                // to make it more attractive to walk around).
//                if (occupied(x, y))
//                {
//                    Gcost += 3 * basicCost;
//                }

                // Skip if Gcost becomes too much
                // Warning: probably not entirely accurate
                if (maxCost > 0 && Gcost > maxCost * basicCost)
                    continue;

                if (newTile->whichList != mOnOpenList)
                {
                    // Found a new tile (not on open nor on closed list)

                    /* Update Hcost of the new tile. The pathfinder does not
                       work reliably if the heuristic cost is higher than the
                       real cost. In particular, using Manhattan distance is
                       forbidden here. */
                    const int dx1 = std::abs(x - destX);
                    newTile->Hcost = std::abs(dx1 - dy1) * basicCost +
                        std::min(dx1, dy1) * (basicCostF);

                    // Set the current tile as the parent of the new tile
                    newTile->parentX = curr.x;
                    newTile->parentY = curr.y;

                    // Update Gcost and Fcost of new tile
                    newTile->Gcost = Gcost;
                    newTile->Fcost = Gcost + newTile->Hcost;

                    if (x != destX || y != destY)
                    {
                        // Add this tile to the open list
                        newTile->whichList = mOnOpenList;
                        openList.push(Location(x, y, newTile));
                    }
                    else
                    {
                        // Target location was found
                        foundPath = true;
                    }
                }
                else if (Gcost < newTile->Gcost)
                {
                    // Found a shorter route.
                    // Update Gcost and Fcost of the new tile
                    newTile->Gcost = Gcost;
                    newTile->Fcost = Gcost + newTile->Hcost;

                    // Set the current tile as the parent of the new tile
                    newTile->parentX = curr.x;
                    newTile->parentY = curr.y;

                    // Add this tile to the open list (it's already
                    // there, but this instance has a lower F score)
                    openList.push(Location(x, y, newTile));
                }
            }
        }
    }

    // Two new values to indicate whether a tile is on the open or closed list,
    // this way we don't have to clear all the values between each pathfinding.
    if (mOnOpenList > UINT_MAX - 2)
    {
        // We reset the list memebers value.
        mOnClosedList = 1;
        mOnOpenList = 2;

        // Clean up the metaTiles
        const int size = mWidth * mHeight;
        for (int i = 0; i < size; ++i)
            mMetaTiles[i].whichList = 0;
    }
    else
    {
        mOnClosedList += 2;
        mOnOpenList += 2;
    }

    // If a path has been found, iterate backwards using the parent locations
    // to extract it.
    if (foundPath)
    {
        int pathX = destX;
        int pathY = destY;

        while (pathX != startX || pathY != startY)
        {
            // Add the new path node to the start of the path list
            path.push_front(Position(pathX, pathY));

            // Find out the next parent
            const MetaTile *const tile = &mMetaTiles[pathX + pathY * mWidth];
            pathX = tile->parentX;
            pathY = tile->parentY;
        }
    }

    BLOCK_END("Map::findPath")
    return path;
}

void Map::addParticleEffect(const std::string &effectFile,
                            const int x, const int y,
                            const int w, const int h)
{
    ParticleEffectData newEffect(effectFile, x, y, w, h);
    mParticleEffects.push_back(newEffect);
}

void Map::initializeParticleEffects(Particle *const engine) const
{
    BLOCK_START("Map::initializeParticleEffects")
    if (!engine)
    {
        BLOCK_END("Map::initializeParticleEffects")
        return;
    }

    if (config.getBoolValue("particleeffects"))
    {
        for (std::vector<ParticleEffectData>::const_iterator
             i = mParticleEffects.begin();
             i != mParticleEffects.end(); ++i)
        {
            Particle *const p = engine->addEffect(i->file, i->x, i->y);
            if (p && i->w > 0 && i->h > 0)
                p->adjustEmitterSize(i->w, i->h);
        }
    }
     BLOCK_END("Map::initializeParticleEffects")
}

void Map::addExtraLayer()
{
    BLOCK_START("Map::addExtraLayer")
    if (!mSpecialLayer)
    {
        logger->log1("No special layer");
        BLOCK_END("Map::addExtraLayer")
        return;
    }
    const std::string mapFileName = getUserMapDirectory().append(
        "/extralayer.txt");
    logger->log("loading extra layer: " + mapFileName);
    struct stat statbuf;
    if (!stat(mapFileName.c_str(), &statbuf) && S_ISREG(statbuf.st_mode))
    {
        std::ifstream mapFile;
        mapFile.open(mapFileName.c_str(), std::ios::in);
        if (!mapFile.is_open())
        {
            mapFile.close();
            BLOCK_END("Map::addExtraLayer")
            return;
        }
        char line[201];

        while (mapFile.getline(line, 200))
        {
            std::string buf;
            std::string str = line;
            if (!str.empty())
            {
                std::string x;
                std::string y;
                std::string type1;
                std::string comment;
                std::stringstream ss(str);
                ss >> x;
                ss >> y;
                ss >> type1;
                ss >> comment;
                while (ss >> buf)
                    comment.append(" ").append(buf);

                const int type = atoi(type1.c_str());

                if (comment.empty())
                {
                    if (type < MapItemType::ARROW_UP
                        || type > MapItemType::ARROW_RIGHT)
                    {
                        comment = "unknown";
                    }
                }
                if (type == MapItemType::PORTAL)
                {
                    updatePortalTile(comment, type, atoi(x.c_str()),
                                     atoi(y.c_str()), false);
                }
                else if (type == MapItemType::HOME)
                {
                    updatePortalTile(comment, type, atoi(x.c_str()),
                                     atoi(y.c_str()));
                }
                else
                {
                    addPortalTile(comment, type, atoi(x.c_str()),
                                  atoi(y.c_str()));
                }
            }
        }
        mapFile.close();
    }
    BLOCK_END("Map::addExtraLayer")
}

void Map::saveExtraLayer() const
{
    if (!mSpecialLayer)
    {
        logger->log1("No special layer");
        return;
    }
    const std::string mapFileName = getUserMapDirectory().append(
        "/extralayer.txt");
    logger->log("saving extra layer: " + mapFileName);

    if (mkdir_r(getUserMapDirectory().c_str()))
    {
        logger->log(strprintf("%s doesn't exist and can't be created! "
                              "Exiting.", getUserMapDirectory().c_str()));
        return;
    }

    std::ofstream mapFile;
    mapFile.open(mapFileName.c_str(), std::ios::binary);
    if (!mapFile.is_open())
    {
        logger->log1("Unable to open extralayer.txt for writing");
        return;
    }

    const int width = mSpecialLayer->mWidth;
    const int height = mSpecialLayer->mHeight;

    for (int x = 0; x < width; x ++)
    {
        for (int y = 0; y < height; y ++)
        {
            const MapItem *const item = mSpecialLayer->getTile(x, y);
            if (item && item->mType != MapItemType::EMPTY
                && item->mType != MapItemType::HOME)
            {
                mapFile << x << " " << y << " "
                    << static_cast<int>(item->mType) << " "
                    << item->mComment << std::endl;
            }
        }
    }
    mapFile.close();
}

std::string Map::getUserMapDirectory() const
{
    return settings.serverConfigDir +
        dirSeparator + getProperty("_realfilename");
}

void Map::addRange(const std::string &name, const int type,
                   const int x, const int y,
                   const int dx, const int dy)
{
    if (!mObjects)
        return;

    mObjects->addObject(name, type, x / mapTileSize, y / mapTileSize,
        dx / mapTileSize, dy / mapTileSize);
}

void Map::addPortal(const std::string &name, const int type,
                    const int x, const int y,
                    const int dx, const int dy)
{
    addPortalTile(name, type, (x / mapTileSize) + (dx / mapTileSize / 2),
        (y / mapTileSize) + (dy / mapTileSize / 2));
}

void Map::addPortalTile(const std::string &name, const int type,
                        const int x, const int y)
{
    if (mSpecialLayer)
        mSpecialLayer->setTile(x, y, new MapItem(type, name, x, y));

    mMapPortals.push_back(new MapItem(type, name, x, y));
}

void Map::updatePortalTile(const std::string &name, const int type,
                           const int x, const int y, const bool addNew)
{
    MapItem *item = findPortalXY(x, y);
    if (item)
    {
        item->mComment = name;
        item->setType(type);
        item->mX = x;
        item->mY = y;
        if (mSpecialLayer)
        {
            item = new MapItem(type, name, x, y);
            mSpecialLayer->setTile(x, y, item);
        }
    }
    else if (addNew)
    {
        addPortalTile(name, type, x, y);
    }
}

MapItem *Map::findPortalXY(const int x, const int y) const
{
    FOR_EACH (std::vector<MapItem*>::const_iterator, it, mMapPortals)
    {
        if (!*it)
            continue;

        MapItem *const item = *it;
        if (item->mX == x && item->mY == y)
            return item;
    }
    return nullptr;
}

const TileAnimation *Map::getAnimationForGid(const int gid) const
{
    if (mTileAnimations.empty())
        return nullptr;

    const TileAnimationMapCIter i = mTileAnimations.find(gid);
    return (i == mTileAnimations.end()) ? nullptr : i->second;
}

void Map::setPvpMode(const int mode)
{
    const int oldMode = mPvp;

    if (!mode)
        mPvp = 0;
    else
        mPvp |= mode;

    if (mPvp != oldMode && localPlayer)
    {
        switch (mPvp)
        {
            case 0:
                NotifyManager::notify(NotifyTypes::PVP_OFF_GVG_OFF);
                break;
            case 1:
                NotifyManager::notify(NotifyTypes::PVP_ON);
                break;
            case 2:
                NotifyManager::notify(NotifyTypes::GVG_ON);
                break;
            case 3:
                NotifyManager::notify(NotifyTypes::PVP_ON_GVG_ON);
                break;
            default:
                NotifyManager::notify(NotifyTypes::PVP_UNKNOWN);
                break;
        }
    }
}

std::string Map::getObjectData(const unsigned x, const unsigned y,
                               const int type) const
{
    if (!mObjects)
        return "";

    MapObjectList *const list = mObjects->getAt(x, y);
    if (!list)
        return "";

    std::vector<MapObject>::const_iterator it = list->objects.begin();
    const std::vector<MapObject>::const_iterator it_end = list->objects.end();
    while (it != it_end)
    {
        if ((*it).type == type)
            return (*it).data;
        ++ it;
    }

    return "";
}

void Map::indexTilesets()
{
    if (mTilesetsIndexed)
        return;

    mTilesetsIndexed = true;

    const Tileset *s = nullptr;
    size_t sSz = 0;
    FOR_EACH (Tilesets::const_iterator, it, mTilesets)
    {
        const size_t sz = (*it)->size();
        if (!s || static_cast<size_t>(s->getFirstGid()) + sSz
            < static_cast<size_t>((*it)->getFirstGid()) + sz)
        {
            s = *it;
            sSz = sz;
        }
    }
    if (!s)
    {
        mIndexedTilesetsSize = 0;
        mIndexedTilesets = nullptr;
        return;
    }

    const int size = static_cast<int>(s->getFirstGid())
        + static_cast<int>(s->size());
    mIndexedTilesetsSize = size;
    mIndexedTilesets = new Tileset*[static_cast<size_t>(size)];
    std::fill_n(mIndexedTilesets, size, static_cast<Tileset*>(nullptr));

    FOR_EACH (Tilesets::const_iterator, it, mTilesets)
    {
        Tileset *const s2 = *it;
        if (s2)
        {
            const int start = s2->getFirstGid();
            const int end = start + static_cast<int>(s2->size());
            for (int f = start; f < end; f ++)
            {
                if (f < size)
                    mIndexedTilesets[f] = s2;
            }
        }
    }
}

void Map::clearIndexedTilesets()
{
    if (!mTilesetsIndexed)
        return;

    mTilesetsIndexed = false;
    delete [] mIndexedTilesets;
    mIndexedTilesetsSize = 0;
}

void Map::reduce()
{
#ifdef USE_SDL2
    return;
#else
    if (!mFringeLayer || mOpenGL != RENDER_SOFTWARE ||
        !config.getBoolValue("enableMapReduce"))
    {
        return;
    }

    int cnt = 0;
    for (int x = 0; x < mWidth; x ++)
    {
        for (int y = 0; y < mHeight; y ++)
        {
            bool correct(true);
            bool dontHaveAlpha(false);

            FOR_EACH (LayersCIter, layeri, mLayers)
            {
                const MapLayer *const layer = *layeri;
                if (x >= layer->mWidth || y >= layer->mHeight)
                    continue;

                Image *const img = layer->mTiles[x + y * layer->mWidth];
                if (img)
                {
                    if (img->hasAlphaChannel() && img->isAlphaCalculated())
                    {
                        if (!img->isAlphaVisible())
                        {
                            dontHaveAlpha = true;
                            img->setAlphaVisible(false);
                        }
                    }
                    else if (img->mBounds.w > mapTileSize
                             || img->mBounds.h > mapTileSize)
                    {
                        correct = false;
                        img->setAlphaVisible(true);
                        break;
                    }
                    else if (!img->isHasAlphaChannel())
                    {
                        dontHaveAlpha = true;
                        img->setAlphaVisible(false);
                    }
                    else if (img->hasAlphaChannel())
                    {
                        const uint8_t *const arr = img->SDLgetAlphaChannel();
                        if (!arr)
                            continue;

                        bool bad(false);
                        bool stop(false);
                        int width;
                        const SubImage *const subImg
                            = dynamic_cast<SubImage*>(img);
                        if (subImg)
                            width = subImg->mInternalBounds.w;
                        else
                            width = img->mBounds.w;

                        for (int f = img->mBounds.x;
                             f < img->mBounds.x + img->mBounds.w; f ++)
                        {
                            for (int d = img->mBounds.y;
                                 d < img->mBounds.y + img->mBounds.h; d ++)
                            {
                                const uint8_t chan = arr[f + d * width];
                                if (chan != 255)
                                {
                                    bad = true;
                                    stop = true;
                                    break;
                                }
                            }
                            if (stop)
                                break;
                        }
                        if (!bad)
                        {
                            dontHaveAlpha = true;
                            img->setAlphaVisible(false);
                        }
                        else
                        {
                            img->setAlphaVisible(true);
                        }
                    }
                    img->setAlphaCalculated(true);
                }
            }
            if (!correct || !dontHaveAlpha)
                continue;

            Layers::reverse_iterator ri = mLayers.rbegin();
            while (ri != mLayers.rend())
            {
                const MapLayer *const layer = *ri;
                if (x >= layer->mWidth || y >= layer->mHeight)
                {
                    ++ ri;
                    continue;
                }

                const Image *img = layer->mTiles[x + y * layer->mWidth];
                if (img && !img->isAlphaVisible())
                {   // removing all down tiles
                    ++ ri;
                    while (ri != mLayers.rend())
                    {
                        MapLayer *const layer2 = *ri;
                        const size_t pos = static_cast<size_t>(
                            x + y * layer2->mWidth);
                        img = layer2->mTiles[pos];
                        if (img)
                        {
                            layer2->mTiles[pos] = nullptr;
                            cnt ++;
                        }
                        ++ ri;
                    }
                    break;
                }
                ++ ri;
            }
        }
    }
    logger->log("tiles reduced: %d", cnt);
#endif
}

void Map::redrawMap()
{
    mRedrawMap = true;
}

void Map::addHeights(MapHeights *const heights)
{
    delete mHeights;
    mHeights = heights;
}

uint8_t Map::getHeightOffset(const int x, const int y) const
{
    if (!mHeights)
        return 0;
    return mHeights->getHeight(x, y);
}

void Map::updateDrawLayersList()
{
    mDrawUnderLayers.clear();
    mDrawOverLayers.clear();

    if (mDrawOnlyFringe)
        return;

    LayersCIter layers = mLayers.begin();
    const LayersCIter layers_end = mLayers.end();
    for (; layers != layers_end; ++ layers)
    {
        MapLayer *const layer = *layers;
        if (!(layer->mMask & mMask))
            continue;

        if (layer->isFringeLayer())
        {
            ++ layers;
            break;
        }

        mDrawUnderLayers.push_back(layer);
    }

    if (mDrawLayersFlags == MapType::SPECIAL2)
        return;

    for (; layers != layers_end; ++ layers)
    {
        MapLayer *const layer = *layers;
        if (!(layer->mMask & mMask))
            continue;

        mDrawOverLayers.push_back(layer);
    }
}

void Map::setMask(const int mask)
{
    if (mask != mMask)
    {
        mRedrawMap = true;
        mMask = mask;
        updateDrawLayersList();
    }
}

void Map::setMusicFile(const std::string &file)
{
    setProperty("music", file);
}

void Map::addAnimation(const int gid, TileAnimation *const animation)
{
    std::map<int, TileAnimation*>::iterator it = mTileAnimations.find(gid);
    if (it != mTileAnimations.end())
    {
        logger->log("duplicate map animation with gid = %d", gid);
        delete (*it).second;
    }
    mTileAnimations[gid] = animation;
}

void Map::setDrawLayersFlags(const MapType::MapType &n)
{
    mDrawLayersFlags = n;
    if (mDrawLayersFlags == MapType::SPECIAL3
        || mDrawLayersFlags == MapType::SPECIAL4
        || mDrawLayersFlags == MapType::BLACKWHITE)
    {
        mDrawOnlyFringe = true;
    }
    else
    {
        mDrawOnlyFringe = false;
    }
    updateDrawLayersList();
    FOR_EACH (Layers::iterator, it, mLayers)
    {
        MapLayer *const layer = *it;
        layer->setDrawLayerFlags(mDrawLayersFlags);
    }
}

void Map::setActorsFix(const int x, const int y)
{
    mActorFixX = x;
    mActorFixY = y;
    if (mFringeLayer)
        mFringeLayer->setActorsFix(y);
}

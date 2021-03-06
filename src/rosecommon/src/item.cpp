#include "item.h"
#include "itemdb.h"

using namespace RoseCommon;

Item::Item() :
    type_(Item::WEARABLE),
    id_(0),
    isCreated_(false),
    gemOpt_(0),
    durability_(0),
    life_(0),
    hasSocket_(false),
    isAppraised_(false),
    refine_(0),
    count_(0),
    isStackable_(false),
    atk_(0),
    def_(0),
    range_(0) {}

Item::Item(const ItemDef& def) :
    type_(def.type),
    id_(def.id),
    isCreated_(false),
    gemOpt_(0),
    durability_(0),
    life_(0),
    hasSocket_(false),
    isAppraised_(false),
    refine_(0),
    count_(1),
    isStackable_(def.type >= 10 && def.type <= 13 ? true : false),
    atk_(0),
    def_(0),
    range_(0)
{}

uint32_t Item::getVisible() const {
    return (refine_ << 20) | (hasSocket_ << 19) | (gemOpt_ << 10) | id_;
}

uint16_t Item::getHeader() const {
    return (isCreated_ << 15) | (id_ << 5) | type_;
}

uint32_t Item::getData() const {
    if (isStackable_)
        return count_;
    return (refine_ << 28) | (isAppraised_ << 27) | (hasSocket_ << 26) | (life_ << 16) | (durability_ << 9) | gemOpt_;
}

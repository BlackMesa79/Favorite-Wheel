#pragma once
#include "WheelLogic.h"

namespace Wheel {
    constexpr Category AlchemyCategory(bool poison,bool food) {
        return poison ? Category::Poisons : food ? Category::Food : Category::Potions;
    }
    constexpr bool ConsumableCategory(Category category) {
        return category==Category::Potions || category==Category::Poisons || category==Category::Food;
    }
}

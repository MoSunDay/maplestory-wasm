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
#pragma once
#include "BodyDrawInfo.h"
#include "EquipSlot.h"

#include "../../Graphics/Texture.h"
#include "../../Template/EnumMap.h"

#include <optional>
#include <string_view>
#include <unordered_map>

namespace jrc
{
    class Clothing
    {
    public:
        static constexpr int32_t TOP_DEFAULT_ID = 1042399;
        static constexpr int32_t BOTTOM_DEFAULT_ID = 1060026;

        enum Layer
        {
            CAPE, SHOES, PANTS, TOP, MAIL, MAILARM,
            EARRINGS, FACEACC, EYEACC, PENDANT, BELT, MEDAL, RING,
            CAP, CAP_BELOW_BODY, CAP_OVER_HAIR,
            GLOVE, WRIST,
            GLOVE_BELOW_BODY, WRIST_BELOW_BODY,
            GLOVE_OVER_BODY, WRIST_OVER_BODY,
            GLOVE_BELOW_HEAD, WRIST_BELOW_HEAD,
            GLOVE_BELOW_MAILARM, WRIST_BELOW_MAILARM,
            GLOVE_BELOW_WEAPON, WRIST_BELOW_WEAPON,
            GLOVE_OVER_HAIR, WRIST_OVER_HAIR,
            SHIELD, BACKSHIELD, SHIELD_BELOW_BODY, SHIELD_OVER_HAIR,
            WEAPON, BACKWEAPON, WEAPON_BELOW_ARM, WEAPON_BELOW_BODY,
            WEAPON_OVER_HAND, WEAPON_OVER_BODY, WEAPON_OVER_GLOVE,
            PANTS_DEFAULT, TOP_DEFAULT,
            NUM_LAYERS
        };

        // Construct a new equip.
        Clothing(int32_t itemid, const BodyDrawinfo& drawinfo);

        // Draw the equip.
        void draw(Stance::Id stance, Layer layer, uint8_t frame, const DrawArgument& args) const;
        void prepare(Stance::Id stance) const;
        // Check if a part of the equip lies on the specified layer while in the specified stance.
        bool contains_layer(Stance::Id stance, Layer layer) const;

        // Return wether the equip is invisble.
        bool is_transparent() const;
        // Return wether this equip uses twohanded stances.
        bool is_twohanded() const;
        // Return the item id.
        int32_t get_id() const;
        // Return the equip slot for this cloth.
        Equipslot::Id get_eqslot() const;
        // Return the standing stance to use while equipped.
        Stance::Id get_stand() const;
        // Return the walking stance to use while equipped.
        Stance::Id get_walk() const;
        // Return the vslot, used to distinguish some layering types.
        const std::string& get_vslot() const;

        // Pure mapping for every glove z label present in the v83 Character
        // NX. Distinct hand parts must never collapse into the default layer.
        static constexpr std::optional<Layer> glove_layer_by_name(
            std::string_view name
        ) {
            if (name == "glove") return GLOVE;
            if (name == "gloveWrist") return WRIST;
            if (name == "gloveBelowBody") return GLOVE_BELOW_BODY;
            if (name == "gloveWristBelowBody") return WRIST_BELOW_BODY;
            if (name == "gloveOverBody") return GLOVE_OVER_BODY;
            if (name == "gloveWristOverBody") return WRIST_OVER_BODY;
            if (name == "gloveBelowHead") return GLOVE_BELOW_HEAD;
            if (name == "gloveWristBelowHead") return WRIST_BELOW_HEAD;
            if (name == "gloveBelowMailArm") return GLOVE_BELOW_MAILARM;
            if (name == "gloveWristBelowMailArm") return WRIST_BELOW_MAILARM;
            if (name == "gloveBelowWeapon") return GLOVE_BELOW_WEAPON;
            if (name == "gloveWristBelowWeapon") return WRIST_BELOW_WEAPON;
            if (name == "gloveOverHair") return GLOVE_OVER_HAIR;
            if (name == "gloveWristOverHair") return WRIST_OVER_HAIR;
            return std::nullopt;
        }

        static constexpr std::optional<int16_t> glove_z(Layer layer)
        {
            switch (layer)
            {
            case GLOVE_BELOW_BODY: return 78;
            case WRIST_BELOW_BODY: return 77;
            case GLOVE_OVER_BODY: return 75;
            case WRIST_OVER_BODY: return 63;
            case GLOVE_BELOW_HEAD: return 57;
            case WRIST_BELOW_HEAD: return 55;
            case GLOVE: return 49;
            case WRIST: return 47;
            case GLOVE_BELOW_MAILARM: return 24;
            case WRIST_BELOW_MAILARM: return 22;
            case GLOVE_BELOW_WEAPON: return 19;
            case WRIST_BELOW_WEAPON: return 18;
            case GLOVE_OVER_HAIR: return 14;
            case WRIST_OVER_HAIR: return 13;
            default: return std::nullopt;
            }
        }

    private:
        EnumMap<Stance::Id, EnumMap<Layer, std::unordered_multimap<uint8_t, Texture>, NUM_LAYERS>> stances;
        int32_t itemid;
        Equipslot::Id eqslot;
        Stance::Id walk;
        Stance::Id stand;
        std::string vslot;
        bool twohanded;
        bool transparent;


        static const std::unordered_map<std::string, Layer> sublayernames;
    };
}

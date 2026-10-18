#pragma once

#include "../../Character/Look/CharLook.h"
#include "../../Graphics/Animation.h"
#include "../../Graphics/Sprite.h"
#include "../../Graphics/Text.h"
#include "../../Graphics/Texture.h"
#include "../../Net/CharacterInfo/CharacterInfoModel.h"
#include "../UIElement.h"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace jrc
{
    class UIUserInfo : public UIElement
    {
    public:
        static constexpr Type TYPE = USERINFO;
        static constexpr bool FOCUSED = false;
        static constexpr bool TOGGLED = false;

        UIUserInfo(int32_t character_id, const std::string& name,
            const CharLook& look, bool self);

        void draw(float alpha) const override;
        void update() override;
        void update_screen(int16_t width, int16_t height) override;
        bool remove_cursor(bool clicked, Point<int16_t> cursor) override;
        CursorResult send_cursor(bool clicked, Point<int16_t> cursor) override;
        void send_key(int32_t keycode, bool pressed, bool escape) override;

        void apply_info(character_info::CharacterInfoPayload payload);
        void fail_info(const std::string& message);
        void apply_fame_response(const character_info::FameResponse& response);
        void fail_fame(const std::string& message);

        int32_t target_id() const;
        bool info_loaded() const;
        bool pet_expanded() const;
        size_t pet_count() const;
        uint8_t selected_pet() const;
        bool fame_pending() const;
        Point<int16_t> origin() const;
        Point<int16_t> size() const;

    protected:
        Button::State button_pressed(uint16_t button_id) override;

    private:
        enum Buttons : uint16_t
        {
            BT_CLOSE,
            BT_PET,
            BT_TRADE,
            BT_FAME_UP,
            BT_FAME_DOWN,
            BT_PET_TAB_0,
            BT_PET_TAB_1,
            BT_PET_TAB_2
        };

        void clamp_position();
        void sync_controls();
        void draw_main(float alpha) const;
        void draw_pet(float alpha) const;
        void refresh_pet_animation();
        void request_fame(bool increase);

        int32_t character_id;
        std::string character_name;
        CharLook look;
        bool self;
        bool can_fame = false;
        bool loading = true;
        bool expanded = false;
        bool waiting_for_fame = false;
        bool dragged = false;
        uint8_t pet_index = 0;
        uint16_t request_frames = 0;
        uint16_t fame_frames = 0;
        Point<int16_t> cursor_offset;
        std::optional<character_info::CharacterInfoPayload> info;

        std::array<Texture, 3> character_backgrounds;
        std::array<Texture, 3> pet_backgrounds;
        std::array<Texture, 3> pet_action_backgrounds;
        std::array<Texture, 3> enabled_tabs;
        std::array<Texture, 3> disabled_tabs;
        Texture character_name_background;
        Texture married_icon;
        Animation pet_animation;
        Text title;
    };
}

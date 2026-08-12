#pragma once

#include "../UIState.h"
#include "CashShopModel.h"
#include "../../Template/EnumMap.h"

#include <memory>

namespace jrc
{
    class UIStateCashShop : public UIState
    {
    public:
        UIStateCashShop(std::shared_ptr<CashShopModel> model, bool female);

        void draw(float alpha, Point<int16_t> cursor) const override;
        void update() override;
        void doubleclick(Point<int16_t> pos) override;
        void rightclick(Point<int16_t> pos) override;
        void send_key(KeyType::Id type, int32_t action, bool pressed, bool escape) override;
        Cursor::State send_cursor(Cursor::State state, Point<int16_t> pos) override;
        void send_scroll(Point<int16_t> pos, double yoffset) override;
        void send_close() override;
        void cancel_drag() override;
        void drag_icon(Icon* icon) override;
        void clear_tooltip(Tooltip::Parent parent) override;
        void show_equip(Tooltip::Parent parent, int16_t slot) override;
        void show_item(Tooltip::Parent parent, int32_t itemid) override;
        void show_skill(Tooltip::Parent parent, int32_t skill_id,
            int32_t level, int32_t masterlevel, int64_t expiration) override;
        void show_text(Tooltip::Parent parent, const std::string& text) override;
        void show_map(Tooltip::Parent parent, const std::string& title,
            const std::string& description, int32_t mapid, bool bolded, bool portal) override;
        Iterator pre_add(UIElement::Type type, bool toggled, bool focused) override;
        void remove(UIElement::Type type) override;
        UIElement* get(UIElement::Type type) override;
        UIElement* get_front(const std::list<UIElement::Type>& types) override;
        UIElement* get_front(Point<int16_t> pos) override;

    private:
        void clear_cursors(bool clicked, Point<int16_t> pos, UIElement::Type except);
        template <class T, typename... Args>
        void emplace(Args&&... args);

        EnumMap<UIElement::Type, UIElement::UPtr, UIElement::NUM_TYPES> elements;
        UIElement::Type focused;
        UIElement::Type cursor_captured;
        int16_t view_width;
        int16_t view_height;
    };
}

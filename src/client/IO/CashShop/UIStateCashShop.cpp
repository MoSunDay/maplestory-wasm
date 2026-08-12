#include "UIStateCashShop.h"

#include "UICashShop.h"
#include "../UI.h"
#include "../../Constants.h"

#include <algorithm>

namespace jrc
{
    UIStateCashShop::UIStateCashShop(std::shared_ptr<CashShopModel> model, bool female)
        : focused(UIElement::NONE), cursor_captured(UIElement::NONE),
          view_width(Constants::viewwidth()), view_height(Constants::viewheight())
    {
        emplace<UICashShop>(std::move(model), female);
    }

    void UIStateCashShop::draw(float alpha, Point<int16_t>) const
    {
        for (const auto& entry : elements)
            if (entry.second && entry.second->is_active())
                entry.second->draw(alpha);
    }

    void UIStateCashShop::update()
    {
        int16_t width = Constants::viewwidth();
        int16_t height = Constants::viewheight();
        bool resized = width != view_width || height != view_height;
        view_width = width;
        view_height = height;
        for (auto& element : elements.values())
        {
            if (!element || !element->is_active())
                continue;
            if (resized)
                element->update_screen(width, height);
            element->update();
        }
    }

    void UIStateCashShop::doubleclick(Point<int16_t> pos)
    {
        if (UIElement* front = get_front(pos))
            front->doubleclick(pos);
    }

    void UIStateCashShop::rightclick(Point<int16_t> pos)
    {
        if (UIElement* front = get_front(pos))
            front->rightclick(pos);
    }

    void UIStateCashShop::send_key(KeyType::Id, int32_t action, bool pressed, bool escape)
    {
        if (UIElement* front = get(focused); front && front->is_active())
            front->send_key(action, pressed, escape);
        else if (UIElement* cash_shop = get(UIElement::CASHSHOP))
            cash_shop->send_key(action, pressed, escape);
    }

    Cursor::State UIStateCashShop::send_cursor(Cursor::State state, Point<int16_t> pos)
    {
        bool clicked = state == Cursor::CLICKING;
        UIElement* target = nullptr;
        UIElement::Type target_type = UIElement::NONE;
        if (UIElement* captured = get(cursor_captured); captured && captured->is_active())
        {
            target = captured;
            target_type = cursor_captured;
        }
        else if (UIElement* focus = get(focused); focus && focus->is_active())
        {
            target = focus;
            target_type = focused;
        }
        else
        {
            target = get_front(pos);
            target_type = target ? target->get_type() : UIElement::NONE;
        }

        clear_cursors(clicked, pos, target_type);
        if (!target)
            return Cursor::IDLE;
        UIElement::CursorResult result = target->send_cursor(clicked, pos);
        if (clicked && result.handled)
            cursor_captured = target_type;
        else if (!clicked)
            cursor_captured = UIElement::NONE;
        return result.state;
    }

    void UIStateCashShop::send_scroll(Point<int16_t> pos, double yoffset)
    {
        if (UIElement* front = get_front(pos))
            front->send_scroll(yoffset);
    }

    void UIStateCashShop::send_close()
    {
        if (UIElement* cash_shop = get(UIElement::CASHSHOP))
            cash_shop->send_key(0, true, true);
    }

    void UIStateCashShop::cancel_drag() {}
    void UIStateCashShop::drag_icon(Icon*) {}
    void UIStateCashShop::clear_tooltip(Tooltip::Parent) {}
    void UIStateCashShop::show_equip(Tooltip::Parent, int16_t) {}
    void UIStateCashShop::show_item(Tooltip::Parent, int32_t) {}
    void UIStateCashShop::show_skill(Tooltip::Parent, int32_t, int32_t, int32_t, int64_t) {}
    void UIStateCashShop::show_text(Tooltip::Parent, const std::string&) {}
    void UIStateCashShop::show_map(Tooltip::Parent, const std::string&,
        const std::string&, int32_t, bool, bool) {}

    template <class T, typename... Args>
    void UIStateCashShop::emplace(Args&&... args)
    {
        if (auto iter = pre_add(T::TYPE, T::TOGGLED, T::FOCUSED))
        {
            auto element = std::make_unique<T>(std::forward<Args>(args)...);
            element->set_type(T::TYPE);
            element->update_screen(view_width, view_height);
            (*iter).second = std::move(element);
        }
    }

    UIState::Iterator UIStateCashShop::pre_add(UIElement::Type type, bool, bool is_focused)
    {
        remove(type);
        if (is_focused)
            focused = type;
        return elements.find(type);
    }

    void UIStateCashShop::remove(UIElement::Type type)
    {
        if (focused == type)
            focused = UIElement::NONE;
        if (cursor_captured == type)
            cursor_captured = UIElement::NONE;
        elements[type].reset();
    }

    UIElement* UIStateCashShop::get(UIElement::Type type)
    {
        return elements[type].get();
    }

    UIElement* UIStateCashShop::get_front(const std::list<UIElement::Type>& types)
    {
        for (auto iter = elements.values().rbegin(); iter != elements.values().rend(); ++iter)
            if (*iter && (*iter)->is_active() &&
                std::find(types.begin(), types.end(), (*iter)->get_type()) != types.end())
                return iter->get();
        return nullptr;
    }

    UIElement* UIStateCashShop::get_front(Point<int16_t> pos)
    {
        for (auto iter = elements.values().rbegin(); iter != elements.values().rend(); ++iter)
            if (*iter && (*iter)->is_active() && (*iter)->is_in_range(pos))
                return iter->get();
        return nullptr;
    }

    void UIStateCashShop::clear_cursors(bool clicked, Point<int16_t> pos, UIElement::Type except)
    {
        for (UIElement::Type type : elements.keys())
            if (type != except && elements[type] && elements[type]->is_active())
                elements[type]->remove_cursor(clicked, pos);
    }
}

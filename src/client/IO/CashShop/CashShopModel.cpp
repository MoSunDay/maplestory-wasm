#include "CashShopModel.h"

#include "nlnx/node.hpp"
#include "nlnx/nx.hpp"

#include <algorithm>
#include <fstream>
#include <iterator>
#include <stdexcept>
#include <string_view>
#include <unordered_map>

namespace jrc
{
    namespace
    {
        int8_t category_for_item(int32_t item_id)
        {
            int32_t prefix = item_id / 1'000'000;
            return (prefix >= 1 && prefix <= 5) ? static_cast<int8_t>(prefix) : 6;
        }

        nl::node item_string_node(int32_t item_id)
        {
            switch (item_id / 1'000'000)
            {
            case 1:
            {
                constexpr const char* categories[] = {
                    "Cap", "Accessory", "Accessory", "Accessory", "Coat",
                    "Longcoat", "Pants", "Shoes", "Glove", "Shield", "Cape",
                    "Ring", "Accessory", "Accessory", "Accessory"
                };
                size_t index = static_cast<size_t>(item_id / 10'000 - 100);
                std::string category;
                if (index < std::size(categories))
                    category = categories[index];
                else if (index >= 30 && index <= 70)
                    category = "Weapon";
                return nl::nx::string["Eqp.img"]["Eqp"][category][std::to_string(item_id)];
            }
            case 2:
                return nl::nx::string["Consume.img"][std::to_string(item_id)];
            case 3:
                return nl::nx::string["Ins.img"][std::to_string(item_id)];
            case 4:
                return nl::nx::string["Etc.img"]["Etc"][std::to_string(item_id)];
            case 5:
                return nl::nx::string["Cash.img"][std::to_string(item_id)];
            default:
                return {};
            }
        }

        std::vector<std::string_view> split(std::string_view value, char separator)
        {
            std::vector<std::string_view> fields;
            size_t begin = 0;
            while (begin <= value.size())
            {
                const size_t end = value.find(separator, begin);
                fields.push_back(value.substr(begin,
                    end == std::string_view::npos ? value.size() - begin : end - begin));
                if (end == std::string_view::npos)
                    break;
                begin = end + 1;
            }
            return fields;
        }

        int32_t number(const std::vector<std::string_view>& fields, size_t index)
        {
            return std::stoi(std::string(fields.at(index)));
        }
    }

    int32_t CashBalances::get(CashCurrency currency) const
    {
        switch (currency)
        {
        case CashCurrency::NX_CREDIT:
            return nx_credit;
        case CashCurrency::MAPLE_POINT:
            return maple_points;
        case CashCurrency::NX_PREPAID:
            return nx_prepaid;
        }
        return 0;
    }

    CashShopModel::CashShopModel()
    {
        load_catalog();
    }

    const std::vector<CashCommodity>& CashShopModel::catalog() const
    {
        return commodities;
    }

    const std::vector<CashLockerItem>& CashShopModel::locker() const
    {
        return locker_items;
    }

    const CashBalances& CashShopModel::balances() const
    {
        return cash_balances;
    }

    void CashShopModel::set_balances(CashBalances value)
    {
        cash_balances = value;
    }

    void CashShopModel::replace_locker(std::vector<CashLockerItem> value)
    {
        locker_items = std::move(value);
    }

    void CashShopModel::add_locker(CashLockerItem value)
    {
        remove_locker(value.cash_id);
        locker_items.push_back(std::move(value));
    }

    void CashShopModel::remove_locker(int64_t cash_id)
    {
        locker_items.erase(
            std::remove_if(locker_items.begin(), locker_items.end(),
                [cash_id](const CashLockerItem& item) { return item.cash_id == cash_id; }),
            locker_items.end());
    }

    void CashShopModel::apply_special_item(int32_t sn, int32_t modifier, int8_t info)
    {
        auto found = std::find_if(commodities.begin(), commodities.end(),
            [sn](const CashCommodity& item) { return item.sn == sn; });
        if (found == commodities.end())
            return;

        // Cosmic documents bit 1024 as the add/remove modifier. The info byte
        // carries the desired state, so this changes visibility without
        // inventing a client-side price rule the server does not enforce.
        if ((modifier & 1024) != 0)
            found->on_sale = info != 0;
    }

    std::string CashShopModel::item_name(int32_t item_id)
    {
        nl::node source = item_string_node(item_id);
        std::string name = source["name"].get_string();
        return name.empty() ? "物品 " + std::to_string(item_id) : name;
    }

    void CashShopModel::load_catalog()
    {
        std::unordered_map<int32_t, std::vector<int32_t>> packages;
        std::ifstream source("data/cash-shop-v83.csv", std::ios::binary);
        if (!source)
            throw std::runtime_error("Missing linked-server cash shop catalog");
        const std::string content{
            std::istreambuf_iterator<char>(source), std::istreambuf_iterator<char>()};
        for (std::string_view record : split(content, ';'))
        {
            const auto fields = split(record, ',');
            if (fields.empty() || fields[0].empty())
                continue;
            if (fields[0] == "C" && fields.size() == 9)
            {
                CashCommodity item;
                item.sn = number(fields, 1);
                item.item_id = number(fields, 2);
                item.price = number(fields, 3);
                item.period = number(fields, 4);
                item.count = static_cast<int16_t>(number(fields, 5));
                item.gender = static_cast<int8_t>(number(fields, 6));
                item.priority = number(fields, 7);
                item.on_sale = number(fields, 8) == 1;
                item.category = category_for_item(item.item_id);
                item.name = item_name(item.item_id);
                commodities.push_back(std::move(item));
            }
            else if (fields[0] == "P" && fields.size() >= 3)
            {
                std::vector<int32_t> sns;
                for (size_t index = 2; index < fields.size(); ++index)
                    sns.push_back(number(fields, index));
                packages.emplace(number(fields, 1), std::move(sns));
            }
            else
                throw std::runtime_error("Invalid linked-server cash shop catalog record");
        }

        for (CashCommodity& item : commodities)
        {
            auto package = packages.find(item.item_id);
            if (package == packages.end())
                continue;
            item.package = true;
            item.category = 6;
            item.package_sns = package->second;
            if (item.name.rfind("物品 ", 0) == 0)
                item.name = "礼包 " + std::to_string(item.item_id);
        }

        auto acceptance = std::find_if(commodities.begin(), commodities.end(),
            [](const CashCommodity& item) { return item.sn == 80000002; });
        if (acceptance == commodities.end() || acceptance->item_id != 4031192 ||
            acceptance->price != 1 || !acceptance->on_sale)
            throw std::runtime_error("Invalid linked-server cash shop acceptance item");

        std::stable_sort(commodities.begin(), commodities.end(),
            [](const CashCommodity& left, const CashCommodity& right) {
                if (left.on_sale != right.on_sale)
                    return left.on_sale > right.on_sale;
                return left.priority > right.priority;
            });
    }
}

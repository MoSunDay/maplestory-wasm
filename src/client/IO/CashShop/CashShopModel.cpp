#include "CashShopModel.h"

#include "nlnx/node.hpp"
#include "nlnx/nx.hpp"

#include <algorithm>
#include <iterator>

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

    void CashShopModel::load_packages(
        std::unordered_map<int32_t, std::vector<int32_t>>& packages)
    {
        for (nl::node package : nl::nx::etc["CashPackage.img"])
        {
            std::vector<int32_t> sns;
            for (nl::node entry : package["SN"])
                sns.push_back(static_cast<int32_t>(entry.get_integer()));
            packages.emplace(std::stoi(package.name()), std::move(sns));
        }
    }

    void CashShopModel::load_catalog()
    {
        std::unordered_map<int32_t, std::vector<int32_t>> packages;
        load_packages(packages);

        for (nl::node source : nl::nx::etc["Commodity.img"])
        {
            CashCommodity item;
            item.sn = source["SN"];
            item.item_id = source["ItemId"];
            item.price = source["Price"];
            item.period = source["Period"];
            item.count = static_cast<int16_t>(source["Count"].get_integer(1));
            item.gender = static_cast<int8_t>(source["Gender"].get_integer(2));
            item.priority = source["Priority"];
            item.on_sale = source["OnSale"].get_integer() == 1;
            item.category = category_for_item(item.item_id);
            item.name = item_name(item.item_id);

            auto package = packages.find(item.item_id);
            if (package != packages.end())
            {
                item.package = true;
                item.category = 6;
                item.package_sns = package->second;
                if (item.name.rfind("物品 ", 0) == 0)
                    item.name = "礼包 " + std::to_string(item.item_id);
            }
            commodities.push_back(std::move(item));
        }

        std::stable_sort(commodities.begin(), commodities.end(),
            [](const CashCommodity& left, const CashCommodity& right) {
                if (left.on_sale != right.on_sale)
                    return left.on_sale > right.on_sale;
                return left.priority > right.priority;
            });
    }
}

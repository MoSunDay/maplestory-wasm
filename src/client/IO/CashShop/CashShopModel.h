#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace jrc
{
    enum class CashCurrency : int32_t
    {
        NX_CREDIT = 1,
        MAPLE_POINT = 2,
        NX_PREPAID = 4
    };

    struct CashBalances
    {
        int32_t nx_credit = 0;
        int32_t maple_points = 0;
        int32_t nx_prepaid = 0;

        int32_t get(CashCurrency currency) const;
    };

    struct CashCommodity
    {
        int32_t sn = 0;
        int32_t item_id = 0;
        int32_t price = 0;
        int32_t period = 0;
        int16_t count = 1;
        int8_t gender = 2;
        int8_t category = 0;
        int32_t priority = 0;
        bool on_sale = false;
        bool package = false;
        std::string name;
        std::vector<int32_t> package_sns;
    };

    struct CashLockerItem
    {
        int64_t cash_id = 0;
        int32_t item_id = 0;
        int32_t sn = 0;
        int16_t count = 1;
        int64_t expiration = 0;
        std::string gift_from;
    };

    class CashShopModel
    {
    public:
        CashShopModel();

        const std::vector<CashCommodity>& catalog() const;
        const std::vector<CashLockerItem>& locker() const;
        const CashBalances& balances() const;

        void set_balances(CashBalances value);
        void replace_locker(std::vector<CashLockerItem> value);
        void add_locker(CashLockerItem value);
        void remove_locker(int64_t cash_id);
        void apply_special_item(int32_t sn, int32_t modifier, int8_t info);

        static std::string item_name(int32_t item_id);

    private:
        void load_catalog();

        std::vector<CashCommodity> commodities;
        std::vector<CashLockerItem> locker_items;
        CashBalances cash_balances;
    };
}

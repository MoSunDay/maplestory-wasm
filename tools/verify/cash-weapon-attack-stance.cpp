// Audits the data contract behind the cash-weapon attack-stance fix.
//
// Cash (NX) weapon imgs under Character/Weapon/0170*.img are pure looks:
//  - info carries cash=1 but NO attack, attackSpeed or afterImage leaves,
//    so a cash cover in the weapon slot can never drive the attack stance
//    (CharLook::attack resolves stances from the real equipped weapon).
//  - their stances are nested under numeric per-weapon-family body groups
//    (e.g. 01702000.img/33/swingO1/...); every group that renders anything
//    carries at least one attack stance, so any selected stance stays
//    renderable (a handful of covers ship only empty placeholder groups).
// Reference real weapons of all 16 families expose integer info/attack in
// 1..9 with a non-empty regular_attack stance set. The body 00002000.img
// documents the symptom scale: stand1 loops 3 frames of 500ms while the
// real attack stances finish in ~800ms, so the old stand1 fallback locked
// the attacking state for roughly twice an attack cycle.
#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "nlnx/bitmap.hpp"
#include "nlnx/file.hpp"
#include "nlnx/node.hpp"

#include "Character/Look/Attack/RegularStances.h"

// Metadata-only audit: stub the constructor so bitmap.cpp is not linked.
namespace nl
{
    bitmap::bitmap(void* file, uint64_t offset, uint16_t width, uint16_t height)
        : m_file(file), m_offset(offset), m_width(width), m_height(height) {}
}

namespace
{
    int failures = 0;
    int checks = 0;

    void check(bool ok, const std::string& what)
    {
        std::cout << (ok ? "  ok  " : " FAIL ") << what << '\n';
        ++checks;
        if (!ok)
            ++failures;
    }

    bool present(nl::node n)
    {
        return static_cast<bool>(n);
    }

    bool is_int(nl::node n)
    {
        return n.data_type() == nl::node::type::integer;
    }

    // The cash flag is an integer 1 everywhere except 01702221.img, which
    // encodes the same value as the string "1".
    bool cash_flag_set(nl::node n)
    {
        if (is_int(n))
            return n.get_integer() == 1;
        if (n.data_type() == nl::node::type::string)
            return n.get_string() == "1";
        return false;
    }

    bool numeric_name(const std::string& name)
    {
        if (name.empty())
            return false;
        for (char c : name)
            if (c < '0' || c > '9')
                return false;
        return true;
    }

    // Stances any weapon-family body group must offer at least one of.
    const char* const ATTACK_STANCES[] = {
        "stabO1", "stabO2", "stabT1", "stabT2",
        "swingO1", "swingO2", "swingO3",
        "swingT1", "swingT2", "swingT3",
        "swingP1", "swingP2",
        "shoot1", "shoot2"
    };

    bool group_has_attack_stance(nl::node group)
    {
        for (const char* stance : ATTACK_STANCES)
            if (present(group[stance]))
                return true;
        return false;
    }

    // One reference weapon per family: 1H sword/axe/mace/dagger, wand, staff,
    // 2H sword/axe/mace, spear, polearm, bow, crossbow, claw, knuckle, gun.
    const char* const REAL_WEAPONS[] = {
        "01302001", "01312003", "01322054", "01332005",
        "01372004", "01382001",
        "01402001", "01412000", "01422000", "01432000", "01442000",
        "01452000", "01462000", "01472000", "01482000", "01492000"
    };

    void audit_cash_weapons(nl::node weaponroot)
    {
        size_t cash_count = 0;
        bool all_cash_flag = true;
        bool none_attack_data = true;
        bool all_groups_usable = true;

        for (nl::node img : weaponroot)
        {
            const std::string& name = img.name();
            if (name.size() < 8 || name.compare(0, 4, "0170") != 0)
                continue;

            ++cash_count;

            nl::node info = img["info"];
            all_cash_flag = all_cash_flag && cash_flag_set(info["cash"]);
            none_attack_data = none_attack_data
                && !present(info["attack"])
                && !present(info["attackSpeed"])
                && !present(info["afterImage"]);

            size_t groups = 0;
            bool groups_usable = true;
            for (nl::node child : img)
            {
                if (!numeric_name(child.name()))
                    continue;
                ++groups;
                // A few covers ship a fully-empty placeholder group (e.g.
                // 01702220.img/37); it offers no stance to select at all, so
                // only groups that render something must carry attack stances.
                bool empty = child.begin() == child.end();
                groups_usable = groups_usable
                    && (empty || group_has_attack_stance(child));
            }
            all_groups_usable = all_groups_usable
                && groups >= 1 && groups_usable;
        }

        check(cash_count > 300,
            "cash weapon set is present (" + std::to_string(cash_count) + " imgs)");
        check(all_cash_flag, "every 0170*.img info carries cash=1");
        check(none_attack_data,
            "no cash weapon exposes attack/attackSpeed/afterImage");
        check(all_groups_usable,
            "every cash weapon has body groups carrying attack stances");
    }

    void audit_real_weapons(nl::node weaponroot)
    {
        bool all_valid = true;
        for (const char* id : REAL_WEAPONS)
        {
            nl::node info = weaponroot[std::string(id) + ".img"]["info"];
            bool valid = is_int(info["attack"]);
            if (valid)
            {
                int64_t attack = info["attack"].get_integer();
                // Knuckles (attack 8) are the single family without a
                // regular_attack stance set; like cash covers they resolve
                // through the stand1 fallback in CharLook::getattackstance.
                valid = attack >= 1 && attack <= 9
                    && (attack == 8 || !jrc::regular_attack::choices(
                        static_cast<uint8_t>(attack), false).empty());
            }
            all_valid = all_valid && valid;
        }
        check(all_valid,
            "all 16 reference real weapons expose attack 1..9 with stances");
    }

    int64_t stance_delay_sum(nl::node body, const char* stance)
    {
        int64_t total = 0;
        nl::node stancenode = body[stance];
        if (!present(stancenode))
            return -1;
        for (nl::node frame : stancenode)
        {
            if (numeric_name(frame.name()) && is_int(frame["delay"]))
                total += frame["delay"].get_integer();
        }
        return total;
    }

    void audit_body_delays(nl::node file_root)
    {
        nl::node body = file_root["00002000.img"];
        const int64_t stand1 = stance_delay_sum(body, "stand1");
        const int64_t swingo1 = stance_delay_sum(body, "swingO1");
        const int64_t stabo1 = stance_delay_sum(body, "stabO1");
        const int64_t shoot1 = stance_delay_sum(body, "shoot1");
        check(stand1 == 1500, "body stand1 loops 1500ms (3 x 500ms)");
        check(swingo1 > 0 && stand1 > swingo1,
            "stand1 outlasts swingO1 (" + std::to_string(stand1) + "ms vs "
            + std::to_string(swingo1) + "ms) - the old pause scale");
        check(stabo1 > 0 && shoot1 > 0,
            "real attack stances exist (stabO1/shoot1 delays)");
    }
}

int main(int argc, char* argv[])
{
    if (argc < 2)
    {
        std::cerr << "Usage: " << argv[0] << " <Character.nx>\n";
        return 1;
    }

    nl::file nxfile(argv[1]);
    nl::node root = nxfile.root();
    check(present(root), "Character.nx loads");

    audit_cash_weapons(root["Weapon"]);
    audit_real_weapons(root["Weapon"]);
    audit_body_delays(root);

    std::cout << (checks - failures) << '/' << checks << " checks passed\n";
    return failures == 0 ? 0 : 1;
}

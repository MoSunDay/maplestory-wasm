//////////////////////////////////////////////////////////////////////////////
// This file is part of the Journey MMORPG client                           //
// Copyright © 2015-2016 Daniel Allendorf                                   //
//                                                                          //
// This program is free software: you can redistribute it and/or modify     //
// it under the terms of the GNU Affero General Public License as           //
// published by the Free Software Foundation, either version 3 of the       //
// License, or (at your option) any later version.                          //
//                                                                          //
// This program is distributed in the hope that it will be useful, but      //
// WITHOUT ANY WARRANTY; without even the implied warranty of               //
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the            //
// GNU Affero General Public License for more details.                      //
//                                                                          //
// You should have received a copy of the GNU Affero General Public License //
// along with this program.  If not, see <http://www.gnu.org/licenses/>.    //
//////////////////////////////////////////////////////////////////////////////
// Late-owner stash for remote summons, split out of SummonPool.cpp so the
// entity/animation pool stays within the per-file line budget.
#include "SummonPool.h"

#include <algorithm>
#include <utility>

namespace jrc
{
    namespace summon
    {
        void stash_event(SummonPool& pool, int32_t owner_cid,
            SummonEvent&& event)
        {
            // SPAWN/MOVE can share a network batch with the owner's
            // SPAWN_PLAYER; keep the newest event per kind and oid for the
            // replay pass instead of dropping it. The per-owner cap bounds
            // an owner that never instantiates.
            std::vector<SummonEvent>& events = pool.pending_events[owner_cid];
            auto stashed = std::find_if(
                events.begin(),
                events.end(),
                [&event](const SummonEvent& entry)
                {
                    return entry.kind == event.kind
                        && entry.oid == event.oid;
                }
            );
            if (stashed != events.end())
                *stashed = std::move(event);
            else
                events.push_back(std::move(event));

            if (events.size() > MAX_STASHED_SUMMON_EVENTS)
                events.erase(events.begin());
        }

        void replay_stashed(
            SummonPool& pool,
            const std::function<bool(int32_t)>& owner_exists)
        {
            // Called right after MapChars::update, so a stash whose owner
            // still cannot be found is stale and simply waits for a later
            // pass or a map change.
            std::vector<int32_t> ready;
            ready.reserve(pool.pending_events.size());
            for (const auto& entry : pool.pending_events)
            {
                if (owner_exists(entry.first))
                    ready.push_back(entry.first);
            }

            for (int32_t owner_cid : ready)
            {
                auto iter = pool.pending_events.find(owner_cid);
                for (SummonEvent& event : iter->second)
                {
                    switch (event.kind)
                    {
                        case SummonEvent::SPAWN:
                            spawn(pool, event.spawn);
                            break;
                        case SummonEvent::MOVE:
                            move(pool, event.oid, event.start,
                                std::move(event.movements));
                            break;
                    }
                }
                pool.pending_events.erase(iter);
            }
        }

        void drop_stashed(SummonPool& pool, int32_t owner_cid)
        {
            pool.pending_events.erase(owner_cid);
        }

        void drop_stashed_oid(SummonPool& pool, int32_t owner_cid,
            int32_t oid)
        {
            // REMOVE_SPECIAL_MAPOBJECT arrived while the owner was still
            // queued: cancel the stashed SPAWN/MOVE instead of letting the
            // replay resurrect a summon the server already removed. The
            // live entity is untouched (it cannot exist before the owner).
            auto owner = pool.pending_events.find(owner_cid);
            if (owner == pool.pending_events.end())
                return;

            std::vector<SummonEvent>& events = owner->second;
            events.erase(
                std::remove_if(
                    events.begin(), events.end(),
                    [oid](const SummonEvent& entry)
                    {
                        return entry.oid == oid;
                    }),
                events.end());
            if (events.empty())
                pool.pending_events.erase(owner);
        }
    }
}

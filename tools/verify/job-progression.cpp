#include "Character/JobProgression.h"
#include "Character/Job.h"

#include <cassert>

using jrc::job_progression::Stage;

int main()
{
    using namespace jrc::job_progression;

    assert(stage_for(0) == Stage::BEGINNER);
    assert(stage_for(1000) == Stage::BEGINNER);
    assert(stage_for(2000) == Stage::BEGINNER);
    assert(stage_for(112) == Stage::FOURTH);
    assert(stage_for(1112) == Stage::FOURTH);
    assert(stage_for(2112) == Stage::FOURTH);

    assert(job_for_stage(112, Stage::BEGINNER) == 0);
    assert(job_for_stage(112, Stage::FIRST) == 100);
    assert(job_for_stage(112, Stage::SECOND) == 110);
    assert(job_for_stage(112, Stage::THIRD) == 111);
    assert(job_for_stage(112, Stage::FOURTH) == 112);

    assert(job_for_stage(1112, Stage::BEGINNER) == 1000);
    assert(job_for_stage(1112, Stage::FIRST) == 1100);
    assert(job_for_stage(1112, Stage::SECOND) == 1110);
    assert(job_for_stage(1112, Stage::THIRD) == 1111);
    assert(job_for_stage(1112, Stage::FOURTH) == 1112);

    assert(job_for_stage(2112, Stage::BEGINNER) == 2000);
    assert(job_for_stage(2112, Stage::FIRST) == 2100);
    assert(job_for_stage(2112, Stage::SECOND) == 2110);
    assert(job_for_stage(2112, Stage::THIRD) == 2111);
    assert(job_for_stage(2112, Stage::FOURTH) == 2112);

    assert(job_for_stage(1110, Stage::THIRD) == 0);
    assert(beginner_skill_for(112, 0) == 1000);
    assert(beginner_skill_for(1112, 1) == 10001001);
    assert(beginner_skill_for(2112, 2) == 20001002);
    assert(archetype_for(1212) == 2);
    assert(archetype_for(1412) == 4);
    assert(archetype_for(2112) == 1);

    jrc::Job cygnus(1112);
    assert(cygnus.get_level() == jrc::Job::FOURTH);
    assert(cygnus.get_subjob(jrc::Job::BEGINNER) == 1000);
    assert(cygnus.get_subjob(jrc::Job::FIRST) == 1100);
    assert(cygnus.can_use(10001000));
    assert(cygnus.can_use(11121000));
    assert(!cygnus.can_use(1000));

    jrc::Job blaze_wizard(1212);
    assert(blaze_wizard.get_primary(jrc::Weapon::WAND) == jrc::Equipstat::INT);
    assert(blaze_wizard.get_secondary(jrc::Weapon::WAND) == jrc::Equipstat::LUK);

    jrc::Job aran(2112);
    assert(aran.get_subjob(jrc::Job::BEGINNER) == 2000);
    assert(aran.can_use(20001002));
    assert(aran.get_primary(jrc::Weapon::POLEARM) == jrc::Equipstat::STR);
}

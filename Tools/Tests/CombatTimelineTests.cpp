#include "../../NarakuGame/Source/NarakuCombatData.h"
#include <cmath>
#include <iostream>
#include <stdexcept>

using namespace NarakuCombat;

void Check(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

int main(int argc, char** argv)
{
    try
    {
        Check(argc == 3, "expected scratch directory and legacy fixture");
        AttackDefinition attack;
        attack.id = "test"; attack.totalFrames = 30;
        Check(attack.phases.empty() && SaveWarnings(attack).empty(), "empty all-type data is valid");
        for (DataType type : {DataType::Attack, DataType::Movement, DataType::Guard, DataType::Invincible})
        {
            attack.dataType = type;
            Check(SaveWarnings(attack).size() == 1, "required phase warning");
        }
        Phase phase; phase.frames = 9; phase.type = PhaseType::Startup;
        phase.hurtbox.center = {0, 0, 0}; phase.hurtbox.size = {1, 1, 1};
        FrameKey key; key.frame = 6; key.hurtbox.center = {2, 0, 0}; key.hurtbox.size = {1, 1, 1};
        phase.keys.push_back(key); attack.phases.push_back(phase);
        for (DataType type : {DataType::Movement, DataType::Guard})
        {
            attack.dataType = type;
            Check(SaveWarnings(attack).empty(), "startup alone satisfies movement/guard");
        }
        Check(EvaluateFrame(attack, 0).hurtbox.center.x == 0, "frame 1 uses common value");
        Check(EvaluateFrame(attack, 5).hurtbox.center.x == 0, "frames 4-6 hold previous value");
        Check(EvaluateFrame(attack, 6).hurtbox.center.x == 2, "frame 7 applies key");
        Check(EvaluateFrame(attack, 8.9f).phaseIndex == 0, "last fractional frame still in phase");
        Check(EvaluateFrame(attack, 9).phaseIndex == -1 && EvaluateFrame(attack, 9).hurtbox.center.x == 2,
            "empty tail retains final hurtbox");
        phase.type = PhaseType::Invincible; phase.frames = 3; phase.keys.clear();
        phase.hurtbox.center = {5, 0, 0}; phase.hasAttackBox = true;
        phase.attackBox.center = {0, 1, 2}; phase.attackBox.size = {1, 2, 3};
        attack.phases.push_back(phase);
        Check(!EvaluateFrame(attack, 8).invincible && EvaluateFrame(attack, 9).invincible, "exclusive phase boundary");
        Check(EvaluateFrame(attack, 9).hurtbox.center.x == 5, "new phase resets to its common box");
        Check(EvaluateFrame(attack, 9).hasAttackBox, "invincibility can also have an attack box");
        Check(!EvaluateFrame(attack, 12).invincible && !EvaluateFrame(attack, 12).hasAttackBox &&
            EvaluateFrame(attack, 12).hurtbox.center.x == 5, "tail clears attacks and invincibility only");
        attack.dataType = DataType::Invincible; attack.completed = true;
        Check(SaveWarnings(attack).empty(), "invincible requirement");
        Phase active; active.type = PhaseType::Attack; active.frames = 2;
        attack.phases.push_back(active);
        Check(SaveWarnings(attack).size() == 1, "each attack phase requires its dedicated box");
        attack.phases.back().hasAttackBox = true;
        Check(SaveWarnings(attack).empty(), "warning cleared after enabling box");
        const std::string path = std::string(argv[1]) + "/roundtrip.nattack";
        std::string error;
        Check(SaveAttack(path, attack, &error), "save v2");
        AttackDefinition loaded;
        Check(LoadAttack(path, loaded, &error), "load v2");
        Check(loaded.phases.size() == 3 && loaded.completed && loaded.dataType == DataType::Invincible,
            "metadata roundtrip");
        Check(loaded.phases[0].keys.size() == 1 && loaded.phases[0].keys[0].frame == 6, "frame key roundtrip");
        Check(EvaluateFrame(loaded, 10).attackBox.size.z == 3, "box half extents roundtrip");
        Catalog catalog; catalog.attacks.push_back(loaded);
        Check(Validate(catalog).empty(), "v2 validation");
        ResizeTimeline(loaded, 11);
        Check(loaded.phases.size() == 1 && loaded.phases[0].frames == 9, "delete entire overflowing phases");
        ResizeTimeline(loaded, 9);
        Check(loaded.phases.size() == 1, "exactly fitting phase preserved");
        ResizeTimeline(loaded, 8);
        Check(loaded.phases.empty(), "partially overflowing phase removed");
        HitboxKey a{0, {1, 1, 1}, {1, 2, 3}}, b{0, {0, 0, 0}, {0, 0, 0}};
        Check(BoxesOverlap(a, {}, 0, b, {0, -1, -2}, 0), "half extent example minimum corner");
        Check(BoxesOverlap(a, {}, 0, b, {2, 3, 4}, 0), "half extent example maximum corner");
        Check(!BoxesOverlap(a, {}, 0, b, {2.01f, 3, 4}, 0), "outside half extent");
        a.center = {}; a.size = {2, 1, 0.1f}; b.size = {0.1f, 0.1f, 0.1f};
        Check(!BoxesOverlap(a, {}, 0, b, {0, 0, 1}, 0), "unrotated box misses");
        Check(BoxesOverlap(a, {}, 1.5707963f, b, {0, 0, 1}, 0), "rotated box hits");
        Check(LoadAttack(argv[2], loaded, &error), "legacy fixture loads");
        Check(loaded.phases.size() == 3 && loaded.phases[1].hasAttackBox, "legacy phase migration");
        Check(std::fabs(loaded.phases[1].attackBox.size.x - 0.25f) < 0.0001f, "legacy full size becomes half size");
        std::cout << "Combat timeline tests passed.\n";
        return 0;
    }
    catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}

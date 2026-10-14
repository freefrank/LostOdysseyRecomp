// Model import material slots: a glTF material that names several slots of the
// original picks the slot the original draws with (11 party models have slots 0
// and 1 on the same material, and only slot 1 is drawn).
#include "modding/model_slot.h"

#include <iostream>
#include <stdexcept>

using namespace modding::model_slot;

static void Require(bool value, const char *why)
{
    if (!value) throw std::runtime_error(why);
}

int main()
{
    try
    {
        // Party model without tears: slots 0 and 1 are the same material, sections use slot 1.
        const std::vector<std::string> party{"pc_000c0_body", "pc_000c0_body", "pc_000c0_face"};
        const std::set<uint16_t> drawn{1, 2};
        // The exporter writes one material per slot, so the file may carry the body as material 0 or 1.
        Require(ChooseSlot(party, drawn, "pc_000c0_body", 0) == 1, "body as material 0 must map to the drawn slot 1");
        Require(ChooseSlot(party, drawn, "pc_000c0_body", 1) == 1, "body as material 1 must map to slot 1");
        Require(ChooseSlot(party, drawn, "PC_000C0_Body.001", 5) == 1, "Blender's .001 copy must map to slot 1");
        Require(ChooseSlot(party, drawn, "pc_000c0_face", 2) == 2, "a unique name keeps its slot");

        // Both slots drawn: the file's material number decides, else the first.
        const std::set<uint16_t> both{0, 1, 2};
        Require(ChooseSlot(party, both, "pc_000c0_body", 1) == 1, "material 1 picks slot 1 when both are drawn");
        Require(ChooseSlot(party, both, "pc_000c0_body", 3) == 0, "otherwise the first drawn slot");

        // Neither drawn (or nothing known): the file's number, then the first match.
        Require(ChooseSlot(party, {}, "pc_000c0_body", 1) == 1, "no drawn slot: the material number");
        Require(ChooseSlot(party, {}, "pc_000c0_body", 7) == 0, "no drawn slot: the first match");

        Require(!ChooseSlot(party, drawn, "other", 0), "an unknown name maps nowhere");
    }
    catch (const std::exception &error)
    {
        std::cerr << "model_slot_test: " << error.what() << '\n';
        return 1;
    }
    std::cout << "model_slot_test passed\n";
    return 0;
}

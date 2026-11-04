#pragma once
#include "cheats.h"
#include "fast_forward.h"
#include "no_encounters.h"
#include "battle_exp.h"
#include "../host_ui/rasterizer.h"
#include "../settings/config.h"
#include "controller_hint.h"
#include "translations.h"
#include <string>

namespace debug_menu::cheat_overlay {
using cheats::Action;
using cheats::Request;
using translations::Pick;
// Tables below are indexed by the debug menu's language: English, Simplified Chinese, Spanish.
enum class Nav { Up, Down, Left, Right, Confirm, Cancel, PrevCategory, NextCategory };
enum class Picker { None, Character, Item, Skill, Equipment };
inline constexpr const wchar_t* Categories[][3] = {
    {L"Quick tools",L"快捷操作",L"Atajos"}, {L"Characters",L"角色与技能",L"Personajes"},
    {L"Inventory",L"物品与素材",L"Inventario"}, {L"Equipment",L"装备",L"Equipo"},
    {L"Party",L"队伍",L"Grupo"}, {L"Developer",L"开发者",L"Desarrollador"}};
inline constexpr unsigned Quantities[] = {1,10,50,99};
inline constexpr int QuickToolRowHeight = 22;
inline constexpr const wchar_t* FastForwardButtonLabel[] = {L"Fast-forward button",L"变速按键",L"Botón de avance"};
inline constexpr const wchar_t* FastForwardButtonValues[] = {L"LT",L"LB"};
inline constexpr const wchar_t* FastForwardAudioLabel[] = {L"Fast-forward audio",L"变速音频",L"Audio del avance"};
inline constexpr const wchar_t* FastForwardAudioValues[][3] = {
    {L"Keep pitch",L"保持音调",L"Mantener tono"}, {L"Speed up",L"随速度升调",L"Acelerar tono"}};
inline constexpr const wchar_t* FastForwardAudioHelp[] = {
    L"Keep pitch preserves voices and music; Speed up raises pitch with speed.",
    L"保持音调让人声和音乐维持原调；随速度升调会按加速倍率提高音调。",
    L"Mantener tono conserva las voces y la música; Acelerar tono sube el tono con la velocidad."};
inline constexpr const wchar_t* EncounterRateLabel[] = {L"Encounter rate",L"遇敌率",L"Encuentros"};
inline constexpr const wchar_t* EncounterRateValues[][3] = {
    {L"Normal",L"正常",L"Normal"}, {L"Half",L"减半",L"Mitad"}, {L"None",L"关闭",L"Ninguno"}, {L"Every step",L"一步一遇敌",L"Cada paso"}};
inline unsigned EncounterRateIndex() {
    return debug_menu::EncounterEveryStepEnabled() ? 3 : unsigned(debug_menu::GetEncounterRate());
}
inline constexpr const wchar_t* BattleExpLabel[] = {L"Battle EXP ×2",L"战斗 EXP ×2",L"EXP de combate ×2"};
inline constexpr const wchar_t* MenuAnimationLabel[] = {L"Menu animations",L"菜单动画",L"Animación de menús"};
inline constexpr const wchar_t* MenuAnimationValues[][3] = {
    {L"Normal (1x)",L"正常 (1x)",L"Normal (1x)"}, {L"Fast (2x)",L"快速 (2x)",L"Rápida (2x)"},
    {L"Faster (4x)",L"更快 (4x)",L"Más rápida (4x)"}};
inline constexpr const wchar_t* MenuAnimationHelp[] = {
    L"Camp menu transitions only; world and audio keep their speed.",
    L"仅加速营地菜单切换；世界和音频保持原速。",
    L"Solo el menú del campamento; el mundo y el audio mantienen su velocidad."};
inline const wchar_t* MenuAnimationValue(unsigned rate, uint32_t lang) {
    for (unsigned i=0;i<std::size(settings::MenuAnimationRates);++i)
        if (settings::MenuAnimationRates[i]==rate) return MenuAnimationValues[i][lang];
    return MenuAnimationValues[0][lang];
}
// The switches' values; Chinese keeps the English ones.
inline const wchar_t* OnOff(bool on, uint32_t lang) {
    return on ? Pick(lang,L"ON",L"ON",L"SÍ") : Pick(lang,L"OFF",L"OFF",L"NO");
}
inline unsigned Wrap(unsigned value, int direction, unsigned count) {
    return unsigned((int(value)+direction+int(count))%int(count));
}
struct Model {
    unsigned category=0, row=0, character=0, inventory=0, item=0, quantity=3,
        skill=0, equipmentSlot=0, equipment=0, partySlot=0, exp=99, backRow=0;
    Picker picker=Picker::None;
    unsigned picked=0;
    bool confirm=false, yes=false;
    enum class Confirmation { Write, Enable, Editor } confirmation=Confirmation::Write;
    Request request;
    std::wstring notice;
    unsigned menuAnimationRate=1;
    unsigned Rows() const { constexpr unsigned rows[]={13,8,6,5,7,2}; return rows[category]; }
    void ChangeCategory(int direction) { category=Wrap(category,direction,unsigned(std::size(Categories))); row=0; }
    std::span<const cheats::data::Entry> EquipmentList() const {
        if (equipmentSlot==0) return cheats::data::Weapons;
        if (equipmentSlot==1) return cheats::data::Rings;
        return cheats::data::Accessories;
    }
    std::span<const cheats::data::Entry> Items() const {
        return inventory ? std::span<const cheats::data::Entry>(cheats::data::Materials)
            : std::span<const cheats::data::Entry>(cheats::data::Items);
    }
    unsigned PickCount() const {
        switch(picker) {
        case Picker::Character:return 9;
        case Picker::Item:return unsigned(Items().size());
        case Picker::Skill:return unsigned(std::size(cheats::data::LearnedSkills));
        case Picker::Equipment:return unsigned(EquipmentList().size());
        default:return 0;
        }
    }
    const wchar_t* PickName(unsigned index) const {
        switch(picker) {
        case Picker::Character:return cheats::data::Characters[index];
        case Picker::Item:return Items()[index].name;
        case Picker::Skill:return cheats::data::LearnedSkills[index].name;
        case Picker::Equipment:return EquipmentList()[index].name;
        default:return L"";
        }
    }
    void Open(Picker p, unsigned selected) { picker=p; picked=selected; }
    void Dismiss() { picker=Picker::None; confirm=false; yes=false; notice.clear(); }
    void Send(Request r, uint32_t lang) {
        const bool queued=cheats::session.Queue(r);
        // The service result owns Pending/Applied status; do not leave a stale
        // "queued" notice covering the actual completion after reopening F1.
        if (queued) notice.clear();
        else if (cheats::session.HasPending())
            notice=Pick(lang, L"Another change is pending; close F1 first", L"已有待执行修改，请先关闭 F1", L"Hay otro cambio pendiente; cierra F1 antes");
        else notice=Pick(lang, L"Not queued: scene changed or memory edits are disabled", L"未执行：场景已变化或修改未启用", L"No encolado: la escena ha cambiado o la edición de memoria está desactivada");
    }
    void Write(Action a, unsigned value, unsigned index, bool needsConfirmation, uint32_t lang) {
        const auto s=cheats::session.Get();
        if (!s.enabled || !s.available) {
            notice=Pick(lang, L"Enable memory edits; a loaded, out-of-battle field is required", L"先启用内存修改；仅支持已加载的非战斗场景", L"Activa la edición de memoria; hace falta un campo cargado fuera de combate");
            return;
        }
        request={a,character,index,value,s.generation};
        if (needsConfirmation) { confirmation=Confirmation::Write; confirm=true; yes=false; }
        else Send(request,lang);
    }
    // Returns false only for B/Esc at the root; nested pickers/confirmation
    // consume Cancel so it never closes F1 or applies a draft accidentally.
    bool Input(Nav nav, uint32_t lang) {
        if (confirm) {
            if (nav==Nav::Cancel) { confirm=false; return true; }
            if (nav==Nav::Left || nav==Nav::Right) yes=!yes;
            if (nav==Nav::Confirm) {
                confirm=false;
                if (yes) {
                    if (confirmation==Confirmation::Write) Send(request,lang);
                    else if (confirmation==Confirmation::Enable) cheats::session.Enable(true);
                    else if (!cheats::session.SetEditor(true))
                        notice=Pick(lang, L"Retail editor unavailable", L"编辑器暂不可用", L"Editor original no disponible");
                }
            }
            return true;
        }
        if (picker!=Picker::None) {
            if (nav==Nav::Cancel) { picker=Picker::None; return true; }
            const unsigned count=PickCount();
            if (nav==Nav::Up && picked>0) --picked;
            if (nav==Nav::Down && picked+1<count) ++picked;
            if (nav==Nav::Left) picked=picked>6 ? picked-6 : 0;
            if (nav==Nav::Right) picked=std::min(count-1,picked+6);
            if (nav==Nav::Confirm) {
                switch(picker) {
                case Picker::Character:character=picked;break;
                case Picker::Item:item=picked;break;
                case Picker::Skill:skill=picked;break;
                case Picker::Equipment:equipment=picked;break;
                default:break;
                }
                picker=Picker::None;
            }
            return true;
        }
        if (nav==Nav::PrevCategory || nav==Nav::NextCategory) {
            ChangeCategory(nav==Nav::PrevCategory ? -1 : 1);
            return true;
        }
        if (nav==Nav::Cancel) return false;
        if (nav==Nav::Up) { if (row>0) --row; return true; }
        if (nav==Nav::Down) { if (row+1<Rows()) ++row; return true; }
        const bool activate=nav==Nav::Confirm;
        const int direction=nav==Nav::Left ? -1 : 1;
        if (row==0) { ChangeCategory(direction); return true; }
        if (category==0) {
            const auto speed=fast_forward::GetStatus();
            if (row==1) fast_forward::Enable(!speed.enabled);
            else if (row==2) fast_forward::SetMode(speed.mode==fast_forward::Mode::Hold
                ? fast_forward::Mode::Toggle : fast_forward::Mode::Hold);
            else if (row==3) fast_forward::SetButton(fast_forward::Button(
                Wrap(unsigned(speed.button),direction,unsigned(std::size(FastForwardButtonValues)))));
            else if (row==4) {
                unsigned index=0;
                for(unsigned i=0;i<std::size(fast_forward::Rates);++i) if(fast_forward::Rates[i]==speed.multiplier)index=i;
                fast_forward::SetRate(fast_forward::Rates[Wrap(index,direction,unsigned(std::size(fast_forward::Rates)))]);
            } else if (row==5) {
                fast_forward::SetKeepAudioPitch(!speed.keepAudioPitch);
            } else if (row==6) {
                const unsigned rate=Wrap(EncounterRateIndex(),direction,unsigned(std::size(EncounterRateValues)));
                if (rate==3) debug_menu::SetEncounterEveryStepEnabled(true);
                else debug_menu::SetEncounterRate(settings::EncounterRate(rate));
            } else if (row==7) {
                debug_menu::SetBattleExpDoubleEnabled(!debug_menu::BattleExpDoubleEnabled());
            } else if (row==8) {
                unsigned index=0;
                for(unsigned i=0;i<std::size(settings::MenuAnimationRates);++i)
                    if(settings::MenuAnimationRates[i]==menuAnimationRate)index=i;
                menuAnimationRate=settings::MenuAnimationRates[Wrap(index,direction,unsigned(std::size(settings::MenuAnimationRates)))];
            } else if (row==9 && activate) {
                if (cheats::session.Get().enabled) cheats::session.Enable(false);
                else { confirmation=Confirmation::Enable; confirm=true; yes=false; }
            } else if (activate && row==10) Write(Action::AddGold,100000,0,false,lang);
            else if (activate && row==11) Write(Action::SetGold,cheats::GoldLimit,0,true,lang);
            else if (activate && row==12) Write(Action::HealParty,0,0,false,lang);
        } else if (category==1) {
            if(row==1) { if(activate)Open(Picker::Character,character);else character=Wrap(character,direction,9); }
            else if(row==2 && activate) Write(Action::Heal,0,0,false,lang);
            else if(row==3) exp=unsigned(std::clamp(int(exp)+direction*(activate ? 10 : 1),0,99));
            else if(row==4 && activate) Write(Action::SetExp,exp,0,true,lang);
            else if(row==5) { if(activate)Open(Picker::Skill,skill);else skill=Wrap(skill,direction,unsigned(std::size(cheats::data::LearnedSkills))); }
            else if(row==6 && activate) Write(Action::LearnSkill,0,skill,true,lang);
            else if(row==7 && activate) Write(Action::LearnSupportedSkills,0,0,true,lang);
        } else if(category==2) {
            if(row==1) { inventory=1-inventory;item=0; }
            else if(row==2) { if(activate)Open(Picker::Item,item);else item=Wrap(item,direction,unsigned(Items().size())); }
            else if(row==3) quantity=Wrap(quantity,direction,unsigned(std::size(Quantities)));
            else if(row==4 && activate) Write(inventory ? Action::Material:Action::Item,Quantities[quantity],item,false,lang);
            else if(row==5 && activate) Write(inventory ? Action::AllMaterials:Action::AllItems,Quantities[quantity],0,true,lang);
        } else if(category==3) {
            if(row==1) { if(activate)Open(Picker::Character,character);else character=Wrap(character,direction,9); }
            else if(row==2) { equipmentSlot=Wrap(equipmentSlot,direction,10);equipment=0; }
            else if(row==3) { if(activate)Open(Picker::Equipment,equipment);else equipment=Wrap(equipment,direction,unsigned(EquipmentList().size())); }
            else if(row==4 && activate) Write(equipmentSlot==0 ? Action::SetWeapon : equipmentSlot==1 ? Action::SetRing : Action::SetAccessory,
                EquipmentList()[equipment].value,equipmentSlot<2 ? 0 : equipmentSlot-2,true,lang);
        } else if(category==4) {
            if(row==1)partySlot=Wrap(partySlot,direction,5);
            else if(row==2) { if(activate)Open(Picker::Character,character);else character=Wrap(character,direction,9); }
            else if(row==3 && activate)Write(Action::SetFormation,character,partySlot,true,lang);
            else if(row==4)backRow=1-backRow;
            else if(row==5 && activate)Write(Action::SetRow,backRow ? 0x80:0,0,true,lang);
            else if(row==6 && activate)Write(Action::SetFieldCharacter,character,0,true,lang);
        } else if(category==5 && row==1 && activate) {
            const auto s=cheats::session.Get();
            if(s.editorRequested)cheats::session.SetEditor(false);
            else if(s.enabled && s.available) { confirmation=Confirmation::Editor;confirm=true;yes=false; }
            else notice=Pick(lang, L"Enable memory edits and enter a playable field first", L"请先启用内存修改并进入可控制场景", L"Activa la edición de memoria y entra antes en un campo jugable");
        }
        return true;
    }
};
inline std::wstring Fit(host_ui::Rasterizer& r, std::wstring text, int width) {
    if(r.MeasureWString(text)<=width)return text;
    while(!text.empty() && r.MeasureWString(text+L"...")>width)text.pop_back();
    return text+L"...";
}
inline const wchar_t* ResultText(cheats::Result result,uint32_t lang) {
    switch(result) {
    case cheats::Result::Pending:return Pick(lang, L"Pending - close F1 to apply", L"已排队，关闭 F1 后执行", L"Pendiente: cierra F1 para aplicarlo");
    case cheats::Result::Applied:return Pick(lang, L"Change applied", L"修改已执行", L"Cambio aplicado");
    case cheats::Result::Cancelled:return Pick(lang, L"Scene changed - request cancelled", L"场景发生变化，修改已取消", L"La escena ha cambiado: petición cancelada");
    case cheats::Result::Invalid:return Pick(lang, L"Invalid data or target - no writes", L"数据或目标无效，未写入", L"Datos o destino no válidos: no se escribe nada");
    case cheats::Result::Disabled:return Pick(lang, L"Memory edits disabled", L"内存修改已关闭", L"Edición de memoria desactivada");
    case cheats::Result::Ready:return Pick(lang, L"Out-of-battle data ready", L"非战斗场景已就绪", L"Datos fuera de combate listos");
    default:return Pick(lang, L"Waiting for a loaded, playable field", L"等待读档完成并进入可控制场景", L"Esperando un campo cargado y jugable");
    }
}
inline const wchar_t* QuickToolHelp(unsigned row,uint32_t lang) {
    if(row==3) return Pick(lang,
        L"Pairs: LT/RT, LB/RB. Holding both blocks boost.",
        L"配对按键：LT/RT、LB/RB；同时按住所选按键及其配对键时不加速。",
        L"Parejas: LT/RT, LB/RB. Mantener ambos impide acelerar.");
    if(row==5) return FastForwardAudioHelp[lang];
    if(row==6) return Pick(lang,
        L"Walking only. Half skips alternate draws; Every step is not saved. Story and boss fights are unchanged.",
        L"仅影响步行遇敌；减半交替跳过成功判定，一步一遇敌不保存。剧情与首领战不受影响。",
        L"Solo al caminar. Mitad salta una tirada de cada dos; Cada paso no se guarda. La historia y los jefes no cambian.");
    if(row==7) return Pick(lang,
        L"Doubles battle EXP. SP and gold stay unchanged.",
        L"战斗经验加倍，SP 与金币不变。",
        L"Duplica la EXP de combate. SP y oro no cambian.");
    if(row==8) return MenuAnimationHelp[lang];
    return nullptr;
}
inline void Render(host_ui::Rasterizer& r,const Model& m,uint32_t lang,int x,int y,int width,bool playStation) {
    const auto s=cheats::session.Get(); const auto speed=fast_forward::GetStatus();
    const auto color=host_ui::MakeColor;
    const auto text=color(255,226,233,243),muted=color(255,145,158,178),gold=color(255,229,196,126);
    const int side=142,gap=16,rx=x+side+gap,rw=width-side-gap,rowH=m.category==0 ? QuickToolRowHeight:35;
    auto drawText=[&](int px,int py,std::wstring t,int maxWidth,uint32_t c) {
        r.DrawWString(px,py,Fit(r,controller_hint::ShoulderLabels(std::move(t),playStation),maxWidth),c);
    };
    for(unsigned i=0;i<6;++i) {
        const bool selected=m.category==i;
        r.FillRect(x,y+int(i)*39,side,32,selected ? color(255,47,59,76):color(255,27,32,41));
        if(selected)r.FillRect(x,y+int(i)*39,3,32,gold);
        drawText(x+12,y+int(i)*39+8,Categories[i][lang],side-20,selected ? gold:muted);
    }
    drawText(x,y+248,Pick(lang, L"OFF by default", L"默认关闭", L"Por defecto: NO"),side,muted);
    drawText(x,y+270,Pick(lang, L"Back up saves", L"先备份存档", L"Copia partidas"),side,gold);
    auto row=[&](unsigned id,const wchar_t* label,std::wstring value,bool enabled=true) {
        const int py=y+int(id)*rowH;
        const bool selected=m.row==id;
        const int height=m.category==0 ? 21:30, textY=py+(m.category==0 ? 4:7);
        r.FillRect(rx,py,rw,height,selected ? color(255,46,57,72):color(255,29,34,43));
        if(selected)r.FillRect(rx,py,3,height,gold);
        const auto c=enabled ? text:muted;
        if(value.empty())drawText(rx+12,textY,label,rw-24,c);
        else {
            const int labelWidth=m.category==0 ? 192:180;
            drawText(rx+12,textY,label,labelWidth-10,c);
            drawText(rx+labelWidth,textY,std::move(value),rw-labelWidth-12,selected ? gold:c);
        }
    };
    row(0,Pick(lang, L"Category  < / >", L"类别  < / >", L"Categoría  < / >"),Categories[m.category][lang]);
    const bool writable=s.enabled && s.available;
    const auto charName=cheats::data::Characters[m.character];
    if(m.category==0) {
        row(1,Pick(lang, L"Fast-forward", L"变速", L"Avance rápido"),OnOff(speed.enabled,lang));
        row(2,Pick(lang, L"Speed mode", L"变速模式", L"Modo de velocidad"),speed.mode==fast_forward::Mode::Hold
            ? Pick(lang, L"Hold", L"按住", L"Mantener") : Pick(lang, L"Toggle", L"切换", L"Alternar"));
        row(3,FastForwardButtonLabel[lang],FastForwardButtonValues[unsigned(speed.button)]);
        row(4,Pick(lang, L"Speed multiplier", L"加速倍率", L"Multiplicador"),std::to_wstring(speed.multiplier)+L"x");
        row(5,FastForwardAudioLabel[lang],FastForwardAudioValues[speed.keepAudioPitch ? 0:1][lang]);
        row(6,EncounterRateLabel[lang],EncounterRateValues[EncounterRateIndex()][lang]);
        row(7,BattleExpLabel[lang],OnOff(debug_menu::BattleExpDoubleEnabled(),lang));
        row(8,MenuAnimationLabel[lang],MenuAnimationValue(m.menuAnimationRate,lang));
        row(9,Pick(lang, L"Allow memory edits", L"允许内存修改", L"Editar la memoria"),OnOff(s.enabled,lang));
        row(10,Pick(lang, L"Add 100,000 gold", L"增加 100,000 金币", L"Añadir 100.000 de oro"),L"",writable);
        row(11,Pick(lang, L"Set gold to 9,999,999", L"金币设为 9,999,999", L"Fijar el oro en 9.999.999"),L"",writable);
        row(12,Pick(lang, L"Restore party HP / MP", L"恢复当前队伍 HP / MP", L"Restaurar HP / MP del grupo"),L"",writable);
    } else if(m.category==1) {
        row(1,Pick(lang, L"Character", L"角色", L"Personaje"),charName);
        row(2,Pick(lang, L"Restore HP / MP", L"恢复 HP / MP", L"Restaurar HP / MP"),L"",writable);
        row(3,Pick(lang, L"EXP target (0-99)", L"EXP 目标（0-99）", L"EXP deseada (0-99)"),std::to_wstring(m.exp));
        row(4,Pick(lang, L"Apply EXP", L"应用 EXP", L"Aplicar EXP"),L"",writable);
        row(5,Pick(lang, L"Choose skill", L"选择技能", L"Elegir habilidad"),cheats::data::LearnedSkills[m.skill].name);
        row(6,Pick(lang, L"Learn selected skill", L"习得所选技能", L"Aprender la habilidad elegida"),L"",writable);
        row(7,Pick(lang, L"Learn known skills", L"习得已核对技能", L"Aprender conocidas"),std::to_wstring(std::size(cheats::data::LearnedSkills)),writable);
    } else if(m.category==2) {
        row(1,Pick(lang, L"Inventory type", L"物品类型", L"Tipo de inventario"),m.inventory ? Pick(lang, L"Materials", L"素材", L"Materiales"):Pick(lang, L"Items", L"物品", L"Objetos"));
        row(2,Pick(lang, L"Choose item", L"选择条目", L"Elegir objeto"),m.Items()[m.item].name);
        row(3,Pick(lang, L"Target quantity", L"目标数量", L"Cantidad deseada"),std::to_wstring(Quantities[m.quantity]));
        row(4,Pick(lang, L"Apply selected item", L"应用所选条目", L"Aplicar el objeto elegido"),L"",writable);
        row(5,Pick(lang, L"Fill listed items", L"填充此类已知条目", L"Rellenar listados"),std::to_wstring(m.Items().size()),writable);
    } else if(m.category==3) {
        row(1,Pick(lang, L"Character", L"角色", L"Personaje"),charName);
        std::wstring slot=m.equipmentSlot==0 ? Pick(lang, L"Weapon", L"武器", L"Arma") : m.equipmentSlot==1 ? Pick(lang, L"Ring", L"指环", L"Anillo")
            : std::wstring(Pick(lang, L"Accessory ", L"饰品 ", L"Accesorio "))+std::to_wstring(m.equipmentSlot-1);
        row(2,Pick(lang, L"Equipment slot", L"装备槽", L"Ranura de equipo"),slot);
        row(3,Pick(lang, L"Choose equipment", L"选择装备", L"Elegir equipo"),m.EquipmentList()[m.equipment].name);
        row(4,Pick(lang, L"Apply equipment (experimental)", L"应用装备（实验性）", L"Aplicar el equipo (experimental)"),L"",writable);
    } else if(m.category==4) {
        row(1,Pick(lang, L"Party position", L"队伍位置", L"Puesto en el grupo"),std::to_wstring(m.partySlot+1));
        row(2,Pick(lang, L"Character", L"角色", L"Personaje"),charName);
        row(3,Pick(lang, L"Swap / assign position", L"交换 / 加入该位置", L"Intercambiar / asignar el puesto"),L"",writable);
        row(4,Pick(lang, L"Battle row", L"前后排", L"Fila de combate"),m.backRow ? Pick(lang, L"Back", L"后排", L"Trasera"):Pick(lang, L"Front", L"前排", L"Delantera"));
        row(5,Pick(lang, L"Apply character row", L"应用角色前后排", L"Aplicar la fila del personaje"),L"",writable);
        row(6,Pick(lang, L"Set field character (experimental)", L"设为场景角色（实验性）", L"Fijar el personaje de campo (experimental)"),L"",writable);
    } else {
        row(1,Pick(lang, L"Retail field editor", L"原版场景编辑器", L"Editor original"),s.editorRequested ? (s.editorApplied ? OnOff(true,lang) : Pick(lang,L"PENDING",L"PENDING",L"PENDIENTE"))
            : (s.editorApplied ? Pick(lang,L"STOPPING",L"STOPPING",L"DETENIENDO") : OnOff(false,lang)),writable);
        drawText(rx+12,y+94,Pick(lang, L"Experimental: try LT + RT for retail EDIT MENU", L"实验性：LT + RT 尝试打开原版 EDIT MENU", L"Experimental: prueba LT + RT para el EDIT MENU original"),rw-24,gold);
        drawText(rx+12,y+123,Pick(lang, L"Not the prototype debug menu", L"不是 Prototype Debug Menu", L"No es el menú de depuración del prototipo"),rw-24,muted);
        drawText(rx+12,y+152,Pick(lang, L"Exit the retail editor before disabling this switch", L"先退出原版编辑器，再关闭此开关", L"Sal del editor original antes de desactivar esto"),rw-24,muted);
    }
    const wchar_t* hints[][3]={
        {speed.mode==fast_forward::Mode::Hold
            ? L"Hold the selected button to boost; release for 1x. Paired buttons do not boost."
            : L"Press the selected button to toggle boost. Paired buttons do not boost.",
         speed.mode==fast_forward::Mode::Hold
            ? L"按住所选按键加速，松开恢复原速；同时按住配对按键时不加速。"
            : L"按所选按键切换加速；同时按住配对按键时不加速。",
         speed.mode==fast_forward::Mode::Hold
            ? L"Mantén el botón elegido para acelerar; suéltalo para volver a 1x. Los botones emparejados no aceleran."
            : L"Pulsa el botón elegido para activar o quitar la aceleración. Los botones emparejados no aceleran."},
        {L"HP/MP are out-of-battle values. EXP is progress, not a level selector.",L"HP/MP 为非战斗数值；EXP 是经验进度，不是等级。",
         L"HP/MP son valores fuera de combate. EXP es el progreso, no un selector de nivel."},
        {L"Choose item: A opens a paged list. Sort the inventory once to refresh.",L"按 A 打开可翻页列表；修改后在游戏内整理物品以刷新。",
         L"Elegir objeto: A abre una lista por páginas. Ordena el inventario una vez para actualizarlo."},
        {L"Only existing accessory slots. Check character compatibility before applying.",L"仅能修改已有饰品槽；应用前请确认角色适用的装备。",
         L"Solo las ranuras de accesorio existentes. Comprueba la compatibilidad del personaje antes de aplicar."},
        {L"Assigning a present member swaps slots. Field character may require reloading.",L"重复成员会交换位置；场景角色修改可能需要重新读档。",
         L"Asignar a un miembro ya presente intercambia los puestos. El personaje de campo puede requerir recargar."},
        {L"Guest flag only. No executable patches. Leave this OFF during normal play.",L"只修改 guest 标记，不改宿主机器码。正常游玩建议关闭。",
         L"Solo una marca del juego, sin parches al ejecutable. Déjalo en NO al jugar con normalidad."}};
    // Warnings wrap instead of disappearing behind an ellipsis.
    auto paragraph=[&](int px,int py,std::wstring value,int maxWidth,uint32_t c) {
        value=controller_hint::ShoulderLabels(std::move(value),playStation);
        for(unsigned line=0;!value.empty() && line<2;++line) {
            size_t n=value.size();
            while(n>1 && r.MeasureWString(value.substr(0,n))>maxWidth)--n;
            if(n<value.size()) { const auto space=value.rfind(L' ',n); if(space!=std::wstring::npos && space>n/2)n=space; }
            drawText(px,py+int(line)*19,value.substr(0,n),maxWidth,c);
            value.erase(0,n);while(!value.empty() && value.front()==L' ')value.erase(0,1);
        }
    };
    const auto quickHelp=m.category==0 ? QuickToolHelp(m.row,lang):nullptr;
    paragraph(x,y+296,quickHelp ? quickHelp
        : (playStation && m.category==2)
        ? Pick(lang, L"Choose item: Confirm opens a paged list. Sort the inventory once to refresh.", L"选择物品：按确定键打开可翻页列表；修改后在游戏内整理物品以刷新。", L"Elegir objeto: Confirmar abre una lista por páginas. Ordena el inventario una vez para actualizarlo.")
        : hints[m.category][lang],width,muted);
    std::wstring status=ResultText(s.result,lang);
    if(s.available) {
        if(m.category==1 || m.category==3) {
            const auto& c=s.characters[m.character];
            status+=L" | HP "+std::to_wstring(unsigned(c.hp))+L"/"+std::to_wstring(unsigned(c.maxHp))+
                L" MP "+std::to_wstring(unsigned(c.mp))+L"/"+std::to_wstring(unsigned(c.maxMp));
        } else status+=Pick(lang,L" | Gold: ",L" | Gold: ",L" | Oro: ")+std::to_wstring(s.gold);
    }
    drawText(x,y+340,status,width,s.result==cheats::Result::Invalid ? gold:text);
    if(!m.notice.empty())drawText(x,y+366,m.notice,width,gold);
    if(m.picker!=Picker::None) {
        r.FillRect(x,y,width,365,color(252,24,31,42));
        drawText(x+16,y+12,Pick(lang, L"Select an entry   Left / Right: page", L"选择条目   左 / 右：翻页", L"Elige una entrada   Izquierda / Derecha: página"),width-32,gold);
        const unsigned count=m.PickCount(),first=(m.picked/6)*6;
        for(unsigned i=first;i<std::min(count,first+6);++i) {
            const int py=y+49+int(i-first)*41;
            const bool selected=i==m.picked;
            r.FillRect(x+12,py,width-24,34,selected ? color(255,50,64,82):color(255,30,38,49));
            drawText(x+24,py+9,m.PickName(i),width-48,selected ? gold:text);
        }
        drawText(x+18,y+309,std::to_wstring(m.picked+1)+L" / "+std::to_wstring(count),width-36,muted);
        if(playStation) {
            int px=controller_hint::Draw(r,x+18,y+337,true,hid::prompts::Face::A,
                Pick(lang, L" / Enter: select", L" / Enter：选择", L" / Enter: elegir"),muted)+20;
            controller_hint::Draw(r,px,y+337,true,hid::prompts::Face::B,
                Pick(lang, L" / Esc: back (no writes)", L" / Esc：返回（不修改）", L" / Esc: atrás (sin escribir)"),muted);
        } else drawText(x+18,y+337,Pick(lang, L"A / Enter: select   B / Esc: back (no writes)", L"A / Enter：选择   B / Esc：返回（不修改）", L"A / Enter: elegir   B / Esc: atrás (sin escribir)"),width-36,muted);
    }
    if(m.confirm) {
        r.FillRect(x,y,width,365,color(250,24,31,42));
        drawText(x+24,y+39,Pick(lang, L"Confirm this change?", L"确认这项修改？", L"¿Confirmar este cambio?"),width-48,gold);
        paragraph(x+24,y+93,Pick(lang, L"Changes can enter your save; switching OFF does not undo edits.", L"修改可能进入存档，关闭开关不会撤销已写入数值。", L"Los cambios pueden quedar en tu partida; ponerlo en NO no deshace lo escrito."),width-48,text);
        paragraph(x+24,y+140,Pick(lang, L"Back up your save. Bulk edits touch only catalogued entries.", L"请先备份存档。批量修改只处理已列出的条目。", L"Haz copia de tu partida. Las ediciones masivas solo tocan entradas catalogadas."),width-48,text);
        paragraph(x+24,y+182,Pick(lang, L"Applied on the game thread after closing F1; cancelled on scene changes.", L"关闭 F1 后由游戏线程执行；场景变化时取消。", L"Se aplica en el hilo del juego al cerrar F1; se cancela si cambia la escena."),width-48,muted);
        const int bw=(width-64)/2,py=y+231;
        for(int i=0;i<2;++i) {
            const bool selected=m.yes==(i==1);
            const int px=x+24+i*(bw+16);
            r.FillRect(px,py,bw,42,selected ? color(255,59,70,86):color(255,31,38,49));
            drawText(px+14,py+13,i ? Pick(lang, L"Confirm", L"确认", L"Confirmar"):Pick(lang, L"Cancel", L"取消", L"Cancelar"),bw-28,selected ? gold:text);
        }
        if(playStation) {
            int px=x+24;
            px+=r.DrawWString(px,y+311,Pick(lang, L"Left / Right: choose   ", L"左 / 右：选择   ", L"Izquierda / Derecha: elegir   "),muted);
            px=controller_hint::Draw(r,px,y+311,true,hid::prompts::Face::A,
                Pick(lang, L": accept", L"：确定", L": aceptar"),muted)+20;
            controller_hint::Draw(r,px,y+311,true,hid::prompts::Face::B,
                Pick(lang, L": cancel", L"：取消", L": cancelar"),muted);
        } else drawText(x+24,y+311,Pick(lang, L"Left / Right: choose   A: accept   B: cancel", L"左 / 右：选择   A：确定   B：取消", L"Izquierda / Derecha: elegir   A: aceptar   B: cancelar"),width-48,muted);
    }
}
} // namespace debug_menu::cheat_overlay

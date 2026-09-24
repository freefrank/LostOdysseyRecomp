#pragma once
#include "cheats.h"
#include "fast_forward.h"
#include "../host_ui/rasterizer.h"
#include <string>

namespace debug_menu::cheat_overlay {
using cheats::Action;
using cheats::Request;
enum class Nav { Up, Down, Left, Right, Confirm, Cancel };
enum class Picker { None, Character, Item, Skill, Equipment };
inline constexpr const wchar_t* Categories[][2] = {
    {L"Quick tools",L"快捷操作"}, {L"Characters",L"角色与技能"},
    {L"Inventory",L"物品与素材"}, {L"Equipment",L"装备"},
    {L"Party",L"队伍"}, {L"Developer",L"开发者"}};
inline constexpr unsigned Quantities[] = {1,10,50,99};
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
    unsigned Rows() const { constexpr unsigned rows[]={7,8,6,5,7,2}; return rows[category]; }
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
    void Send(Request r, bool zh) {
        const bool queued=cheats::session.Queue(r);
        // The service result owns Pending/Applied status; do not leave a stale
        // "queued" notice covering the actual completion after reopening F1.
        if (queued) notice.clear();
        else if (cheats::session.HasPending())
            notice=zh ? L"已有待执行修改，请先关闭 F1" : L"Another change is pending; close F1 first";
        else notice=zh ? L"未执行：场景已变化或修改未启用" : L"Not queued: scene changed or memory edits are disabled";
    }
    void Write(Action a, unsigned value, unsigned index, bool needsConfirmation, bool zh) {
        const auto s=cheats::session.Get();
        if (!s.enabled || !s.available) {
            notice=zh ? L"先启用内存修改；仅支持已加载的非战斗场景" : L"Enable memory edits; a loaded, out-of-battle field is required";
            return;
        }
        request={a,character,index,value,s.generation};
        if (needsConfirmation) { confirmation=Confirmation::Write; confirm=true; yes=false; }
        else Send(request,zh);
    }
    // Returns false only for B/Esc at the root; nested pickers/confirmation
    // consume Cancel so it never closes F1 or applies a draft accidentally.
    bool Input(Nav nav, bool zh) {
        if (confirm) {
            if (nav==Nav::Cancel) { confirm=false; return true; }
            if (nav==Nav::Left || nav==Nav::Right) yes=!yes;
            if (nav==Nav::Confirm) {
                confirm=false;
                if (yes) {
                    if (confirmation==Confirmation::Write) Send(request,zh);
                    else if (confirmation==Confirmation::Enable) cheats::session.Enable(true);
                    else if (!cheats::session.SetEditor(true))
                        notice=zh ? L"编辑器暂不可用" : L"Retail editor unavailable";
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
        if (nav==Nav::Cancel) return false;
        if (nav==Nav::Up) { if (row>0) --row; return true; }
        if (nav==Nav::Down) { if (row+1<Rows()) ++row; return true; }
        const bool activate=nav==Nav::Confirm;
        const int direction=nav==Nav::Left ? -1 : 1;
        if (row==0) { category=Wrap(category,direction,6); return true; }
        if (category==0) {
            const auto speed=fast_forward::GetStatus();
            if (row==1) fast_forward::Enable(!speed.enabled);
            else if (row==2) {
                unsigned index=0;
                for(unsigned i=0;i<std::size(fast_forward::Rates);++i) if(fast_forward::Rates[i]==speed.multiplier)index=i;
                fast_forward::SetRate(fast_forward::Rates[Wrap(index,direction,unsigned(std::size(fast_forward::Rates)))]);
            } else if (row==3 && activate) {
                if (cheats::session.Get().enabled) cheats::session.Enable(false);
                else { confirmation=Confirmation::Enable; confirm=true; yes=false; }
            } else if (activate && row==4) Write(Action::AddGold,100000,0,false,zh);
            else if (activate && row==5) Write(Action::SetGold,cheats::GoldLimit,0,true,zh);
            else if (activate && row==6) Write(Action::HealParty,0,0,false,zh);
        } else if (category==1) {
            if(row==1) { if(activate)Open(Picker::Character,character);else character=Wrap(character,direction,9); }
            else if(row==2 && activate) Write(Action::Heal,0,0,false,zh);
            else if(row==3) exp=unsigned(std::clamp(int(exp)+direction*(activate ? 10 : 1),0,99));
            else if(row==4 && activate) Write(Action::SetExp,exp,0,true,zh);
            else if(row==5) { if(activate)Open(Picker::Skill,skill);else skill=Wrap(skill,direction,unsigned(std::size(cheats::data::LearnedSkills))); }
            else if(row==6 && activate) Write(Action::LearnSkill,0,skill,true,zh);
            else if(row==7 && activate) Write(Action::LearnSupportedSkills,0,0,true,zh);
        } else if(category==2) {
            if(row==1) { inventory=1-inventory;item=0; }
            else if(row==2) { if(activate)Open(Picker::Item,item);else item=Wrap(item,direction,unsigned(Items().size())); }
            else if(row==3) quantity=Wrap(quantity,direction,unsigned(std::size(Quantities)));
            else if(row==4 && activate) Write(inventory ? Action::Material:Action::Item,Quantities[quantity],item,false,zh);
            else if(row==5 && activate) Write(inventory ? Action::AllMaterials:Action::AllItems,Quantities[quantity],0,true,zh);
        } else if(category==3) {
            if(row==1) { if(activate)Open(Picker::Character,character);else character=Wrap(character,direction,9); }
            else if(row==2) { equipmentSlot=Wrap(equipmentSlot,direction,10);equipment=0; }
            else if(row==3) { if(activate)Open(Picker::Equipment,equipment);else equipment=Wrap(equipment,direction,unsigned(EquipmentList().size())); }
            else if(row==4 && activate) Write(equipmentSlot==0 ? Action::SetWeapon : equipmentSlot==1 ? Action::SetRing : Action::SetAccessory,
                EquipmentList()[equipment].value,equipmentSlot<2 ? 0 : equipmentSlot-2,true,zh);
        } else if(category==4) {
            if(row==1)partySlot=Wrap(partySlot,direction,5);
            else if(row==2) { if(activate)Open(Picker::Character,character);else character=Wrap(character,direction,9); }
            else if(row==3 && activate)Write(Action::SetFormation,character,partySlot,true,zh);
            else if(row==4)backRow=1-backRow;
            else if(row==5 && activate)Write(Action::SetRow,backRow ? 0x80:0,0,true,zh);
            else if(row==6 && activate)Write(Action::SetFieldCharacter,character,0,true,zh);
        } else if(category==5 && row==1 && activate) {
            const auto s=cheats::session.Get();
            if(s.editorRequested)cheats::session.SetEditor(false);
            else if(s.enabled && s.available) { confirmation=Confirmation::Editor;confirm=true;yes=false; }
            else notice=zh ? L"请先启用内存修改并进入可控制场景" : L"Enable memory edits and enter a playable field first";
        }
        return true;
    }
};
inline std::wstring Fit(host_ui::Rasterizer& r, std::wstring text, int width) {
    if(r.MeasureWString(text)<=width)return text;
    while(!text.empty() && r.MeasureWString(text+L"...")>width)text.pop_back();
    return text+L"...";
}
inline const wchar_t* ResultText(cheats::Result result,bool zh) {
    switch(result) {
    case cheats::Result::Pending:return zh ? L"已排队，关闭 F1 后执行" : L"Pending - close F1 to apply";
    case cheats::Result::Applied:return zh ? L"修改已执行" : L"Change applied";
    case cheats::Result::Cancelled:return zh ? L"场景发生变化，修改已取消" : L"Scene changed - request cancelled";
    case cheats::Result::Invalid:return zh ? L"数据或目标无效，未写入" : L"Invalid data or target - no writes";
    case cheats::Result::Disabled:return zh ? L"内存修改已关闭" : L"Memory edits disabled";
    case cheats::Result::Ready:return zh ? L"非战斗场景已就绪" : L"Out-of-battle data ready";
    default:return zh ? L"等待读档完成并进入可控制场景" : L"Waiting for a loaded, playable field";
    }
}
inline void Render(host_ui::Rasterizer& r,const Model& m,bool zh,int x,int y,int width) {
    const auto s=cheats::session.Get(); const auto speed=fast_forward::GetStatus();
    const auto color=host_ui::MakeColor;
    const auto text=color(255,226,233,243),muted=color(255,145,158,178),gold=color(255,229,196,126);
    const int side=142,gap=16,rx=x+side+gap,rw=width-side-gap,rowH=35;
    auto drawText=[&](int px,int py,std::wstring t,int maxWidth,uint32_t c) {
        r.DrawWString(px,py,Fit(r,std::move(t),maxWidth),c);
    };
    for(unsigned i=0;i<6;++i) {
        const bool selected=m.category==i;
        r.FillRect(x,y+int(i)*39,side,32,selected ? color(255,47,59,76):color(255,27,32,41));
        if(selected)r.FillRect(x,y+int(i)*39,3,32,gold);
        drawText(x+12,y+int(i)*39+8,Categories[i][zh],side-20,selected ? gold:muted);
    }
    drawText(x,y+248,zh ? L"默认关闭" : L"OFF by default",side,muted);
    drawText(x,y+270,zh ? L"先备份存档" : L"Back up saves",side,gold);
    auto row=[&](unsigned id,const wchar_t* label,std::wstring value,bool enabled=true) {
        const int py=y+int(id)*rowH;
        const bool selected=m.row==id;
        r.FillRect(rx,py,rw,30,selected ? color(255,46,57,72):color(255,29,34,43));
        if(selected)r.FillRect(rx,py,3,30,gold);
        const auto c=enabled ? text:muted;
        if(value.empty())drawText(rx+12,py+7,label,rw-24,c);
        else {
            const int labelWidth=180;
            drawText(rx+12,py+7,label,labelWidth-10,c);
            drawText(rx+labelWidth,py+7,std::move(value),rw-labelWidth-12,selected ? gold:c);
        }
    };
    row(0,zh ? L"类别  < / >" : L"Category  < / >",Categories[m.category][zh]);
    const bool writable=s.enabled && s.available;
    const auto charName=cheats::data::Characters[m.character];
    if(m.category==0) {
        row(1,zh ? L"按住 LT 加速" : L"Hold LT to speed up",speed.enabled ? L"ON":L"OFF");
        row(2,zh ? L"加速倍率" : L"Speed multiplier",std::to_wstring(speed.multiplier)+L"x");
        row(3,zh ? L"允许内存修改" : L"Allow memory edits",s.enabled ? L"ON":L"OFF");
        row(4,zh ? L"增加 100,000 金币" : L"Add 100,000 gold",L"",writable);
        row(5,zh ? L"金币设为 9,999,999" : L"Set gold to 9,999,999",L"",writable);
        row(6,zh ? L"恢复当前队伍 HP / MP" : L"Restore party HP / MP",L"",writable);
    } else if(m.category==1) {
        row(1,zh ? L"角色" : L"Character",charName);
        row(2,zh ? L"恢复 HP / MP" : L"Restore HP / MP",L"",writable);
        row(3,zh ? L"EXP 目标（0-99）" : L"EXP target (0-99)",std::to_wstring(m.exp));
        row(4,zh ? L"应用 EXP" : L"Apply EXP",L"",writable);
        row(5,zh ? L"选择技能" : L"Choose skill",cheats::data::LearnedSkills[m.skill].name);
        row(6,zh ? L"习得所选技能" : L"Learn selected skill",L"",writable);
        row(7,zh ? L"习得已核对技能" : L"Learn known skills",std::to_wstring(std::size(cheats::data::LearnedSkills)),writable);
    } else if(m.category==2) {
        row(1,zh ? L"物品类型" : L"Inventory type",m.inventory ? (zh ? L"素材":L"Materials"):(zh ? L"物品":L"Items"));
        row(2,zh ? L"选择条目" : L"Choose item",m.Items()[m.item].name);
        row(3,zh ? L"目标数量" : L"Target quantity",std::to_wstring(Quantities[m.quantity]));
        row(4,zh ? L"应用所选条目" : L"Apply selected item",L"",writable);
        row(5,zh ? L"填充此类已知条目" : L"Fill listed items",std::to_wstring(m.Items().size()),writable);
    } else if(m.category==3) {
        row(1,zh ? L"角色" : L"Character",charName);
        std::wstring slot=m.equipmentSlot==0 ? (zh ? L"武器":L"Weapon") : m.equipmentSlot==1 ? (zh ? L"指环":L"Ring")
            : std::wstring(zh ? L"饰品 ":L"Accessory ")+std::to_wstring(m.equipmentSlot-1);
        row(2,zh ? L"装备槽" : L"Equipment slot",slot);
        row(3,zh ? L"选择装备" : L"Choose equipment",m.EquipmentList()[m.equipment].name);
        row(4,zh ? L"应用装备（实验性）" : L"Apply equipment (experimental)",L"",writable);
    } else if(m.category==4) {
        row(1,zh ? L"队伍位置" : L"Party position",std::to_wstring(m.partySlot+1));
        row(2,zh ? L"角色" : L"Character",charName);
        row(3,zh ? L"交换 / 加入该位置" : L"Swap / assign position",L"",writable);
        row(4,zh ? L"前后排" : L"Battle row",m.backRow ? (zh ? L"后排":L"Back"):(zh ? L"前排":L"Front"));
        row(5,zh ? L"应用角色前后排" : L"Apply character row",L"",writable);
        row(6,zh ? L"设为场景角色（实验性）" : L"Set field character (experimental)",L"",writable);
    } else {
        row(1,zh ? L"原版场景编辑器" : L"Retail field editor",s.editorRequested ? (s.editorApplied ? L"ON":L"PENDING") : (s.editorApplied ? L"STOPPING":L"OFF"),writable);
        drawText(rx+12,y+94,zh ? L"实验性：LT + RT 尝试打开原版 EDIT MENU" : L"Experimental: try LT + RT for retail EDIT MENU",rw-24,gold);
        drawText(rx+12,y+123,zh ? L"不是 Prototype Debug Menu" : L"Not the prototype debug menu",rw-24,muted);
        drawText(rx+12,y+152,zh ? L"先退出原版编辑器，再关闭此开关" : L"Exit the retail editor before disabling this switch",rw-24,muted);
    }
    const wchar_t* hints[][2]={
        {L"Release LT to return to 1x. LT+RT does not boost. Audio is not time-stretched.",L"松开 LT 恢复原速；LT+RT 不加速。音频不做时间拉伸。"},
        {L"HP/MP are out-of-battle values. EXP is progress, not a level selector.",L"HP/MP 为非战斗数值；EXP 是经验进度，不是等级。"},
        {L"Choose item: A opens a paged list. Sort the inventory once to refresh.",L"按 A 打开可翻页列表；修改后在游戏内整理物品以刷新。"},
        {L"Only existing accessory slots. Check character compatibility before applying.",L"仅能修改已有饰品槽；应用前请确认角色适用的装备。"},
        {L"Assigning a present member swaps slots. Field character may require reloading.",L"重复成员会交换位置；场景角色修改可能需要重新读档。"},
        {L"Guest flag only. No executable patches. Leave this OFF during normal play.",L"只修改 guest 标记，不改宿主机器码。正常游玩建议关闭。"}};
    // Warnings wrap instead of disappearing behind an ellipsis.
    auto paragraph=[&](int px,int py,std::wstring value,int maxWidth,uint32_t c) {
        for(unsigned line=0;!value.empty() && line<2;++line) {
            size_t n=value.size();
            while(n>1 && r.MeasureWString(value.substr(0,n))>maxWidth)--n;
            if(n<value.size()) { const auto space=value.rfind(L' ',n); if(space!=std::wstring::npos && space>n/2)n=space; }
            drawText(px,py+int(line)*19,value.substr(0,n),maxWidth,c);
            value.erase(0,n);while(!value.empty() && value.front()==L' ')value.erase(0,1);
        }
    };
    paragraph(x,y+296,hints[m.category][zh],width,muted);
    std::wstring status=ResultText(s.result,zh);
    if(s.available) {
        if(m.category==1 || m.category==3) {
            const auto& c=s.characters[m.character];
            status+=L" | HP "+std::to_wstring(unsigned(c.hp))+L"/"+std::to_wstring(unsigned(c.maxHp))+
                L" MP "+std::to_wstring(unsigned(c.mp))+L"/"+std::to_wstring(unsigned(c.maxMp));
        } else status+=L" | Gold: "+std::to_wstring(s.gold);
    }
    drawText(x,y+340,status,width,s.result==cheats::Result::Invalid ? gold:text);
    if(!m.notice.empty())drawText(x,y+366,m.notice,width,gold);
    if(m.picker!=Picker::None) {
        r.FillRect(x,y,width,365,color(252,24,31,42));
        drawText(x+16,y+12,zh ? L"选择条目   左 / 右：翻页" : L"Select an entry   Left / Right: page",width-32,gold);
        const unsigned count=m.PickCount(),first=(m.picked/6)*6;
        for(unsigned i=first;i<std::min(count,first+6);++i) {
            const int py=y+49+int(i-first)*41;
            const bool selected=i==m.picked;
            r.FillRect(x+12,py,width-24,34,selected ? color(255,50,64,82):color(255,30,38,49));
            drawText(x+24,py+9,m.PickName(i),width-48,selected ? gold:text);
        }
        drawText(x+18,y+309,std::to_wstring(m.picked+1)+L" / "+std::to_wstring(count),width-36,muted);
        drawText(x+18,y+337,zh ? L"A / Enter：选择   B / Esc：返回（不修改）" : L"A / Enter: select   B / Esc: back (no writes)",width-36,muted);
    }
    if(m.confirm) {
        r.FillRect(x,y,width,365,color(250,24,31,42));
        drawText(x+24,y+39,zh ? L"确认这项修改？" : L"Confirm this change?",width-48,gold);
        paragraph(x+24,y+93,zh ? L"修改可能进入存档，关闭开关不会撤销已写入数值。" : L"Changes can enter your save; switching OFF does not undo edits.",width-48,text);
        paragraph(x+24,y+140,zh ? L"请先备份存档。批量修改只处理已列出的条目。" : L"Back up your save. Bulk edits touch only catalogued entries.",width-48,text);
        paragraph(x+24,y+182,zh ? L"关闭 F1 后由游戏线程执行；场景变化时取消。" : L"Applied on the game thread after closing F1; cancelled on scene changes.",width-48,muted);
        const int bw=(width-64)/2,py=y+231;
        for(int i=0;i<2;++i) {
            const bool selected=m.yes==(i==1);
            const int px=x+24+i*(bw+16);
            r.FillRect(px,py,bw,42,selected ? color(255,59,70,86):color(255,31,38,49));
            drawText(px+14,py+13,i ? (zh ? L"确认":L"Confirm"):(zh ? L"取消":L"Cancel"),bw-28,selected ? gold:text);
        }
        drawText(x+24,y+311,zh ? L"左 / 右：选择   A：确定   B：取消" : L"Left / Right: choose   A: accept   B: cancel",width-48,muted);
    }
}
} // namespace debug_menu::cheat_overlay

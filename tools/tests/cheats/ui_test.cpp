#include <debug/cheat_overlay.h>
#include <cassert>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <random>
using namespace debug_menu;
using namespace debug_menu::cheat_overlay;
unsigned checks=0;
void Check(bool condition) { ++checks; if(!condition) { std::fprintf(stderr,"FAIL UI check %u\n",checks);std::abort(); } }
void Set(cheats::Memory m,uint32_t p,uint32_t v) { const cheats::Write w{p,v,4};cheats::Commit(m,{&w,1}); }
void Number(cheats::Memory m,uint32_t p,float v) { Set(m,p,std::bit_cast<uint32_t>(v)); }
void Ppm(const std::filesystem::path& path,const host_ui::PixelBuffer& buffer) {
    std::ofstream f(path,std::ios::binary);f<<"P6\n"<<buffer.width<<' '<<buffer.height<<"\n255\n";
    for(uint32_t p:buffer.pixels) { const char rgb[]={char(p),char(p>>8),char(p>>16)};f.write(rgb,3); }
    Check(bool(f));
}
int main(int argc,char** argv) {
    using namespace cheats;
    std::vector<uint8_t> bytes(data::DataSize);Memory memory{bytes};
    for(unsigned n=0;n<5;++n)Set(memory,Party+n*4,n);
    for(unsigned n=0;n<9;++n) { auto b=n*data::CharacterStride;Number(memory,MaxHp+b,500+n);Number(memory,MaxMp+b,80+n); }
    const Context context{0x100008,0x200000,0x300000};
    session.Enable(false);session.Tick(context,memory,true);
    Check(session.Get().available && !session.Get().enabled);
    Model m;
    m.row=3;m.Input(Nav::Confirm,false);Check(m.confirm && !m.yes);
    m.Input(Nav::Confirm,false);Check(!session.Get().enabled); // default Cancel
    m.Input(Nav::Confirm,false);m.Input(Nav::Right,false);m.Input(Nav::Cancel,false);Check(!session.Get().enabled);
    m.Input(Nav::Confirm,false);m.Input(Nav::Right,false);m.Input(Nav::Confirm,false);Check(session.Get().enabled);
    auto baseline=bytes;
    m.row=5;m.Input(Nav::Confirm,false);Check(m.confirm && !session.HasPending());
    m.Input(Nav::Right,false);m.Input(Nav::Confirm,false);Check(session.HasPending() && bytes==baseline);
    session.Tick(context,memory,true);Check(memory.U32(Gold)==GoldLimit && session.Get().result==Result::Applied);
    m.category=1;m.row=1;m.Input(Nav::Confirm,false);Check(m.picker==Picker::Character);
    m.Input(Nav::Right,false);Check(m.picked==6);
    m.Input(Nav::Right,false);Check(m.picked==8);
    m.Input(Nav::Confirm,false);Check(m.character==8 && m.picker==Picker::None);
    m.row=5;m.Input(Nav::Confirm,false);Check(m.picker==Picker::Skill);
    Check(m.Input(Nav::Cancel,false) && m.picker==Picker::None);Check(!m.Input(Nav::Cancel,false));
    m.category=3;m.row=2;m.equipmentSlot=0;m.equipment=62;
    m.Input(Nav::Right,false);Check(m.equipmentSlot==1 && m.equipment==0);
    m.row=3;m.Input(Nav::Confirm,false);Check(m.PickCount()==236);
    for(unsigned n=0;n<45;++n)m.Input(Nav::Right,false);
    Check(m.picked==235);m.Input(Nav::Confirm,false);Check(m.equipment==235);
    m.row=0;m.Input(Nav::Right,false);Check(m.category==4 && m.row==0);
    m.category=0;m.row=1;fast_forward::Enable(false);m.Input(Nav::Confirm,false);Check(fast_forward::GetStatus().enabled);
    m.row=2;for(unsigned n=0;n<5;++n)m.Input(Nav::Right,false);Check(fast_forward::GetStatus().multiplier==2);
    fast_forward::Enable(false);
    std::mt19937 random(25);
    for(unsigned n=0;n<5000;++n) {
        m.Input(static_cast<Nav>(random()%6),bool(n%2));
        Check(m.category<6 && m.row<m.Rows() && m.character<9 && m.item<m.Items().size() && m.equipment<m.EquipmentList().size());
        if(m.picker!=Picker::None)Check(m.picked<m.PickCount());
        session.Tick(context,memory,true);
    }
    host_ui::PixelBuffer pixels;Check(pixels.Resize());host_ui::Rasterizer raster(pixels);
    // Paint all categories and both nested flows through the actual renderer.
    const std::filesystem::path output=argc>1 ? argv[1] : ".";
    std::filesystem::create_directories(output);
    for(bool zh:{false,true})for(unsigned category=0;category<6;++category) {
        Model view;view.category=category;view.row=1;
        pixels.Clear(host_ui::MakeColor(255,17,22,30));
        raster.FillRect(260,90,760,540,host_ui::MakeColor(255,24,29,38));
        raster.DrawWString(280,103,zh ? L"F1 / Cheats - 测试界面" : L"F1 / Cheats - synthetic UI fixture",host_ui::MakeColor(255,230,235,242));
        Render(raster,view,zh,290,176,700);
        Ppm(output/(std::string(zh ? "zh-":"en-")+std::to_string(category)+".ppm"),pixels);
    }
    for(bool confirmation:{false,true}) {
        Model view;view.category=3;view.row=3;view.equipmentSlot=1;
        if(confirmation) { view.confirm=true;view.yes=false; }
        else { view.Open(Picker::Equipment,235); }
        pixels.Clear(host_ui::MakeColor(255,17,22,30));Render(raster,view,false,290,176,700);
        Ppm(output/(confirmation ? "confirm.ppm":"picker.ppm"),pixels);
    }
    std::printf("PASS: %u UI checks; default-cancel confirmation, nested back, paging, 5000 navigation steps, 14 renders\n",checks);
}

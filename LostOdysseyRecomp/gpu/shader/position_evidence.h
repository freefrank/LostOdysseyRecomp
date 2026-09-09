#pragma once

#include <algorithm>
#include <array>
#include <bitset>
#include <charconv>
#include <compare>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace gpu::position_evidence {
// Diagnostic evidence only. Never authorizes shader modification or jitter.
struct Summary {
    uint32_t version = 1;
    uint32_t kind = 0; // 0 unproven, 1 four-row linear matrix, 2 direct vertex position
    int32_t slot = -1;
    uint32_t issues = 0; // relative=1, flow=2, unsupported=4, ambiguous=8, self-dependent=16, limit=32
    uint32_t outputs = 0; // Interpolators that may also depend on the position matrix.
    auto operator<=>(const Summary&) const = default;
};

namespace detail {
inline std::string_view Trim(std::string_view s) {
    while (!s.empty() && (s.front()==' ' || s.front()=='\t' || s.front()=='\r' || s.front()=='\n')) s.remove_prefix(1);
    while (!s.empty() && (s.back()==' ' || s.back()=='\t' || s.back()=='\r' || s.back()=='\n')) s.remove_suffix(1);
    return s;
}
inline bool Integer(std::string_view s, unsigned& n) {
    const auto r=std::from_chars(s.data(),s.data()+s.size(),n);
    return r.ec==std::errc{} && r.ptr==s.data()+s.size();
}
struct Value { std::array<uint32_t,4> id{}; unsigned size=4; };
struct Node {
    enum Type { Opaque, Input, Constant, Add, Mul } type=Opaque;
    uint32_t a=0,b=0;
    unsigned slot=0,component=0,group=0;
    std::bitset<256> constants;
    bool unknown=false;
};

// This parser accepts only the controlled translator's straight-line HLSL body.
// It follows register versions and component swizzles, rather than searching for
// constant names. Unrecognized expressions cannot provide position proof.
class Analyzer {
    std::vector<Node> nodes;
    std::map<std::string,Value,std::less<>> values;
    uint32_t limits=0,flow=0,relative=0,nextGroup=1;
    unsigned positionWrites=0;

    uint32_t Put(Node n) {
        if(nodes.size()>=32768) {limits=32;return 0;}
        nodes.push_back(std::move(n));return uint32_t(nodes.size()-1);
    }
    Value Unknown() {return {{0,0,0,0},4};}
    Value Literal() {Node n;const auto id=Put(n);return {{id,id,id,id},1};}
    Value Opaque(const std::vector<Value>& args,unsigned size) {
        Node n;
        for(const auto& a:args) for(unsigned i=0;i<a.size;++i) {
            n.constants|=nodes[a.id[i]].constants;n.unknown|=nodes[a.id[i]].unknown;
        }
        Value out;out.size=size;
        for(unsigned i=0;i<size;++i) out.id[i]=Put(n);
        return out;
    }
    Value Binary(Node::Type type,Value a,Value b) {
        Value out;out.size=std::max(a.size,b.size);
        if(a.size!=b.size && a.size!=1 && b.size!=1)return Unknown();
        for(unsigned i=0;i<out.size;++i) {
            Node n;n.type=type;n.a=a.id[a.size==1?0:i];n.b=b.id[b.size==1?0:i];
            n.constants=nodes[n.a].constants|nodes[n.b].constants;
            n.unknown=nodes[n.a].unknown||nodes[n.b].unknown;out.id[i]=Put(n);
        }
        return out;
    }
    static std::vector<std::string_view> Arguments(std::string_view text) {
        std::vector<std::string_view> result;int nesting=0;size_t start=0;
        for(size_t i=0;i<text.size();++i) {
            if(text[i]=='(')++nesting;
            if(text[i]==')')--nesting;
            if(text[i]==',' && !nesting) {result.push_back(Trim(text.substr(start,i-start)));start=i+1;}
        }
        result.push_back(Trim(text.substr(start)));return result;
    }
    Value Parse(std::string_view s,unsigned depth=0) {
        s=Trim(s);
        if(depth>40 || s.empty()) {limits|=depth>40?32:0;return Unknown();}
        // Split only top-level operators; respect the translator's precedence.
        for(char op:{'+','*'}) {
            int nesting=0;
            for(size_t i=s.size();i-->0;) {
                if(s[i]==')')++nesting;else if(s[i]=='(')--nesting;
                if(s[i]==op && !nesting && i && s[i-1]!='e' && s[i-1]!='E')
                    return Binary(op=='+'?Node::Add:Node::Mul,Parse(s.substr(0,i),depth+1),Parse(s.substr(i+1),depth+1));
            }
        }
        if(s.front()=='(' && s.back()==')')return Parse(s.substr(1,s.size()-2),depth+1);
        if(s.front()=='-')return Opaque({Parse(s.substr(1),depth+1)},4);
        const auto dot=s.rfind('.');
        if(dot!=s.npos && dot+1<s.size()) {
            const auto sw=s.substr(dot+1);
            if(sw.size()<=4 && sw.find_first_not_of("xyzw")==sw.npos) {
                auto v=Parse(s.substr(0,dot),depth+1);Value out;out.size=unsigned(sw.size());
                for(unsigned i=0;i<out.size;++i) {
                    const auto c=std::string_view("xyzw").find(sw[i]);
                    if(c>=v.size)return Unknown();out.id[i]=v.id[c];
                }
                return out;
            }
        }
        if(s.starts_with("XeConst(") && s.back()==')') {
            unsigned slot=0;
            if(!Integer(Trim(s.substr(8,s.size()-9)),slot) || slot>=256) {relative=1;return Unknown();}
            Value v;
            for(unsigned i=0;i<4;++i) {Node n;n.type=Node::Constant;n.slot=slot;n.component=i;n.constants.set(slot);v.id[i]=Put(n);}
            return v;
        }
        if(auto it=values.find(s);it!=values.end())return it->second;
        if(s=="xeVertexId") {Node n;n.type=Node::Input;n.group=0;auto id=Put(n);return {{id,id,id,id},1};}
        std::string number(s);char* end=nullptr;std::strtod(number.c_str(),&end);
        if(end!=number.c_str() && (*end==0 || ((*end=='u'||*end=='f') && end[1]==0)))return Literal();
        const auto open=s.find('(');
        if(open!=s.npos && s.back()==')') {
            const auto name=Trim(s.substr(0,open));
            if(name.starts_with("XeVF_")) {
                // Fetch addressing is part of the dependency, even though the
                // fetched values themselves are opaque to static analysis.
                auto it=values.find("xeVfetchBase");Node n;
                if(it==values.end())n.unknown=true;
                else for(unsigned i=0;i<it->second.size;++i) {
                    n.constants|=nodes[it->second.id[i]].constants;n.unknown|=nodes[it->second.id[i]].unknown;
                }
                Value v;const auto group=nextGroup++;
                for(unsigned i=0;i<4;++i) {n.type=Node::Input;n.group=group;n.component=i;v.id[i]=Put(n);}return v;
            }
            std::vector<Value> args;
            for(const auto arg:Arguments(s.substr(open+1,s.size()-open-2)))args.push_back(Parse(arg,depth+1));
            if(name=="max" && args.size()==2 && args[0].size==args[1].size && args[0].id==args[1].id)return args[0];
            if(name=="float4") {
                Value v;
                if(args.size()==1 && args[0].size==1) {v.id.fill(args[0].id[0]);return v;}
                if(args.size()==4) {for(unsigned i=0;i<4;++i){if(args[i].size!=1)return Unknown();v.id[i]=args[i].id[0];}return v;}
                return Unknown();
            }
            if(name=="dot" || name=="float" || name=="int" || name=="uint")return Opaque(args,1);
            if(name=="max" || name=="min" || name=="abs" || name=="saturate" || name=="floor" ||
               name=="ceil" || name=="frac" || name=="rsqrt" || name=="rcp" || name=="sqrt" || name=="exp2" || name=="log2")
                return Opaque(args,args.empty()?4:args[0].size);
        }
        return Unknown();
    }
    void Assign(std::string_view left,Value v) {
        left=Trim(left);auto dot=left.find('.');const auto name=left.substr(0,dot);
        if(name.empty())return;
        if(name.find_first_not_of("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_")!=name.npos) {limits|=4;return;}
        if(name=="oPos") {
            ++positionWrites;
            if(dot!=left.npos && left.substr(dot+1)!="xyzw")positionWrites+=100;
        }
        if(dot==left.npos) {
            if(v.size==1 && name!="ps" && name!="a0" && name!="aL" && name!="p0" && name!="xeVfetchBase") {
                v.id.fill(v.id[0]);v.size=4;
            }
            values[std::string(name)]=v;return;
        }
        const auto mask=left.substr(dot+1);
        if(mask.size()>4 || mask.empty() || mask.find_first_not_of("xyzw")!=mask.npos) {limits|=4;return;}
        if(v.size!=1 && v.size!=mask.size())v=Unknown();
        auto [it,inserted]=values.try_emplace(std::string(name),Unknown());
        it->second.size=4;
        for(unsigned i=0;i<mask.size();++i)it->second.id[std::string_view("xyzw").find(mask[i])]=v.id[v.size==1?0:i];
    }
    bool Terms(uint32_t id,std::vector<uint32_t>& terms,unsigned depth=0) {
        if(depth>8 || terms.size()>4)return false;
        const auto n=nodes[id];
        if(n.type==Node::Add)return Terms(n.a,terms,depth+1)&&Terms(n.b,terms,depth+1);
        terms.push_back(id);return true;
    }
public:
    Analyzer() {Node unknown;unknown.unknown=true;unknown.constants.set();nodes.push_back(unknown);}
    Summary Run(std::string_view hlsl) {
        Summary result;
        if(hlsl.size()>2*1024*1024) {result.issues=32;return result;}
        const auto main=hlsl.find("void main(");
        constexpr std::string_view startMarker="float4 oPointSize = 0.0;";
        const auto start=main==hlsl.npos?hlsl.npos:hlsl.find(startMarker,main);
        const auto end=start==hlsl.npos?hlsl.npos:hlsl.find("if ((xeFlags & 8u) == 0u)",start);
        if(start==hlsl.npos || end==hlsl.npos) {result.issues=4;return result;}
        // Initialize translator temporaries (before the body marker).
        const auto zero=Literal();Value zeros;zeros.id.fill(zero.id[0]);
        for(unsigned i=0;i<256;++i)values["r"+std::to_string(i)]=zeros;
        for(unsigned i=0;i<16;++i)values["o"+std::to_string(i)]=zeros;
        values["xePV"]=zeros;values["xeDiscard"]=zeros;values["xeVfetchBase"]=zero;
        for(const auto name:{"ps","a0","aL","p0"})values[name]=zero;
        auto vertex=Parse("xeVertexId");values["r0"].id[0]=vertex.id[0];
        auto body=hlsl.substr(start+startMarker.size(),end-start-startMarker.size());
        unsigned lines=0;
        while(!body.empty()) {
            auto n=body.find('\n');auto line=Trim(body.substr(0,n));
            body=n==body.npos?std::string_view{}:body.substr(n+1);
            if(++lines>8192) {limits=32;break;}
            if(auto c=line.find("//");c!=line.npos)line=Trim(line.substr(0,c));
            if(line.empty())continue;
            if(line.find('{')!=line.npos || line.find('}')!=line.npos || line.starts_with("if") ||
               line.starts_with("for") || line.starts_with("while") || line.starts_with("switch") ||
               line.starts_with("case") || line.starts_with("break") || line.starts_with("return") || line.starts_with("do ")) {flow=2;continue;}
            const auto equal=line.find('=');
            if(equal==line.npos || line.back()!=';')continue;
            Assign(line.substr(0,equal),Parse(line.substr(equal+1,line.size()-equal-2)));
        }
        result.issues=limits|flow|relative;
        if(result.issues)return result;
        const auto found=values.find("oPos");
        if(found==values.end() || positionWrites!=1 || found->second.size!=4) {result.issues=8;return result;}
        const Value position=found->second;
        for(auto id:position.id)if(nodes[id].unknown) {result.issues=4;return result;}
        bool direct=true;const auto group=nodes[position.id[0]].group;
        for(unsigned i=0;i<4;++i) {
            const auto n=nodes[position.id[i]];
            direct&=n.type==Node::Input && n.group==group && n.component==i;
        }
        if(direct) {result.kind=2;return result;}
        std::array<uint32_t,4> inputs{};int first=-1;
        for(unsigned column=0;column<4;++column) {
            std::vector<uint32_t> terms;
            if(!Terms(position.id[column],terms) || terms.size()!=4) {result.issues=8;return result;}
            std::map<unsigned,uint32_t> rows;
            for(const auto t:terms) {
                auto n=nodes[t];if(n.type!=Node::Mul) {result.issues=8;return result;}
                if(nodes[n.b].type==Node::Constant)std::swap(n.a,n.b);
                const auto c=nodes[n.a];
                if(c.type!=Node::Constant || c.component!=column || !rows.emplace(c.slot,n.b).second) {result.issues=8;return result;}
            }
            if(first<0)first=int(rows.begin()->first);
            if(first>252) {result.issues=8;return result;}
            for(unsigned row=0;row<4;++row) {
                auto it=rows.find(unsigned(first)+row);
                if(it==rows.end() || (column && inputs[row]!=it->second)) {result.issues=8;return result;}
                inputs[row]=it->second;
            }
        }
        for(auto input:inputs)for(int row=first;row<first+4;++row)
            if(nodes[input].constants.test(row)) {result.issues=16;return result;}
        result.kind=1;result.slot=first;
        for(unsigned output=0;output<16;++output)if(auto it=values.find("o"+std::to_string(output));it!=values.end())
            for(auto id:it->second.id)for(int row=first;row<first+4;++row)
                if(nodes[id].constants.test(row))result.outputs|=1u<<output;
        return result;
    }
};
} // namespace detail

inline Summary Analyze(std::string_view hlsl) {return detail::Analyzer().Run(hlsl);}
} // namespace gpu::position_evidence

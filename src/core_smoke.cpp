#include "sim/World.hpp"
#include <algorithm>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <string>

static std::string signature(const sim::World& w){std::ostringstream o;for(const auto&e:w.events())o<<e.day<<':'<<e.minute<<':'<<e.personId<<':'<<e.otherPersonId<<':'<<e.placeId<<':'<<e.type<<'|'<<e.text<<'\n';return o.str();}

int main(){
    constexpr unsigned seed=48192; constexpr int population=8;
    sim::World a(seed,population), b(seed,population);
    std::set<std::string> names; std::set<sim::Activity> activities; std::set<std::pair<int,int>> pairs;
    std::map<std::string,int> counts;
    bool valid=true; int maxActive=0;
    for(int elapsed=0;elapsed<1440;elapsed+=5){
        a.step(5); b.step(5);
        int active=0;
        for(const auto&p:a.citizens()){
            names.insert(p.name); activities.insert(p.activity); active+=p.activeInteractionId>=0;
            valid=valid&&p.hunger>=0&&p.hunger<=100&&p.energy>=0&&p.energy<=100&&p.stress>=0&&p.stress<=100&&p.loneliness>=0&&p.loneliness<=100;
            valid=valid&&!(p.activity==sim::Activity::Commuting&&p.destinationPlaceId<0);
        }
        maxActive=std::max(maxActive,active);
    }
    for(const auto&e:a.events()){++counts[e.type];if((e.type=="interaction_start"||e.type=="interaction_end")&&e.otherPersonId>=0)pairs.insert(std::minmax(e.personId,e.otherPersonId));}
    int memories=0, links=0; for(const auto&p:a.citizens()){memories+=(int)p.memories.size();links+=(int)p.relationships.size();}
    bool deterministic=signature(a)==signature(b);
    int trips=counts["departure"], arrivals=counts["arrival"], starts=counts["interaction_start"], ends=counts["interaction_end"], intents=counts["intent"];
    bool routinePass=names.size()==population&&activities.size()>=4&&trips>=population&&arrivals>=population&&intents>=population&&valid;
    bool socialPass=starts>0&&ends>0&&ends<=starts&&pairs.size()>0&&memories>=ends*2&&maxActive<=population;
    bool pass=routinePass&&socialPass&&deterministic;
    std::cout<<"MASON BLOCK V3 CORE SMOKE\n";
    std::cout<<"seed="<<seed<<" population="<<population<<" simulated_hours=24\n\n";
    std::cout<<"ROUTINE REALISM\n"<<"unique_names="<<names.size()<<"/"<<population<<"\nactivity_states_seen="<<activities.size()<<"\nintent_changes="<<intents<<"\ntrips="<<trips<<" arrivals="<<arrivals<<"\nstate_invariants="<<(valid?"PASS":"FAIL")<<"\nroutine_result="<<(routinePass?"PASS":"FAIL")<<"\n\n";
    std::cout<<"SOCIAL QUALITY\ninteraction_starts="<<starts<<" completed="<<ends<<"\nunique_pairs="<<pairs.size()<<" memories="<<memories<<" relationship_links="<<links/2<<"\nmax_residents_interacting="<<maxActive<<"\nsocial_result="<<(socialPass?"PASS":"FAIL")<<"\n\n";
    std::cout<<"DETERMINISM\nsame_seed_event_stream="<<(deterministic?"PASS":"FAIL")<<"\n\nOVERALL="<<(pass?"PASS":"FAIL")<<"\n";
    return pass?0:1;
}

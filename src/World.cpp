#include "sim/World.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <iomanip>
#include <set>
#include <sstream>

namespace sim {
namespace {
float clamp100(float v) { return std::clamp(v, 0.0f, 100.0f); }
int absoluteMinute(int day, int minute) { return (day - 1) * 1440 + minute; }
}

World::World(std::uint32_t seed, int population)
    : seed_(seed), rng_(seed), population_(std::max(1, population)) {
    buildNeighborhood(); spawnCitizens(); seedHouseholdRelationships();
    emit(-1, "world", "Neighborhood simulation started", 0.2f);
}

void World::buildNeighborhood() {
    int nextId = 0;
    static constexpr std::array<Vec2,12> homes = {Vec2{52,52},Vec2{122,52},Vec2{192,52},Vec2{262,52},Vec2{52,126},Vec2{122,126},Vec2{192,126},Vec2{262,126},Vec2{52,286},Vec2{122,286},Vec2{192,286},Vec2{262,286}};
    for (int i=0;i<(int)homes.size();++i) { places_.push_back(Place{nextId,"Apartment "+std::to_string(i+1),PlaceType::Home,homes[(size_t)i],6}); homeIds_.push_back(nextId++); }
    static constexpr std::array<Vec2,5> jobs = {Vec2{410,55},Vec2{500,55},Vec2{590,55},Vec2{410,132},Vec2{500,132}};
    const std::array<std::string,5> names={"Mason Garage","Corner Market","Riverside Diner","Briar Office","North Warehouse"};
    for(int i=0;i<5;++i){places_.push_back(Place{nextId,names[(size_t)i],PlaceType::Workplace,jobs[(size_t)i],20});workplaceIds_.push_back(nextId++);}
    cafeId_=nextId; places_.push_back(Place{nextId++,"Blue Cup Cafe",PlaceType::Cafe,{410,286},24});
    shopId_=nextId; places_.push_back(Place{nextId++,"Mason Convenience",PlaceType::Shop,{500,286},18});
    parkId_=nextId; places_.push_back(Place{nextId++,"Willow Park",PlaceType::Park,{590,300},40});
}

void World::spawnCitizens() {
    static const std::array<std::string,24> first={"Marcus","Maya","Daniel","Nora","Eli","Sarah","Jonah","Lena","Andre","Tara","Miles","Rosa","Noah","Iris","Caleb","Jade","Victor","Amara","Theo","Naomi","Isaac","Mina","Owen","Leah"};
    static const std::array<std::string,24> last={"Reed","Cole","Parker","Brooks","Hayes","Kim","Ortiz","Stone","Bennett","Price","Ward","Foster","Diaz","Grant","Turner","Ross","Morgan","Bell","Morris","Bailey","Gray","Cooper","Rivera","James"};
    std::set<std::string> used;
    citizens_.reserve((size_t)population_);
    for(int i=0;i<population_;++i){
        Person p; p.id=i;
        do { p.name=first[(size_t)randomInt(0,23)]+" "+last[(size_t)randomInt(0,23)]; } while(!used.insert(p.name).second);
        p.age=randomInt(19,61); p.homeId=homeIds_[(size_t)randomInt(0,(int)homeIds_.size()-1)]; p.workplaceId=workplaceIds_[(size_t)randomInt(0,(int)workplaceIds_.size()-1)]; p.currentPlaceId=p.homeId;
        p.traits=Traits{random01(),random01(),random01(),random01(),random01()};
        p.behavior=BehaviorSignature{0.85f+random01()*0.35f,random01(),std::clamp((p.traits.sociability+random01())*.5f,0.f,1.f),std::clamp((p.traits.impulsiveness+random01())*.5f,0.f,1.f),random01()};
        p.cash=80+random01()*270; p.hunger=5+random01()*22; p.energy=70+random01()*28; p.stress=5+random01()*28; p.loneliness=10+random01()*35; p.jobSatisfaction=35+random01()*55;
        p.shiftStart=450+randomInt(0,3)*30; p.shiftEnd=p.shiftStart+480; p.dailyWage=65+random01()*55; p.housingCostPerDay=10+random01()*12;
        citizens_.push_back(std::move(p));
    }
}

void World::seedHouseholdRelationships(){for(size_t i=0;i<citizens_.size();++i)for(size_t j=i+1;j<citizens_.size();++j)if(citizens_[i].homeId==citizens_[j].homeId){auto&a=ensureRelationship(citizens_[i],citizens_[j].id);auto&b=ensureRelationship(citizens_[j],citizens_[i].id);float f=45+random01()*25,aff=25+random01()*45;a.familiarity=b.familiarity=f;a.affinity=b.affinity=aff;a.trust=b.trust=20+random01()*35;}}

void World::step(int minutes){
    int dt=std::max(1,minutes); if(minute_==0)for(auto&c:citizens_)c.paidToday=false;
    for(auto&c:citizens_)updateCitizen(c,dt);
    updatePerception(); advanceInteractions(); handleInteractions();
    minute_+=dt; while(minute_>=1440){minute_-=1440;++day_;}
}
void World::runDays(int days,int dt){int total=std::max(0,days)*1440;dt=std::max(1,dt);for(int e=0;e<total;e+=dt)step(std::min(dt,total-e));}

void World::applyNeeds(Person&p,int dt){float h=dt/60.f;if(p.activity==Activity::Sleeping){p.energy+=16*h;p.hunger+=2*h;p.stress-=6*h;}else{p.energy-=(p.activity==Activity::Working?6:p.activity==Activity::Commuting?5:4)*h;p.hunger+=6.5f*h;if(p.activity==Activity::Working){p.stress+=(2+(100-p.jobSatisfaction)/100*5)*h;p.loneliness+=.6f*h;}else if(p.activity==Activity::Relaxing||p.activity==Activity::Socializing){p.stress-=4*h;}else p.stress-=.6f*h;}if(p.hunger>80)p.stress+=2*h;if(p.energy<20)p.stress+=2*h;p.energy=clamp100(p.energy);p.hunger=clamp100(p.hunger);p.stress=clamp100(p.stress);p.loneliness=clamp100(p.loneliness);}

void World::updateCitizen(Person&p,int dt){applyNeeds(p,dt);handleEconomy(p);if(p.activity==Activity::Commuting){updateTravel(p,dt);return;}if(p.activeInteractionId>=0)return;updateIntent(p);applyIntent(p);}

bool World::shouldInterruptIntent(const Person&p)const{if(p.intent.kind==IntentKind::None)return true;int now=absoluteMinute(day_,minute_);if(now>=p.intent.commitUntilMinute)return true;if(p.energy<12&&p.intent.kind!=IntentKind::Sleep)return true;if(p.hunger>88&&p.intent.kind!=IntentKind::Eat)return true;int travel=estimateTravelMinutes(p.currentPlaceId,p.workplaceId);if(minute_>=p.shiftStart-travel-10&&minute_<p.shiftEnd&&p.intent.kind!=IntentKind::Work)return true;return false;}

int World::chooseSocialTarget(const Person&p)const{int best=-1;float score=-1;for(int id:p.visiblePeople){auto*o=personById(id);if(!o||o->activeInteractionId>=0)continue;auto*r=findRelationship(p,id);float s=(r?r->affinity*.5f+r->familiarity*.25f:8.f)+p.loneliness*.25f;if(s>score){score=s;best=id;}}return best;}

void World::updateIntent(Person&p){
    if(!shouldInterruptIntent(p))return;
    Intent next; next.createdDay=day_;next.createdMinute=minute_;int now=absoluteMinute(day_,minute_);
    auto set=[&](IntentKind k,int place,int person,float utility,int duration){if(utility>next.utility){next.kind=k;next.targetPlaceId=place;next.targetPersonId=person;next.utility=utility;next.commitUntilMinute=now+duration;}};
    if(minute_<360||minute_>=1380)set(IntentKind::Sleep,p.homeId,-1,95+(100-p.energy),60);
    int commute=estimateTravelMinutes(p.currentPlaceId,p.workplaceId); if(minute_>=p.shiftStart-commute-15&&minute_<p.shiftEnd)set(IntentKind::Work,p.workplaceId,-1,110,45);
    set(IntentKind::Eat,p.cash>=8?cafeId_:p.homeId,-1,p.hunger,35);
    set(IntentKind::ReturnHome,p.homeId,-1,(100-p.energy)*.55f+(minute_>1200?35:0),50);
    set(IntentKind::Relax,parkId_,-1,p.stress*.72f,45);
    if(p.cash>=12)set(IntentKind::Shop,shopId_,-1,p.hunger*.35f+18,30);
    int target=chooseSocialTarget(p); if(target>=0)set(IntentKind::Socialize,p.currentPlaceId,target,p.loneliness*.65f+p.behavior.talkativeness*30,25);
    if(minute_>p.shiftEnd&&minute_<1200&&p.behavior.spontaneity>.45f)set(IntentKind::Wander,parkId_,-1,24+p.behavior.spontaneity*22,35);
    if(next.kind==IntentKind::None)set(IntentKind::StayHome,p.homeId,-1,10,40);
    if(next.kind!=p.intent.kind||next.targetPlaceId!=p.intent.targetPlaceId){emit(p.id,"intent",p.name+" formed a new plan",.08f);}
    p.intent=next;
}

void World::applyIntent(Person&p){switch(p.intent.kind){case IntentKind::Sleep:moveTo(p,p.homeId,Activity::Sleeping);break;case IntentKind::StayHome:case IntentKind::ReturnHome:moveTo(p,p.homeId,Activity::AtHome);break;case IntentKind::Work:moveTo(p,p.workplaceId,Activity::Working);break;case IntentKind::Eat:moveTo(p,p.intent.targetPlaceId,Activity::Eating);break;case IntentKind::Shop:moveTo(p,shopId_,Activity::Shopping);break;case IntentKind::Relax:moveTo(p,parkId_,Activity::Relaxing);break;case IntentKind::Wander:moveTo(p,parkId_,Activity::Wandering);break;case IntentKind::Socialize:p.activity=Activity::Socializing;break;case IntentKind::Visit:case IntentKind::None:break;}}
void World::updateRoutine(Person&p){updateIntent(p);applyIntent(p);}

void World::updateTravel(Person&p,int dt){if(p.activity!=Activity::Commuting||p.destinationPlaceId<0)return;p.travelMinutesRemaining=std::max(0,p.travelMinutesRemaining-dt);if(p.travelMinutesRemaining)return;p.currentPlaceId=p.destinationPlaceId;p.activity=p.destinationActivity;p.originPlaceId=-1;p.destinationPlaceId=-1;p.travelMinutesTotal=p.travelMinutesRemaining=0;emit(p.id,"arrival",p.name+" arrived",.06f);}

void World::updatePerception(){for(auto&c:citizens_)c.visiblePeople.clear();for(size_t i=0;i<citizens_.size();++i)for(size_t j=i+1;j<citizens_.size();++j){auto&a=citizens_[i];auto&b=citizens_[j];if(a.currentPlaceId>=0&&a.currentPlaceId==b.currentPlaceId&&a.activity!=Activity::Sleeping&&b.activity!=Activity::Sleeping){a.visiblePeople.push_back(b.id);b.visiblePeople.push_back(a.id);}}}

void World::handleInteractions(){int now=absoluteMinute(day_,minute_);for(auto&a:citizens_){if(a.activeInteractionId>=0||now<a.interactionCooldownUntil||a.activity==Activity::Working||a.activity==Activity::Sleeping||a.activity==Activity::Commuting)continue;int target=chooseSocialTarget(a);auto*b=personById(target);if(!b||b->activeInteractionId>=0||now<b->interactionCooldownUntil)continue;float chance=.015f+.05f*a.traits.sociability+.035f*a.behavior.talkativeness;if(a.intent.kind==IntentKind::Socialize)chance+=.18f;if(random01()>chance)continue;Interaction x;x.id=nextInteractionId_++;x.participants={a.id,b->id};x.initiatorId=a.id;x.placeId=a.currentPlaceId;x.phase=InteractionPhase::Notice;x.startedDay=day_;x.startedMinute=minute_;x.phaseMinute=now;interactions_.push_back(x);a.activeInteractionId=b->activeInteractionId=x.id;emit(a.id,"interaction_start",a.name+" noticed "+b->name,.18f,b->id);}}

void World::advanceInteractions(){int now=absoluteMinute(day_,minute_);for(auto&x:interactions_){if(x.phase==InteractionPhase::None||x.phase==InteractionPhase::Disengage)continue;int age=now-x.phaseMinute;if(x.phase==InteractionPhase::Notice&&age>=2){x.phase=InteractionPhase::Approach;x.phaseMinute=now;}else if(x.phase==InteractionPhase::Approach&&age>=2){x.phase=InteractionPhase::Engage;x.phaseMinute=now;emit(x.initiatorId,"conversation","A conversation began",.22f,x.participants.size()>1?x.participants[1]:-1);}else if(x.phase==InteractionPhase::Engage&&age>=8){x.phase=InteractionPhase::Disengage;x.phaseMinute=now;if(x.participants.size()>=2){auto*a=personById(x.participants[0]);auto*b=personById(x.participants[1]);if(a&&b){auto&ab=ensureRelationship(*a,b->id);auto&ba=ensureRelationship(*b,a->id);float delta=1.5f+(a->traits.kindness+b->traits.kindness)*1.5f;ab.familiarity=ba.familiarity=clamp100(std::max(ab.familiarity,ba.familiarity)+delta);ab.affinity=ba.affinity=clamp100((ab.affinity+ba.affinity)*.5f+(random01()-.35f)*2);ab.trust=ba.trust=clamp100((ab.trust+ba.trust)*.5f+.8f);ab.lastSeenDay=ba.lastSeenDay=day_;ab.lastSeenMinute=ba.lastSeenMinute=minute_;ab.lastNotableInteractionDay=ba.lastNotableInteractionDay=day_;++ab.interactionCount;++ba.interactionCount;a->loneliness=clamp100(a->loneliness-10);b->loneliness=clamp100(b->loneliness-10);remember(*a,"conversation",b->id,.35f);remember(*b,"conversation",a->id,.35f);a->activeInteractionId=b->activeInteractionId=-1;a->interactionCooldownUntil=b->interactionCooldownUntil=now+35;emit(a->id,"interaction_end",a->name+" and "+b->name+" finished talking",.2f,b->id);}}}}}

void World::handleEconomy(Person&p){if(minute_>=p.shiftEnd&&!p.paidToday&&p.activity!=Activity::Working){p.cash+=p.dailyWage;p.paidToday=true;emit(p.id,"income",p.name+" finished a shift and was paid",.12f);}if(minute_>=1260&&minute_<1270&&p.lastPurchaseDay!=day_&&p.cash<p.housingCostPerDay){p.stress=clamp100(p.stress+10);remember(p,"money_pressure",-1,.55f);}}
int World::chooseEveningPlace(Person&p){if(p.energy<25)return p.homeId;if(p.stress>68)return parkId_;if(p.loneliness>58&&p.cash>=6)return cafeId_;if(p.hunger>62&&p.cash>=12)return shopId_;return random01()<p.traits.sociability*.4f?cafeId_:parkId_;}

void World::moveTo(Person&p,int placeId,Activity activity,const std::string&reason){if(placeId<0)return;if(p.activity==Activity::Commuting){if(p.destinationPlaceId==placeId)p.destinationActivity=activity;return;}if(p.currentPlaceId==placeId){p.activity=activity;return;}p.originPlaceId=p.currentPlaceId;p.destinationPlaceId=placeId;p.destinationActivity=activity;p.travelMinutesTotal=estimateTravelMinutes(p.originPlaceId,placeId);p.travelMinutesRemaining=p.travelMinutesTotal;p.currentPlaceId=-1;p.activity=Activity::Commuting;emit(p.id,"departure",p.name+" started walking",.06f);if(!reason.empty())emit(p.id,"routine",p.name+" "+reason,.03f);}
int World::estimateTravelMinutes(int a,int b)const{auto*from=placeById(a);auto*to=placeById(b);if(!from||!to||a==b)return 0;float d=std::abs(from->position.x-to->position.x)+std::abs(from->position.y-to->position.y);return std::clamp((int)std::ceil(d/(16.f*std::max(.7f,1.f))),8,35);}
Place* World::placeById(int id){for(auto&p:places_)if(p.id==id)return&p;return nullptr;}const Place* World::placeById(int id)const{for(auto&p:places_)if(p.id==id)return&p;return nullptr;}Person* World::personById(int id){for(auto&p:citizens_)if(p.id==id)return&p;return nullptr;}const Person* World::personById(int id)const{for(auto&p:citizens_)if(p.id==id)return&p;return nullptr;}
void World::remember(Person&p,std::string tag,int other,float intensity){p.memories.push_back(Memory{day_,minute_,std::move(tag),other,intensity});if(p.memories.size()>64)p.memories.erase(p.memories.begin());}
void World::emit(int id,std::string type,std::string text,float imp,int other){int place=-1;if(auto*p=personById(id)){place=p->currentPlaceId;if(place<0)place=p->destinationPlaceId;}events_.push_back(Event{day_,minute_,id,other,place,std::move(type),std::move(text),imp});}
float World::random01(){return std::uniform_real_distribution<float>(0,1)(rng_);}int World::randomInt(int a,int b){return std::uniform_int_distribution<int>(a,b)(rng_);}
std::string World::clockLabel()const{std::ostringstream o;o<<"Day "<<day_<<' '<<std::setfill('0')<<std::setw(2)<<minute_/60<<':'<<std::setw(2)<<minute_%60;return o.str();}
std::string World::dailySummary()const{if(citizens_.empty())return"No citizens";float cash=0,stress=0,hunger=0;int walking=0,links=0,active=0;for(auto&c:citizens_){cash+=c.cash;stress+=c.stress;hunger+=c.hunger;walking+=c.activity==Activity::Commuting;links+=(int)c.relationships.size();active+=c.activeInteractionId>=0;}float n=(float)citizens_.size();std::ostringstream o;o<<std::fixed<<std::setprecision(1)<<"population="<<citizens_.size()<<" avg_cash=$"<<cash/n<<" avg_stress="<<stress/n<<" avg_hunger="<<hunger/n<<" walking="<<walking<<" social_links="<<links/2<<" interacting="<<active;return o.str();}
} // namespace sim

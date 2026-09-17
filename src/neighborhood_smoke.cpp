#include "sim/World.hpp"
#include "raylib.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <sstream>

namespace {
constexpr int W=640,H=360,FPS=12;
Color c(unsigned char r,unsigned char g,unsigned char b,unsigned char a=255){return Color{r,g,b,a};}
const sim::Place* place(const sim::World&w,int id){for(auto&p:w.places())if(p.id==id)return&p;return nullptr;}
Vector2 pos(const sim::World&w,const sim::Person&p){
 if(p.activity!=sim::Activity::Commuting){auto*q=place(w,p.currentPlaceId);if(!q)return{320,180};float a=float((p.id*97)%360)*3.14159265f/180.f;return{q->position.x+std::cos(a)*8,q->position.y+std::sin(a)*6};}
 auto*a=place(w,p.originPlaceId);auto*b=place(w,p.destinationPlaceId);if(!a||!b)return{320,180};float t=p.travelMinutesTotal?1.f-float(p.travelMinutesRemaining)/p.travelMinutesTotal:1.f;t=std::clamp(t,0.f,1.f);
 // Deterministic sidewalk/crosswalk route. This smoke shows every traveler rather than only editorial subjects.
 float sy=a->position.y<190?186.f:239.f, ey=b->position.y<190?186.f:239.f; Vector2 A{a->position.x,sy},B{b->position.x,ey};
 if((sy<210)==(ey<210)){if(t<.18f)return{a->position.x+(A.x-a->position.x)*t/.18f,a->position.y+(A.y-a->position.y)*t/.18f};if(t<.82f)return{A.x+(B.x-A.x)*(t-.18f)/.64f,A.y};return{B.x+(b->position.x-B.x)*(t-.82f)/.18f,B.y+(b->position.y-B.y)*(t-.82f)/.18f};}
 float crossX=348.f+float((p.id%3)-1)*2.f; if(t<.15f)return{a->position.x+(A.x-a->position.x)*t/.15f,a->position.y+(A.y-a->position.y)*t/.15f};if(t<.42f)return{A.x+(crossX-A.x)*(t-.15f)/.27f,A.y};if(t<.60f)return{crossX,A.y+(ey-sy)*(t-.42f)/.18f};if(t<.85f)return{crossX+(B.x-crossX)*(t-.60f)/.25f,B.y};return{B.x+(b->position.x-B.x)*(t-.85f)/.15f,B.y+(b->position.y-B.y)*(t-.85f)/.15f};
}
void roads(){DrawRectangle(0,181,W,62,c(158,157,145));DrawRectangle(0,196,W,33,c(73,76,76));DrawRectangle(318,0,60,H,c(158,157,145));DrawRectangle(331,0,34,H,c(73,76,76));for(int x=8;x<W;x+=28)DrawRectangle(x,212,14,2,c(188,176,130));for(int y=6;y<H;y+=27)DrawRectangle(347,y,2,13,c(188,176,130));}
void building(const sim::Place&p,bool night){int x=int(p.position.x),y=int(p.position.y);if(p.type==sim::PlaceType::Park){DrawRectangle(x-47,y-29,94,58,c(88,125,76));DrawRectangle(x-43,y-3,86,6,c(164,149,117));return;}Color wall=p.type==sim::PlaceType::Home?c(204,184,151):p.type==sim::PlaceType::Cafe?c(182,154,126):p.type==sim::PlaceType::Shop?c(191,177,143):c(169,163,147);DrawRectangle(x-27,y-18,54,37,wall);DrawRectangle(x-3,y+4,7,15,c(84,70,58));Color win=night?c(245,205,128):c(105,139,145);DrawRectangle(x-18,y-8,12,10,win);DrawRectangle(x+7,y-8,12,10,win);}
Color shirt(int id){static std::array<Color,8>p={c(170,92,82),c(78,115,145),c(176,131,73),c(103,132,88),c(121,90,139),c(172,108,137),c(78,139,132),c(146,108,72)};return p[size_t(id)%p.size()];}
void person(const sim::World&w,const sim::Person&p,int frame){if(p.activity==sim::Activity::Sleeping)return;Vector2 q=pos(w,p);bool walk=p.activity==sim::Activity::Commuting;float bob=walk?std::sin(frame*.55f+p.id)*1.2f:0;int x=int(q.x),y=int(q.y+bob);DrawCircle(x,y+5,4,Fade(BLACK,.2f));DrawRectangle(x-2,y-1,5,7,shirt(p.id));DrawCircle(x,y-4,3,c(211,172,135));if(walk){int s=std::sin(frame*.75f+p.id)>0?1:-1;DrawLine(x-1,y+6,x-2*s,y+10,c(55,58,62));DrawLine(x+1,y+6,x+2*s,y+10,c(55,58,62));}if(p.activeInteractionId>=0){DrawCircleLines(x,y-4,6,c(245,220,145));}}
float dark(int m){if(m>=20*60||m<5*60)return .48f;if(m>=18*60)return .48f*(m-1080)/120.f;if(m<420)return .48f*(420-m)/120.f;return 0;}
std::string frameName(const std::filesystem::path&d,int n){std::ostringstream s;s<<"frame_"<<std::setfill('0')<<std::setw(5)<<n<<".png";return(d/s.str()).string();}
}
int main(int argc,char**argv){unsigned seed=argc>1?std::stoul(argv[1]):48192;int pop=argc>2?std::stoi(argv[2]):20;std::filesystem::path out=argc>3?argv[3]:"visual-smoke";std::filesystem::create_directories(out/"frames");sim::World world(seed,pop);SetConfigFlags(FLAG_WINDOW_HIDDEN|FLAG_WINDOW_UNDECORATED);InitWindow(W,H,"Mason Block V3 neighborhood smoke");RenderTexture2D tex=LoadRenderTexture(W,H);int frame=0;constexpr int videoSeconds=75,simMinutesPerSecond=16;for(int sec=0;sec<videoSeconds;++sec){world.step(simMinutesPerSecond);for(int f=0;f<FPS;++f){BeginTextureMode(tex);ClearBackground(c(101,123,82));roads();bool night=world.minute()>=1140||world.minute()<360;for(auto&p:world.places())building(p,night);for(auto&p:world.citizens())person(world,p,frame);float a=dark(world.minute());if(a>0)DrawRectangle(0,0,W,H,Fade(c(27,39,66),a));DrawRectangle(8,8,168,28,Fade(BLACK,.72f));DrawText("MASON BLOCK / V3 SMOKE",14,13,8,c(242,235,207));std::ostringstream clock;clock<<"DAY "<<world.day()<<"  "<<std::setfill('0')<<std::setw(2)<<world.minute()/60<<':'<<std::setw(2)<<world.minute()%60;DrawText(clock.str().c_str(),14,24,7,c(194,197,187));EndTextureMode();Image im=LoadImageFromTexture(tex.texture);ImageFlipVertical(&im);ExportImage(im,frameName(out/"frames",frame).c_str());UnloadImage(im);++frame;}}
UnloadRenderTexture(tex);CloseWindow();std::string cmd="ffmpeg -y -loglevel error -framerate 12 -i '"+(out/"frames/frame_%05d.png").string()+"' -vf 'scale=1280:720:flags=neighbor' -c:v libx264 -preset fast -crf 20 -pix_fmt yuv420p -an -movflags +faststart '"+(out/"mason-v3-neighborhood-smoke.mp4").string()+"'";int status=std::system(cmd.c_str());std::filesystem::remove_all(out/"frames");std::cout<<"seed="<<seed<<" population="<<pop<<" video_seconds="<<videoSeconds<<" simulated_minutes="<<videoSeconds*simMinutesPerSecond<<" events="<<world.events().size()<<"\n";return status==0?0:1;}

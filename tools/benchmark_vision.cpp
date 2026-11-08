#include <chrono>
#include <iostream>
#include <iomanip>
#include <vector>
#include "host_v46o/Arduino.h"
#include "../src/white_marker_tracker.h"
static uint32_t state=1;
uint32_t randv(){state=state*1664525u+1013904223u;return state;}
int main(){
 using namespace std::chrono;
 WhiteMarker1DTracker a(0), b(1);std::vector<uint8_t> im(320*240);
 uint64_t ns=0;
 for(int k=0;k<2000;k++){
  for(auto&v:im)v=(randv()>>24)%60;
  if(k%53==0) for(auto&v:im)v=(k%106==0)?0:255;
  int mx=30+static_cast<int>(randv()%260), my=66+static_cast<int>(randv()%65)-32;
  int nx=30+static_cast<int>(randv()%260), ny=160+static_cast<int>(randv()%65)-32;
  if(k%31==0) {mx=0;nx=319;} // edge and clipping
  if(k%17<8) {mx=100;my=66;nx=180;ny=160;} // stable/reacquisition sequences
  if(k%53!=0) for(int y=0;y<240;y++)for(int x=0;x<320;x++){
   if((abs(x-mx)<(int)(3+((k/50)%23)) && abs(y-my)<11)||(abs(x-nx)<11 && abs(y-ny)<11))im[y*320+x]=180+(randv()%76);
  }
  host_us+=100000;auto t0=steady_clock::now();
  const uint8_t* frame = k%37 ? im.data() : nullptr;
  auto o1=a.process(frame);auto o2=b.process(frame);
  ns+=duration_cast<nanoseconds>(steady_clock::now()-t0).count();
  for(auto o:{o1,o2})std::cout<<o.valid<<' '<<(int)o.reason<<' '<<(int)o.templates_tested<<' '<<(int)o.candidate_count<<' '<<std::hexfloat<<o.ambiguity_ratio<<' '<<o.alternate_x_px<<' '<<o.alternate_y_px<<' '<<o.center_x_px<<' '<<o.center_y_px<<' '<<o.peak_contrast<<' '<<o.weight_sum<<' '<<o.peak_x_px<<' '<<o.bright_width_px<<' '<<o.success_count<<' '<<o.fail_count<<'\n';
 }
 std::cerr<<"average_us_per_frame="<<ns/2000.0/1000<<"\n";
}

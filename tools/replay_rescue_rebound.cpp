#include "../src/rescue_rebound.h"
#include <iostream>
#include <cstdlib>
int main(int argc, char** argv) {
 if(argc!=2) return 2;
 rescue_rebound::Detector d;unsigned t;float angle,measured,rate;int health;
 d.arm(static_cast<unsigned>(std::strtoul(argv[1],nullptr,10)),1);
 while(std::cin>>t>>angle>>measured>>rate>>health) {
  auto p=d.observe(t,angle,measured,rate,health);
  if(p.valid)std::cout<<p.ms<<" "<<p.angle<<" "<<p.detector_angle<<"\n";
 }
}

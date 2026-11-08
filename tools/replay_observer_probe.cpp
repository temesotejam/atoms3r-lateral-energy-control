#include "foot_fullstride_observer.h"
#include <cstdint>
#include <iomanip>
#include <iostream>

int main() {
  foot_fullstride_observer::Observer o; o.reset(1);
  char kind; unsigned t; int a,b; float l,r; uint32_t previous=0;
  while(std::cin>>kind) {
    if(kind=='P') { std::cin>>t>>a; o.peak(1,a,t); }
    else if(kind=='F') { std::cin>>t>>a>>b>>l>>r; o.frame(1,t,a,b,l,r); }
    else return 2;
    const auto& c=o.latest();
    if(c.sequence!=previous) {
      previous=c.sequence;
      std::cout<<c.sequence<<' '<<c.start_ms<<' '<<c.end_ms<<' '<<c.valid<<' '
               <<c.left.valid<<' '<<c.right.valid<<' '<<c.continuous<<' '
               <<std::hexfloat<<c.left.span<<' '<<c.right.span<<'\n';
    }
  }
}

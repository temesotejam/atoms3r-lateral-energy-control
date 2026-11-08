#include "../src/foot_fast_record_gate.h"
#include <cassert>
#include <cstdint>
#include <initializer_list>
int main() {
 for (uint64_t step : {10000ULL,50000ULL,100000ULL}) {
   uint64_t next=0; int n=0;
   // 80s bounds the 60s passive measurement plus synchronization margin.
   for (uint64_t t=1; t<=80000001; t+=step) n+=fastRecordGate(t,next);
   assert(n>=639 && n<=641 && n<768);
 }
 uint64_t next=0; assert(!fastRecordGate(0,next)); assert(fastRecordGate(100000,next));
 assert(!fastRecordGate(150000,next)); assert(fastRecordGate(1000000,next));
 assert(!fastRecordGate(1000001,next));
}

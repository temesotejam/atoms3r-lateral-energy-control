#include "../src/foot_fast_record_gate.h"
#include <cassert>
#include <cstdint>
int main() {
 uint64_t next = 0; int n=0;
 for (uint64_t t=1; t<=40000001; t+=50000) n+=fastRecordGate(t,next);
 assert(n>=599 && n<=601 && n<768);
 next=0; n=0;
 for (uint64_t t=1; t<=40000001; t+=100000) n+=fastRecordGate(t,next);
 assert(n>=399 && n<=401);
 next=0; assert(!fastRecordGate(0,next)); assert(fastRecordGate(100000,next));
 assert(!fastRecordGate(150000,next)); assert(fastRecordGate(1000000,next));
 assert(!fastRecordGate(1000001,next));
}

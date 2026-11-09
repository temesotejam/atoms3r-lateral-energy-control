#include "../src/camera_experiment_mode.h"
#include <cassert>
#include <cstring>
int main(){
 assert(cameraExperimentAllowsCurrent(0));
#if CAMERA_DIAGNOSTIC_ONLY
 assert(!cameraExperimentAllowsCurrent(300)); assert(!cameraExperimentAllowsCurrent(-300));
#else
 assert(cameraExperimentAllowsCurrent(300)); assert(cameraExperimentAllowsCurrent(-300));
#endif
 char sha[41]; cameraBuildSha(sha);
 assert(strlen(sha)==7 || strlen(sha)==40);
}

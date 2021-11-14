// Bridging header between C & Swift

// C interface to Swift funtions

// v0.1 API

#include <stdint.h>
#include <stdbool.h>

typedef int64_t RetCode;

RetCode mc_sw_init(uint64_t device_index);
RetCode mc_sw_release();
RetCode mc_sw_compile(const char* program, const char* functionName);
RetCode mc_sw_alloc(int icount, float* input, int iformat, int ocount, int oformat); // Allocate I/O buffers and fill input buffer
RetCode mc_sw_run();
RetCode mc_sw_retrieve(int ocount, float* output); // Copy results to output buffer
char* mc_sw_get_compile_error(); // Must free after calling

// v0.2 API

typedef struct _mc_dev {
    char* name; 
    int64_t recommendedMaxWorkingSetSize;
    bool hasUnifiedMemory;
    int64_t maxTransferRate;
} mc_dev;

typedef struct _mc_shared {
    int64_t dev_count;
    mc_dev* devs;
} mc_shared;

RetCode mc_sw_count_devs(mc_shared* shared);

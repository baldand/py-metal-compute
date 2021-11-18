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

typedef struct {
    char* name; 
    int64_t recommendedMaxWorkingSetSize;
    bool hasUnifiedMemory;
    int64_t maxTransferRate;
} mc_dev;

typedef struct {
    int64_t dev_count;
    mc_dev* devs;
} mc_devices;

RetCode mc_sw_count_devs(mc_devices* devices);

typedef struct {
    int64_t id;
    char* name;
} mc_dev_handle;

typedef struct {
    int64_t id;
} mc_kern_handle;

typedef struct {
    int64_t id;
} mc_fn_handle;

typedef struct {
    int64_t id;
    char* buf;
    int64_t length;
} mc_buf_handle;

typedef struct {
    int64_t id;
    int64_t kcount;
    int64_t buf_count;
    mc_buf_handle** bufs;
} mc_run_handle;

RetCode mc_sw_dev_open(uint64_t device_index, mc_dev_handle* dev_handle);
RetCode mc_sw_dev_close(mc_dev_handle* dev_handle);
RetCode mc_sw_kern_open(const mc_dev_handle* dev_handle, const char* program, mc_kern_handle* kern_handle);
RetCode mc_sw_kern_close(const mc_dev_handle* dev_handle, mc_kern_handle* kern_handle);
RetCode mc_sw_fn_open(const mc_dev_handle* dev_handle, const mc_kern_handle* kern_handle, const char* func_name, mc_fn_handle* fn_handle);
RetCode mc_sw_fn_close(const mc_dev_handle* dev_handle, const mc_kern_handle* kern_handle, mc_fn_handle* fn_handle);
RetCode mc_sw_buf_open(const mc_dev_handle* dev_handle, uint64_t length, char* src, mc_buf_handle* buf_handle);
RetCode mc_sw_buf_close(const mc_dev_handle* dev_handle, mc_buf_handle* buf_handle);
RetCode mc_sw_run_open(const mc_dev_handle* dev_handle, const mc_kern_handle* kern_handle,
                     const mc_fn_handle* fn_handle, mc_run_handle* run_handle);
RetCode mc_sw_run_close(const mc_run_handle* run_handle);

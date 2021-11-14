/*
metalcompute.c

Python extension to use Metal for compute

(c) Andrew Baldwin 2021
*/

#define PY_SSIZE_T_CLEAN
#include <Python.h>

#include "metalcompute.h"

// Value symbols are shared with Swift, so declared extern in shared header and defined here

const RetCode Success = 0;
const RetCode CannotCreateDevice = -1;
const RetCode CannotCreateCommandQueue = -2;
const RetCode NotReadyToCompile = -3;
const RetCode FailedToCompile = -4;
const RetCode FailedToFindFunction = -5;
const RetCode NotReadyToCompute = -6;
const RetCode FailedToMakeInputBuffer = -7;
const RetCode FailedToMakeOutputBuffer = -8;
const RetCode NotReadyToRun = -9;
const RetCode CannotCreateCommandBuffer = -10;
const RetCode CannotCreateCommandEncoder = -11;
const RetCode CannotCreatePipelineState = -12;
const RetCode IncorrectOutputCount = -13;
const RetCode NotReadyToRetrieve = -14;
const RetCode UnsupportedInputFormat = -15;
const RetCode UnsupportedOutputFormat = -16;
// Reserved block for Swift level errors

// Buffer formats
const long FormatUnknown = -1;
const long FormatI8 = 0;
const long FormatU8 = 1;
const long FormatI16 = 2;
const long FormatU16 = 3;
const long FormatI32 = 4;
const long FormatU32 = 5;
const long FormatI64 = 6;
const long FormatU64 = 7;
const long FormatF16 = 8;
const long FormatF32 = 9;
const long FormatF64 = 10;


static PyObject *MetalComputeError;

RetCode mc_err(RetCode ret) {
    // Map error codes to exception with string
    if (ret != Success) {

        const char* errString = "Unknown error";

        switch (ret) {
            case CannotCreateDevice: errString = "Cannot create device"; break;
            case CannotCreateCommandQueue: errString = "Cannot create command queue"; break;
            case NotReadyToCompile: errString = "Not ready to compile"; break;
            case FailedToCompile: errString = "Failed to compile"; break;
            case FailedToFindFunction: errString = "Failed to find function"; break;
            case NotReadyToCompute: errString = "Not ready to compute"; break;
            case FailedToMakeInputBuffer: errString = "Failed to make input buffer"; break;
            case FailedToMakeOutputBuffer: errString = "Failed to make output buffer"; break;
            case NotReadyToRun: errString = "Not ready to run"; break;
            case CannotCreateCommandBuffer: errString = "Cannot create command buffer"; break;
            case CannotCreateCommandEncoder: errString = "Cannot create command encoder"; break;
            case CannotCreatePipelineState: errString = "Cannot create pipeline state"; break;
            case IncorrectOutputCount: errString = "Incorrect output count"; break;
            case NotReadyToRetrieve: errString = "Not ready to retrieve"; break;
            case UnsupportedInputFormat: errString = "Unsupported input format"; break;
            case UnsupportedOutputFormat: errString = "Unsupported output format"; break;
            // C level errors below
        }

        if (ret == FailedToCompile) {
            char* compileErrString = mc_sw_get_compile_error(); 
            PyErr_SetString(MetalComputeError, compileErrString);
            free(compileErrString);
        } else {
            PyErr_SetString(MetalComputeError, errString);
        }

    }
    return ret;
}

static PyObject *
mc_py_1_init(PyObject *self, PyObject *args)
 {
    uint64_t device_index = -1; // Default device
    PyArg_ParseTuple(args, "|L", &device_index); // Device is optional argument

    if (mc_err(mc_sw_init(device_index)))
        return NULL;

    Py_RETURN_NONE;
}

static PyObject *
mc_py_1_release(PyObject *self, PyObject *args)
{
    if (mc_err(mc_sw_release()))
        return NULL;

    Py_RETURN_NONE;
}

static PyObject *
mc_py_1_compile(PyObject *self, PyObject *args)
{
    const char *program;
    const char *functionName;

    if (!PyArg_ParseTuple(args, "ss", &program, &functionName))
        return NULL;

    if (mc_err(mc_sw_compile(program, functionName)))
        return NULL;

    Py_RETURN_NONE;
}

int format_buf_to_mc(char* buf_format) {
    if (buf_format == 0) {
        // NULL -> "B" -> uint8
        return FormatU8;
    } else if (buf_format[0]=='f' && buf_format[1]==0) {
        // OK "f" -> float32
        return FormatF32;
    } else if (buf_format[0]=='B' && buf_format[1]==0) {
        // OK "B" -> uint8
        return FormatU8;
    } else {
        // Unrecognized output format
        return FormatUnknown;
    }
}

static PyObject *
mc_py_1_run(PyObject *self, PyObject *args)
{
    PyObject* input_object;
    PyObject* output_object;
    Py_buffer input;
    Py_buffer output;
    int kcount, ret = 0;

    if (!PyArg_ParseTuple(args, "OOi", &input_object, &output_object, &kcount))
        return NULL;

    ret = PyObject_GetBuffer(input_object, &input, PyBUF_FORMAT|PyBUF_ND|PyBUF_C_CONTIGUOUS);
    if (ret != 0) {
        mc_err(UnsupportedInputFormat);
        return NULL;
    }

    ret = PyObject_GetBuffer(output_object, &output, PyBUF_FORMAT|PyBUF_WRITEABLE|PyBUF_ND|PyBUF_C_CONTIGUOUS);
    if (ret != 0) {
        mc_err(UnsupportedOutputFormat);
        PyBuffer_Release(&input);
        return NULL;
    }

    int icount = input.len / input.itemsize;
    int ocount = output.len / output.itemsize;
    // Copy data into metal buffer and release input

    int input_format = format_buf_to_mc(input.format);
    int output_format = format_buf_to_mc(output.format);
    if (input_format == FormatUnknown) ret = UnsupportedInputFormat;
    if (output_format == FormatUnknown) ret = UnsupportedOutputFormat;
    if (mc_err(ret)) {
        PyBuffer_Release(&input);
        PyBuffer_Release(&output);
        return NULL;
    }
    ret = mc_sw_alloc(icount, input.buf, input_format, ocount, output_format);
    PyBuffer_Release(&input);
    if (mc_err(ret)) {
        PyBuffer_Release(&output);
        return NULL;
    }

    // Run compute without lock
    Py_BEGIN_ALLOW_THREADS
    ret = mc_sw_run(kcount);
    Py_END_ALLOW_THREADS
    
    if (mc_err(ret)) {
        PyBuffer_Release(&output);
        return NULL;
    }

    // Retrieve result
    ret = mc_sw_retrieve(ocount, output.buf);
    PyBuffer_Release(&output);

    if (mc_err(ret))
        return NULL;

    Py_RETURN_NONE;
}

static PyObject *
mc_py_1_rerun(PyObject *self, PyObject *args)
{
    int kcount;

    if (!PyArg_ParseTuple(args, "i", &kcount))
        return NULL;

    int ret = 0;
    // Run compute without lock
    Py_BEGIN_ALLOW_THREADS
    ret = mc_sw_run(kcount);
    Py_END_ALLOW_THREADS
    
    if (mc_err(ret)) {
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyTypeObject *DeviceItem;

static PyObject *
mc_py_2_get_devices(PyObject *self, PyObject *args)
 {
    mc_shared share;
    if (mc_err(mc_sw_count_devs(&share)))
        return NULL;
    PyObject *dev_result = PyTuple_New(share.dev_count);
    for (int i=0; i<share.dev_count; i++) {
        PyObject* device_item = PyStructSequence_New(DeviceItem);
        PyObject* name = PyUnicode_FromString(share.devs[i].name);
        Py_INCREF(name);
        PyStructSequence_SetItem(device_item, 0, name);
        free(share.devs[i].name); // Done with Swift-provided string
        PyObject* working = PyLong_FromLongLong(share.devs[i].recommendedMaxWorkingSetSize);
        Py_INCREF(working);
        PyStructSequence_SetItem(device_item, 1, working);
        PyObject* transfer = PyLong_FromLongLong(share.devs[i].maxTransferRate);
        Py_INCREF(transfer);
        PyStructSequence_SetItem(device_item, 2, transfer);
        PyObject* unified = PyBool_FromLong(share.devs[i].hasUnifiedMemory);
        Py_INCREF(unified);
        PyStructSequence_SetItem(device_item, 3, unified);
        PyTuple_SetItem(dev_result, i, device_item);
    }
    free(share.devs); // Done with the Swift-provided array
    
    return dev_result;
}

static PyMethodDef MetalComputeMethods[] = {
    // v0.1 functions - simple/deprecated
    {"init",  mc_py_1_init, METH_VARARGS,
     "init"},
    {"release",  mc_py_1_release, METH_VARARGS,
     "release"},
    {"compile",  mc_py_1_compile, METH_VARARGS,
     "compile"},
    {"run",  mc_py_1_run, METH_VARARGS,
     "run"},
    {"rerun",  mc_py_1_rerun, METH_VARARGS,
     "rerun"},

    // v0.2 functions - more flexible/current
    { "get_devices", mc_py_2_get_devices, METH_VARARGS, "get_devices" },

    // End
    {NULL, NULL, 0, NULL}        /* Sentinel */
};

static struct PyModuleDef metalcomputemodule = {
    PyModuleDef_HEAD_INIT,
    "metalcompute",   /* name of module */
    "Run metal compute kernels", /* module documentation, may be NULL */
    -1,       /* size of per-interpreter state of the module,
                 or -1 if the module keeps state in global variables. */
    MetalComputeMethods
};

void define_device_item_type() {
    PyStructSequence_Field fields[5] = {
        { .name="deviceName", .doc="" },
        { .name="recommendedWorkingSetSize", .doc="" },
        { .name="maxTransferRate", .doc="" },
        { .name="hasUnifiedMemory", .doc="" },
        { .name=0, .doc=NULL }
    };
    PyStructSequence_Desc dev_item_desc = {
        .name = "metalcompute_device",
        .doc = "",
        .fields = fields,
        .n_in_sequence = 4 };
    DeviceItem = PyStructSequence_NewType(&dev_item_desc);
}

PyMODINIT_FUNC
PyInit_metalcompute(void)
{
    //printf("(creating stdout)\n"); // Uncomment if debugging swift code with print statements

    PyObject *m;

    m = PyModule_Create(&metalcomputemodule);
    if (m == NULL)
        return NULL;

    MetalComputeError = PyErr_NewException("metalcompute.error", NULL, NULL);
    Py_XINCREF(MetalComputeError);
    if (PyModule_AddObject(m, "error", MetalComputeError) < 0) {
        Py_XDECREF(MetalComputeError);
        Py_CLEAR(MetalComputeError);
        Py_DECREF(m);
        return NULL;
    }

    define_device_item_type();

    return m;
}





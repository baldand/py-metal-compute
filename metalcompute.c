/*
metalcompute.c

Python extension to use Metal for compute

(c) Andrew Baldwin 2021
*/

#define PY_SSIZE_T_CLEAN
#include <Python.h>

static PyObject *MetalComputeError;

/* C interface to Swift library */
int mc_sw_init();
int mc_sw_release();
int mc_sw_compile(const char* program, const char* functionName);
int mc_sw_alloc(int icount, float* input, int iformat, int ocount, int oformat); // Allocate I/O buffers and fill input buffer
int mc_sw_run();
int mc_sw_retrieve(int ocount, float* output); // Copy results to output buffer
char* mc_sw_get_compile_error(); // Must free after calling

const int Success = 0;
const int CannotCreateDevice = -1;
const int CannotCreateCommandQueue = -2;
const int NotReadyToCompile = -3;
const int FailedToCompile = -4;
const int FailedToFindFunction = -5;
const int NotReadyToCompute = -6;
const int FailedToMakeInputBuffer = -7;
const int FailedToMakeOutputBuffer = -8;
const int NotReadyToRun = -9;
const int CannotCreateCommandBuffer = -10;
const int CannotCreateCommandEncoder = -11;
const int CannotCreatePipelineState = -12;
const int IncorrectOutputCount = -13;
const int NotReadyToRetrieve = -14;
const int UnsupportedInputFormat = -15;
const int UnsupportedOutputFormat = -16;
// Reserved block for Swift level errors

// Buffer formats
const int FormatUnknown = -1;
const int FormatI8 = 0;
const int FormatU8 = 1;
const int FormatI16 = 2;
const int FormatU16 = 3;
const int FormatI32 = 4;
const int FormatU32 = 5;
const int FormatI64 = 6;
const int FormatU64 = 7;
const int FormatF16 = 8;
const int FormatF32 = 9;
const int FormatF64 = 10;

int mc_err(int ret) {
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
mc_py_init(PyObject *self, PyObject *args)
{
    if (mc_err(mc_sw_init()))
        return NULL;

    Py_RETURN_NONE;
}

static PyObject *
mc_py_release(PyObject *self, PyObject *args)
{
    if (mc_err(mc_sw_release()))
        return NULL;

    Py_RETURN_NONE;
}

static PyObject *
mc_py_compile(PyObject *self, PyObject *args)
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
mc_py_run(PyObject *self, PyObject *args)
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
mc_py_rerun(PyObject *self, PyObject *args)
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


static PyMethodDef MetalComputeMethods[] = {
    {"init",  mc_py_init, METH_VARARGS,
     "init"},
    {"release",  mc_py_release, METH_VARARGS,
     "release"},
    {"compile",  mc_py_compile, METH_VARARGS,
     "compile"},
    {"run",  mc_py_run, METH_VARARGS,
     "run"},
    {"rerun",  mc_py_rerun, METH_VARARGS,
     "rerun"},
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

PyMODINIT_FUNC
PyInit_metalcompute(void)
{
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

    return m;
}





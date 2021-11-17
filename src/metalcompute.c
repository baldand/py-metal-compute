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

// v2 errors
const RetCode DeviceNotFound = -1000;
const RetCode KernelNotFound = -1001;
const RetCode FunctionNotFound = -1002;
const RetCode CouldNotMakeBuffer = -1003;
const RetCode BufferNotFound = -1004;

// Python level errors
const RetCode FirstArgumentNotDevice = -2000;
const RetCode FirstArgumentNotKernel = -2001;

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
            // v2 errors
            case DeviceNotFound: errString = "Device not found"; break;
            case KernelNotFound: errString = "Kernel not found"; break;
            case FunctionNotFound: errString = "Function not found"; break;
            case CouldNotMakeBuffer: errString = "Could not make buffer"; break;
            case BufferNotFound: errString = "Buffer not found"; break;
            // Python level errors
            case FirstArgumentNotDevice: errString = "First argument should be a metalcompute.Device object"; break;
            case FirstArgumentNotKernel: errString = "First argument should be a metalcompute.Kernel object"; break;
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

static PyTypeObject *DeviceInfo;

static PyObject *
mc_py_2_get_devices(PyObject *self, PyObject *args)
 {
    mc_devices devices;
    if (mc_err(mc_sw_count_devs(&devices)))
        return NULL;
    PyObject *dev_result = PyTuple_New(devices.dev_count);
    for (int i=0; i<devices.dev_count; i++) {
        PyObject* device_item = PyStructSequence_New(DeviceInfo);
        PyObject* name = PyUnicode_FromString(devices.devs[i].name);
        Py_INCREF(name);
        PyStructSequence_SetItem(device_item, 0, name);
        free(devices.devs[i].name); // Done with Swift-provided string
        PyObject* working = PyLong_FromLongLong(devices.devs[i].recommendedMaxWorkingSetSize);
        Py_INCREF(working);
        PyStructSequence_SetItem(device_item, 1, working);
        PyObject* transfer = PyLong_FromLongLong(devices.devs[i].maxTransferRate);
        Py_INCREF(transfer);
        PyStructSequence_SetItem(device_item, 2, transfer);
        PyObject* unified = PyBool_FromLong(devices.devs[i].hasUnifiedMemory);
        Py_INCREF(unified);
        PyStructSequence_SetItem(device_item, 3, unified);
        PyTuple_SetItem(dev_result, i, device_item);
    }
    free(devices.devs); // Done with the Swift-provided array
    
    return dev_result;
}

typedef struct {
    PyObject_HEAD
    mc_dev_handle dev_handle;
} Device;

typedef struct {
    PyObject_HEAD
    Device* dev_obj;
    mc_kern_handle kern_handle;
} Kernel;

typedef struct {
    PyObject_HEAD
    Kernel* kern_obj;
    mc_fn_handle fn_handle;
} Function;

typedef struct {
    PyObject_HEAD
    Device* dev_obj;
    mc_buf_handle buf_handle;
    uint64_t length;
    uint64_t exports;
} Buffer;

static int
Device_init(Device *self, PyObject *args, PyObject *kwds)
{
    uint64_t device_index = -1; // Default device
    PyArg_ParseTuple(args, "|L", &device_index); // Device is optional argument

    if (mc_err(mc_sw_dev_open(device_index, &(self->dev_handle))))
        return -1;

    //printf("Device init called %s\n", self->handle.name);
    return 0;
}

static void
Device_dealloc(Device *self)
{
    if (self->dev_handle.id != 0) {
        free(self->dev_handle.name); // Name string allocated by Swift on open
        mc_sw_dev_close(&(self->dev_handle));
    }
    Py_TYPE(self)->tp_free((PyObject *) self);
}

static PyObject *
Device_str(Device* self)
{
    return PyUnicode_FromFormat("metalcompute.Device(%s)", self->dev_handle.name);
}

static PyTypeObject KernelType; // Forward reference
static PyTypeObject BufferType; // Forward reference

static PyObject *
Device_kernel(Device* self, PyObject* args, PyObject* kwargs)
{
    PyObject* first_arg;

    if (!PyArg_ParseTuple(args, "O", &first_arg))
        return NULL;

    PyObject *kernelArgList = Py_BuildValue("OO", self, first_arg);
    PyObject *newKernelObj = PyObject_CallObject((PyObject *) &KernelType, kernelArgList);
    Py_DECREF(kernelArgList);
    return newKernelObj;
}

static PyObject *
Device_buffer(Device* self, PyObject* args, PyObject* kwargs)
{
    PyObject* first_arg;

    if (!PyArg_ParseTuple(args, "O", &first_arg))
        return NULL;

    PyObject *bufferArgList = Py_BuildValue("OO", self, first_arg);
    PyObject *newBufferObj = PyObject_CallObject((PyObject *) &BufferType, bufferArgList);
    Py_DECREF(bufferArgList);
    return newBufferObj;
}

static PyMethodDef Device_methods[] = {
    {"kernel", (PyCFunction) Device_kernel, METH_VARARGS,
     "Compile a kernel for this device"
    },
    {"buffer", (PyCFunction) Device_buffer, METH_VARARGS,
     "Create a buffer for this device"
    },
    {NULL}  /* Sentinel */
};

static PyTypeObject DeviceType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "metalcompute.Device",
    .tp_doc = "A Metal device which can be to allocated buffer, compile and run kernels",
    .tp_basicsize = sizeof(Device),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
    .tp_init = (initproc) Device_init,
    .tp_dealloc = (destructor) Device_dealloc,
    .tp_str = (reprfunc) Device_str,
    .tp_methods = Device_methods,
};

static int
Kernel_init(Kernel *self, PyObject *args, PyObject *kwds)
{
    PyObject* dev_obj;
    const char *program;

    if (!PyArg_ParseTuple(args, "Os", &dev_obj, &program))
        return -1;

    if (dev_obj->ob_type != &DeviceType) {
        mc_err(FirstArgumentNotDevice);
        return -1;
    }

    self->dev_obj = (Device*)dev_obj;

    if (mc_err(mc_sw_kern_open(&(self->dev_obj->dev_handle), program, &(self->kern_handle))))
        return -1;

    Py_INCREF(dev_obj); // Cannot close device while kernel open

    return 0;
}

static void
Kernel_dealloc(Kernel *self)
{
    if (self->kern_handle.id != 0) {
        mc_sw_kern_close(&(self->dev_obj->dev_handle), &(self->kern_handle));
        Py_DECREF(self->dev_obj);
    }
    Py_TYPE(self)->tp_free((PyObject *) self);
}

static PyObject *
Kernel_str(Kernel* self)
{
    return PyUnicode_FromFormat("metalcompute.Kernel");
}

static PyTypeObject FunctionType; // Forward reference

static PyObject *
Kernel_function(Kernel* self, PyObject* args, PyObject* kwargs)
{
    PyObject* first_arg;

    if (!PyArg_ParseTuple(args, "O", &first_arg))
        return NULL;

    PyObject *functionArgList = Py_BuildValue("OO", self, first_arg);
    PyObject *newFunctionObj = PyObject_CallObject((PyObject *) &FunctionType, functionArgList);
    Py_DECREF(functionArgList);
    return newFunctionObj;
}

static PyMethodDef Kernel_methods[] = {
    {"function", (PyCFunction) Kernel_function, METH_VARARGS,
     "Link a funtion from this kernel"
    },
    {NULL}  /* Sentinel */
};

static PyTypeObject KernelType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "metalcompute.Kernel",
    .tp_doc = "A Metal compute kernel with one or more functions",
    .tp_basicsize = sizeof(Kernel),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
    .tp_init = (initproc) Kernel_init,
    .tp_dealloc = (destructor) Kernel_dealloc,
    .tp_str = (reprfunc) Kernel_str,
    .tp_methods = Kernel_methods
};

static int
Function_init(Function *self, PyObject *args, PyObject *kwds)
{
    PyObject* kern_obj;
    const char *func_name;

    if (!PyArg_ParseTuple(args, "Os", &kern_obj, &func_name))
        return -1;

    if (!PyObject_TypeCheck(kern_obj, &KernelType)) {
        mc_err(FirstArgumentNotKernel);
        return -1;
    }

    self->kern_obj = (Kernel*)kern_obj;

    if (mc_err(mc_sw_fn_open(&(self->kern_obj->dev_obj->dev_handle), &(self->kern_obj->kern_handle), func_name, &(self->fn_handle))))
        return -1;

    Py_INCREF(kern_obj); // Cannot close kernel while function open

    return 0;
}

static void
Function_dealloc(Function *self)
{
    if (self->fn_handle.id != 0) {
        mc_sw_fn_close(&(self->kern_obj->dev_obj->dev_handle), &(self->kern_obj->kern_handle), &(self->fn_handle));
        Py_DECREF(self->kern_obj);
    }
    Py_TYPE(self)->tp_free((PyObject *) self);
}

static PyObject *
Function_str(Function* self)
{
    return PyUnicode_FromFormat("metalcompute.Function");
}

static PyTypeObject BufferType; // Forward reference

static PyObject *
Function_call(Function* self, PyObject *args, PyObject *kwargs) {
    Buffer* buf_in;
    Buffer* buf_out;
    int64_t kcount;

    if (!PyArg_ParseTuple(args, "OOL", &buf_in, &buf_out, &kcount)) {
        mc_err(NotReadyToRun);
        return NULL;
    }

    if (((PyObject*)buf_in)->ob_type != &BufferType) {
        mc_err(NotReadyToRun);
        return NULL;
    }

    if (((PyObject*)buf_out)->ob_type != &BufferType) {
        mc_err(NotReadyToRun);
        return NULL;
    }

    if (mc_err(mc_sw_fn_run(
        &(self->kern_obj->dev_obj->dev_handle),
        &(self->kern_obj->kern_handle),
        &(self->fn_handle),
        &(buf_in->buf_handle),
        &(buf_out->buf_handle), 
        kcount))) {
        return NULL;
    }

    Py_RETURN_NONE;
}

static PyTypeObject FunctionType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "metalcompute.Function",
    .tp_doc = "A Metal compute kernel function which can be run",
    .tp_basicsize = sizeof(Function),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
    .tp_init = (initproc) Function_init,
    .tp_dealloc = (destructor) Function_dealloc,
    .tp_str = (reprfunc) Function_str,
    .tp_call = (ternaryfunc) Function_call
};

static int
Buffer_init(Buffer *self, PyObject *args, PyObject *kwds)
{
    PyObject* dev_obj;
    int64_t length;

    if (!PyArg_ParseTuple(args, "OL", &dev_obj, &length))
        return -1;

    if (!PyObject_TypeCheck(dev_obj, &DeviceType)) {
        mc_err(FirstArgumentNotDevice);
        return -1;
    }

    self->dev_obj = (Device*)dev_obj;

    if (mc_err(mc_sw_buf_open(&(self->dev_obj->dev_handle), length, &(self->buf_handle))))
        return -1;

    self->length = length;
    self->exports = 0;

    Py_INCREF(dev_obj); // Cannot close device while buffer open

    return 0;
}

static void
Buffer_dealloc(Buffer *self)
{
    mc_sw_buf_close(&(self->dev_obj->dev_handle), &(self->buf_handle));
    Py_DECREF(self->dev_obj);
    Py_TYPE(self)->tp_free((PyObject *) self);
}

static PyObject *
Buffer_str(Buffer* self)
{
    return PyUnicode_FromFormat("metalcompute.Buffer(length=%lld)",self->length);
}

int Buffer_getbuffer(Buffer *self, Py_buffer *view, int flags) {
    view->buf = self->buf_handle.buf;
    view->obj = (PyObject*)self;
    Py_INCREF(view->obj);
    view->len = self->buf_handle.length;
    view->readonly = false;
    view->itemsize = 1;
    view->format = NULL; // 'B'
    view->ndim = 1;

    self->exports++;

    return 0;
}

int Buffer_releasebuffer(Buffer *self, Py_buffer *view, int flags) {
    self->exports--;
    return 0;
}

static PyBufferProcs BufferProcs = {
    .bf_getbuffer = (getbufferproc) Buffer_getbuffer,
    .bf_releasebuffer = (releasebufferproc) Buffer_releasebuffer
};

static PyTypeObject BufferType = {
    PyVarObject_HEAD_INIT(NULL, 0)
    .tp_name = "metalcompute.Buffer",
    .tp_doc = "A Metal compute buffer",
    .tp_basicsize = sizeof(Buffer),
    .tp_itemsize = 0,
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_new = PyType_GenericNew,
    .tp_init = (initproc) Buffer_init,
    .tp_dealloc = (destructor) Buffer_dealloc,
    .tp_str = (reprfunc) Buffer_str,
    .tp_as_buffer = &BufferProcs,
};

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

void define_device_info_type() {
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
    DeviceInfo = PyStructSequence_NewType(&dev_item_desc);
}

PyMODINIT_FUNC
PyInit_metalcompute(void)
{
    //printf("(creating stdout)\n"); // Uncomment if debugging swift code with print statements

    if (PyType_Ready(&DeviceType) < 0)
        return NULL;

    if (PyType_Ready(&KernelType) < 0)
        return NULL;

    if (PyType_Ready(&FunctionType) < 0)
        return NULL;

    if (PyType_Ready(&BufferType) < 0)
        return NULL;

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

    define_device_info_type();
    
    Py_INCREF(&DeviceType);
    if (PyModule_AddObject(m, "Device", (PyObject *) &DeviceType) < 0) {
        Py_XDECREF(MetalComputeError);
        Py_DECREF(&DeviceType);
        Py_DECREF(m);
        return NULL;
    }

    Py_INCREF(&KernelType);
    if (PyModule_AddObject(m, "Kernel", (PyObject *) &KernelType) < 0) {
        Py_XDECREF(MetalComputeError);
        Py_DECREF(&DeviceType);
        Py_DECREF(&KernelType);
        Py_DECREF(m);
        return NULL;
    }

    Py_INCREF(&FunctionType);
    if (PyModule_AddObject(m, "Function", (PyObject *) &FunctionType) < 0) {
        Py_XDECREF(MetalComputeError);
        Py_DECREF(&DeviceType);
        Py_DECREF(&KernelType);
        Py_DECREF(&FunctionType);
        Py_DECREF(m);
        return NULL;
    }

    Py_INCREF(&BufferType);
    if (PyModule_AddObject(m, "Buffer", (PyObject *) &BufferType) < 0) {
        Py_XDECREF(MetalComputeError);
        Py_DECREF(&DeviceType);
        Py_DECREF(&KernelType);
        Py_DECREF(&FunctionType);
        Py_DECREF(&BufferType);
        Py_DECREF(m);
        return NULL;
    }

    return m;
}





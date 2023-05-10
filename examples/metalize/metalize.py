import inspect
from array import array

import metalcompute as mc

def _metalkernel_decorator(mcdev, *args, **kwargs):
    fn = args[0]
    argnames = inspect.getfullargspec(fn).args
    values = 0
    indent = "    "
    kernel = f"""
#include <metal_stdlib>
using namespace metal;

kernel void {fn.__name__}(
"""
    extend = False
    for index, argname in enumerate(argnames):
        if extend: kernel += ",\n"
        kernel += f"{indent}const device float *{argname} [[ buffer({index}) ]]"
        extend = True

    body = ""
    ops = ["sin","cos","sqrt","log","log2"]
    class arg:
        def __init__(self, name=None):
            nonlocal values
            if name is None:
                value = values
                values += 1
                self.__name__ = f"r{value}"
            else:
                self.__name__ = name
        def __str__(self):
            return self.__name__
        def expr_bin(self,op,arg0,arg1):
            nonlocal body
            ret = arg()
            if type(arg0)==int: arg0=float(arg0)
            if type(arg1)==int: arg1=float(arg1)
            body += f"{indent}float {ret} = {arg0} {op} {arg1};\n"
            return ret
        def expr_un(self,op,arg0):
            nonlocal body
            ret = arg()
            if type(arg0)==int: arg0=float(arg0)
            body += f"{indent}float {ret} = {op}({arg0});\n"
            return ret
        def __add__(self, other):
            return self.expr_bin("+", self, other)
        def __mul__(self, other):
            return self.expr_bin("*", self, other)
        def __sub__(self, other):
            return self.expr_bin("-", self, other)
        def __truediv__(self, other):
            return self.expr_bin("/", self, other)
        def __radd__(self, other):
            return self.expr_bin("+", self, other)
        def __rmul__(self, other):
            return self.expr_bin("*", self, other)
        def __rsub__(self, other):
            return self.expr_bin("-", self, other)
        def __rtruediv__(self, other):
            return self.expr_bin("/", self, other)
        def __gt__(self, other):
            return self.expr_bin(">", self, other)
        def __lt__(self, other):
            return self.expr_bin("<", self, other)
        def __getattr__(self, name):
            nonlocal ops
            if name not in ops: raise Exception(f"Unknown operation '{name}'")
            return self.expr_un(name, self)
    args = [arg(f"{name}[id]") for name in argnames]
    ret = fn(*args)
    
    # Add buffers for return args    
    if type(ret)!=tuple:
        ret = (ret,)
    for index, retarg in enumerate(ret):
        if extend: kernel += ",\n"
        kernel += f"{indent}device float *out{index} [[ buffer({len(argnames)+index}) ]]"
        body += f"{indent}out{index}[id] = {retarg};\n"
    if extend: kernel += ",\n"
    kernel += f"{indent}uint id [[thread_position_in_grid ]]"
    kernel += ") {\n"
    body += "}"
    full_kernel = kernel + body
    #print(full_kernel)
    compiled_kernel = mcdev.kernel(full_kernel)
    compiled_function = compiled_kernel.function(fn.__name__)
    def fn_wrapper(*call_args):
        # Check arg count matches defined
        if len(call_args) != len(args): raise Exception(f"Expected {len(args)} arguments. Received {len(call_args)}")
        # Check all args have len
        count = None
        shape = None
        converted = []
        returns = []
        for call_arg in call_args:
            if hasattr(call_arg,"shape"):
                if shape is None: 
                    shape = call_arg.shape
                    count = call_arg.size
                if call_arg.shape != shape:
                    raise Exception(f"Expected all args to have shape {shape}")
                as_float = call_arg.astype('f')
            else:
                if count is None:
                    count = len(call_arg)
                if len(call_arg) != count:
                    raise Exception(f"Expected all args to have {count} items")
                as_float = array('f',call_arg)
            as_buf = mcdev.buffer(as_float)
            converted.append(as_buf)
        # Allocate return buffers
        for retarg in ret:
            buf = mcdev.buffer(4*count)
            converted.append(buf)
            if shape is None:
                r = memoryview(buf).cast('f')
                returns.append(r)
            else:
                r = memoryview(buf).cast('f',shape)
                returns.append(r)

        # Call function
        compiled_function(count, *converted)

        if len(returns)==1: 
            return returns[0]
        else: return tuple(returns)

    return fn_wrapper

_default_device = None

def get_default_device():
    global _default_device
    if _default_device is None:
        _default_device = mc.Device()
    return _default_device

def metalize(fn):
    return _metalkernel_decorator(get_default_device(), fn)

def metalize_wth_device(device):
    def wrapped(*args):
        return _metalkernel_decorator(device, *args)
    return wrapped 

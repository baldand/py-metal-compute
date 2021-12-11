/*
metalcompute.swift

Swift backend for Python extension to use Metal for compute

(c) Andrew Baldwin 2021
*/

import MetalKit

import mc_c2sw // Bridging C to Swift

let Success:RetCode = 0
let CannotCreateDevice:RetCode = -1
let CannotCreateCommandQueue:RetCode = -2
let NotReadyToCompile:RetCode = -3
let FailedToCompile:RetCode = -4
let FailedToFindFunction:RetCode = -5
let NotReadyToCompute:RetCode = -6
let FailedToMakeInputBuffer:RetCode = -7
let FailedToMakeOutputBuffer:RetCode = -8
let NotReadyToRun:RetCode = -8
let CannotCreateCommandBuffer:RetCode = -9
let CannotCreateCommandEncoder:RetCode = -10
let CannotCreatePipelineState:RetCode = -11
let IncorrectOutputCount:RetCode = -12
let NotReadyToRetrieve:RetCode = -13
let UnsupportedInputFormat:RetCode = -14
let UnsupportedOutputFormat:RetCode = -15

// v2 errors
let DeviceNotFound:RetCode = -1000
let KernelNotFound:RetCode = -1001
let FunctionNotFound:RetCode = -1002
let CouldNotMakeBuffer:RetCode = -1003
let BufferNotFound:RetCode = -1004
let RunNotFound:RetCode = -1005
let DeviceBuffersAllocated:RetCode = -1006

// Buffer formats
let FormatUnknown = -1
let FormatI8 = 0
let FormatU8 = 1
let FormatI16 = 2
let FormatU16 = 3
let FormatI32 = 4
let FormatU32 = 5
let FormatI64 = 6
let FormatU64 = 7
let FormatF16 = 8
let FormatF32 = 9
let FormatF64 = 10


// -------------------------------------------------
// v0.1 of API - simple functions and retained state
//
// Can only be used for single kernel 
// sequential synchronous (non-pipelined) execution
// Data must be copied in/out (copy overhead)

var device:MTLDevice?
var commandQueue:MTLCommandQueue?
var library:MTLLibrary?
var function:MTLFunction?
var inputBuffer:MTLBuffer?
var inputCount:Int = 0;
var inputStride:Int = 0
var outputBuffer:MTLBuffer?
var outputCount:Int = 0
var outputStride:Int = 0
var readyToCompile = false
var readyToCompute = false
var readyToRun = false
var readyToRetrieve = false
var compileError:String = ""

@_cdecl("mc_sw_init") public func mc_sw_init(device_index_i64:Int64) -> RetCode {
    let device_index = Int(device_index_i64)
    let devices = MTLCopyAllDevices()
    guard let defaultDevice = MTLCreateSystemDefaultDevice() else {
        return CannotCreateDevice
    }
    if devices.count == 0 {
        return CannotCreateDevice
    }
    if device_index >= devices.count {
        return CannotCreateDevice
    }
    let newDevice = device_index < 0 ? defaultDevice : devices[device_index] 
    device = newDevice
    guard let newCommandQueue = newDevice.makeCommandQueue() else {
        return CannotCreateCommandQueue 
    } 
    commandQueue = newCommandQueue
    readyToCompile = true
    readyToCompute = false

    return Success
}


@_cdecl("mc_sw_release") public func mc_sw_release() -> RetCode {
    inputBuffer = nil
    outputBuffer = nil
    function = nil
    library = nil
    device = nil
    readyToCompile = false
    readyToCompute = false
    readyToRun = false
    readyToRetrieve = false

    return Success
}

@_cdecl("mc_sw_compile") public func mc_sw_compile(programRaw: UnsafePointer<CChar>, functionNameRaw: UnsafePointer<CChar>) -> RetCode {
    guard readyToCompile else { return NotReadyToCompile }
    guard let lDevice = device else { return NotReadyToCompile }

    // Convert c strings to Swift String
    let program = String(cString:programRaw)
    let functionName = String(cString:functionNameRaw)

    let options = MTLCompileOptions();
    options.fastMathEnabled = true
    options.languageVersion = .version2_3
    do {
        let newLibrary = try lDevice.makeLibrary(source: program, options:options) 
        library = newLibrary
        guard let newFunction = newLibrary.makeFunction(name: functionName) else { return FailedToFindFunction }
        function = newFunction
    } catch {
        compileError = error.localizedDescription
        return FailedToCompile
    }

    readyToCompute = true
    readyToRun = false

    return Success; 
}

func get_stride(_ format: Int) -> Int {
    if (format == FormatF32) {
        return MemoryLayout<Float>.stride;
    } else if (format == FormatU8) {
        return MemoryLayout<UInt8>.stride;
    } else {
        return 0;
    }
}

@_cdecl("mc_sw_alloc") public func mc_sw_alloc(icount: Int, input: UnsafeRawPointer, iformat: Int, ocount: Int, oformat: Int) -> RetCode {
    // Allocate input/output buffers for run
    // Separating this step allows python global lock to be released for the actual run which does not need any python objects
    guard readyToCompute else { return NotReadyToCompute }
    guard let lDevice = device else { return NotReadyToCompute }

    inputStride = get_stride(iformat);
    guard inputStride != 0 else { return UnsupportedInputFormat }

    outputStride = get_stride(oformat);
    guard outputStride != 0 else { return UnsupportedOutputFormat }

    guard let newInputBuffer = lDevice.makeBuffer(bytes: input, length: inputStride * icount, options: .storageModeShared) else { return FailedToMakeInputBuffer }
    guard let newOutputBuffer = lDevice.makeBuffer(length: outputStride * ocount, options: .storageModeShared) else { return FailedToMakeOutputBuffer }

    inputBuffer = newInputBuffer
    outputBuffer = newOutputBuffer
    inputCount = icount
    outputCount = ocount
    readyToRun = true
    readyToRetrieve = false

    return Success
}

@_cdecl("mc_sw_run") public func mc_sw_run(kcount:Int) -> RetCode {
    // Execute the configured compute task, waiting for completion
    guard readyToRun else { return NotReadyToRun }
    guard let lDevice = device else { return NotReadyToRun }
    guard let lFunction = function else { return NotReadyToRun }
    guard let lCommandQueue = commandQueue else { return NotReadyToRun }
    guard let lInputBuffer = inputBuffer else { return NotReadyToRun }
    guard let lOutputBuffer = outputBuffer else { return NotReadyToRun }
    guard let commandBuffer = lCommandQueue.makeCommandBuffer() else { return CannotCreateCommandBuffer }
    guard let encoder = commandBuffer.makeComputeCommandEncoder() else { return CannotCreateCommandEncoder }

    do {
        let pipelineState = try lDevice.makeComputePipelineState(function:lFunction)
        encoder.setComputePipelineState(pipelineState);
        encoder.setBuffer(lInputBuffer, offset: 0, index: 0)
        encoder.setBuffer(lOutputBuffer, offset: 0, index: 1)
        let w = pipelineState.threadExecutionWidth
        let h = pipelineState.maxTotalThreadsPerThreadgroup / w
        let numThreadgroups = MTLSize(width: (kcount+(w*h-1))/(w*h), height: 1, depth: 1)
        let threadsPerThreadgroup = MTLSize(width: w*h, height: 1, depth: 1)
        //print(numThreadgroups, threadsPerThreadgroup)
        encoder.dispatchThreadgroups(numThreadgroups, threadsPerThreadgroup: threadsPerThreadgroup)
        encoder.endEncoding()
        commandBuffer.commit()
        commandBuffer.waitUntilCompleted()

    } catch {
        return CannotCreatePipelineState
    }

    readyToRetrieve = true

    return Success
}

@_cdecl("mc_sw_retrieve") public func mc_sw_retrieve(ocount:Int, output: UnsafeMutableRawPointer) -> RetCode {
    // Return result of compute task
    guard readyToRetrieve else { return NotReadyToRetrieve }
    guard ocount == outputCount else { return IncorrectOutputCount }
    guard let lOutputBuffer = outputBuffer else { return NotReadyToRetrieve }

    output.copyMemory(from: lOutputBuffer.contents(), byteCount: outputCount * outputStride)

    return Success
}

@_cdecl("mc_sw_get_compile_error") public func mc_sw_get_compile_error() -> UnsafeMutablePointer<CChar> {
    return strdup(compileError)
}

// ------------------------------
// v0.2 of the API - object based
//
// - Multiple devices can be opened
// - Buffers are allocated and filled/emptied externally
// - Multiple kernel objects are possible
// - Kernels can be pipelined to devices
// - Callbacks when kernels are completed and data is available

class mc_sw_buf {
    let buf:MTLBuffer
    init(_ buf:MTLBuffer) {
        self.buf = buf
    }
}

class mc_sw_fn {
    let fn:MTLFunction
    init(_ fn:MTLFunction) {
        self.fn = fn
    }
}

class mc_sw_kern {
    let lib:MTLLibrary
    var fns:[Int64:mc_sw_fn] = [:]
    init(_ lib:MTLLibrary) {
        self.lib = lib
    }
}

class mc_sw_dev {
    let dev:MTLDevice
    let queue:MTLCommandQueue
    var kerns:[Int64:mc_sw_kern] = [:]
    var bufs:[Int64:mc_sw_buf] = [:]
    init(_ dev:MTLDevice, _ queue:MTLCommandQueue) {
        self.dev = dev
        self.queue = queue
    }
}

class mc_sw_cb {
    let dev_id:Int64
    let cb:MTLCommandBuffer
    var running = true
    var released = false
    init(_ dev_id:Int64, _ cb:MTLCommandBuffer) {
        self.dev_id = dev_id
        self.cb = cb
    }
}

// Index of next object
var mc_next_index:Int64 = 4242 
var mc_devs:[Int64:mc_sw_dev] = [:]
var mc_cbs:[Int64:mc_sw_cb] = [:]

@_cdecl("mc_sw_count_devs") public func mc_sw_get_devices(devices: UnsafeMutablePointer<mc_devices>) -> RetCode {
    let metal_devices = MTLCopyAllDevices()

    devices[0].dev_count = Int64(metal_devices.count)
    let dev_array = UnsafeMutablePointer<mc_dev>.allocate(capacity: metal_devices.count) // Must be freed by python  
    devices[0].devs = dev_array 
    for dev_index in 0..<metal_devices.count {
        let dev = metal_devices[dev_index]
        dev_array[dev_index].recommendedMaxWorkingSetSize = Int64(dev.recommendedMaxWorkingSetSize)
        dev_array[dev_index].maxTransferRate = Int64(dev.maxTransferRate)
        dev_array[dev_index].hasUnifiedMemory = Bool(dev.hasUnifiedMemory)
        dev_array[dev_index].name = strdup(dev.name) // Copy - must be released by python side
    }

    return Success
}

@_cdecl("mc_sw_dev_open") public func mc_sw_dev_open(
        device_index_i64:Int64, 
        dev_handle: UnsafeMutablePointer<mc_dev_handle>) -> RetCode {
    let device_index = Int(device_index_i64)
    let devices = MTLCopyAllDevices()
    guard let defaultDevice = MTLCreateSystemDefaultDevice() else {
        return CannotCreateDevice
    }
    if devices.count == 0 {
        return CannotCreateDevice
    }
    if device_index >= devices.count {
        return CannotCreateDevice
    }
    let newDevice = device_index < 0 ? defaultDevice : devices[device_index] 
    guard let newCommandQueue = newDevice.makeCommandQueue() else {
        return CannotCreateCommandQueue 
    } 

    // Return device object
    let dev_obj = mc_sw_dev(newDevice, newCommandQueue)
    let id = mc_next_index
    mc_next_index += 1
    mc_devs[id] = dev_obj // Store the dev
    dev_handle[0].id = id // Return id of dev
    dev_handle[0].name = strdup(newDevice.name) // Python must free this later

    return Success
}

@_cdecl("mc_sw_dev_close") public func mc_sw_dev_close(handle: UnsafeMutablePointer<mc_dev_handle>) -> RetCode {
    guard let sw_dev = mc_devs[handle[0].id] else { return DeviceNotFound }
    guard sw_dev.bufs.count == 0 else { return DeviceBuffersAllocated }
    mc_devs.removeValue(forKey: handle[0].id)
    return Success
}

@_cdecl("mc_sw_kern_open") public func mc_sw_kern_open(
        dev_handle: UnsafePointer<mc_dev_handle>, 
        program_raw: UnsafePointer<CChar>, 
        kern_handle: UnsafeMutablePointer<mc_kern_handle>) -> RetCode {
    guard let sw_dev = mc_devs[dev_handle[0].id] else { return DeviceNotFound }

    // Convert c strings to Swift String
    let program = String(cString:program_raw)

    let options = MTLCompileOptions();
    options.fastMathEnabled = true
    options.languageVersion = .version2_3

    do {
        let newLibrary = try sw_dev.dev.makeLibrary(source: program, options:options) 
        let kern = mc_sw_kern(newLibrary)
        let id = mc_next_index
        mc_next_index += 1
        sw_dev.kerns[id] = kern
        kern_handle[0].id = id 
    } catch {
        compileError = error.localizedDescription
        return FailedToCompile
    }

    return Success; 
}

@_cdecl("mc_sw_kern_close") public func mc_sw_kern_close(
        dev_handle: UnsafePointer<mc_dev_handle>,
        kern_handle: UnsafeMutablePointer<mc_kern_handle>) -> RetCode {
    guard let sw_dev = mc_devs[dev_handle[0].id] else { return DeviceNotFound }
    guard sw_dev.kerns.removeValue(forKey: kern_handle[0].id) != nil else {
        return KernelNotFound
    }
    return Success
}

@_cdecl("mc_sw_fn_open") public func mc_sw_fn_open(
        dev_handle: UnsafePointer<mc_dev_handle>, 
        kern_handle: UnsafePointer<mc_kern_handle>,
        func_name_raw: UnsafePointer<CChar>, 
        fn_handle: UnsafeMutablePointer<mc_fn_handle>
        ) -> RetCode {
    guard let sw_dev = mc_devs[dev_handle[0].id] else { return DeviceNotFound }
    guard let sw_kern = sw_dev.kerns[kern_handle[0].id] else { return KernelNotFound }

    // Convert c string to Swift String
    let func_name = String(cString:func_name_raw)

    guard let newFunction = sw_kern.lib.makeFunction(name: func_name) else { return FunctionNotFound }

    let fn = mc_sw_fn(newFunction)
    let id = mc_next_index
    mc_next_index += 1
    sw_kern.fns[id] = fn
    fn_handle[0].id = id 

    return Success; 
}

@_cdecl("mc_sw_fn_close") public func mc_sw_fn_close(
        dev_handle: UnsafePointer<mc_dev_handle>,
        kern_handle: UnsafePointer<mc_kern_handle>,
        fn_handle: UnsafeMutablePointer<mc_fn_handle>) -> RetCode {
    guard let sw_dev = mc_devs[dev_handle[0].id] else { return DeviceNotFound }
    guard let sw_kern = sw_dev.kerns[kern_handle[0].id] else { return KernelNotFound }
    guard sw_kern.fns.removeValue(forKey: fn_handle[0].id) != nil else {
        return FunctionNotFound
    }
    return Success
}

@_cdecl("mc_sw_buf_open") public func mc_sw_buf_open(
        dev_handle: UnsafePointer<mc_dev_handle>, 
        length:Int64,
        src_opt: UnsafeRawPointer?,
        buf_handle: UnsafeMutablePointer<mc_buf_handle>) -> RetCode {
    guard let sw_dev = mc_devs[dev_handle[0].id] else { return DeviceNotFound }
    var newBuffer:MTLBuffer
    if let src = src_opt {
        guard let copyBuffer = sw_dev.dev.makeBuffer(bytes: src, length: Int(length), options: .storageModeShared) else {
            return CouldNotMakeBuffer
        }
        newBuffer = copyBuffer 
    } else {
        guard let zeroBuffer = sw_dev.dev.makeBuffer(length: Int(length), options: .storageModeShared) else {
            return CouldNotMakeBuffer
        }
        newBuffer = zeroBuffer 
    }

    let buf = mc_sw_buf(newBuffer)
    let id = mc_next_index
    mc_next_index += 1
    sw_dev.bufs[id] = buf
    buf_handle[0].id = id
    buf_handle[0].buf = newBuffer.contents().bindMemory(to: CChar.self, capacity: Int(length))
    buf_handle[0].length = length

    return Success; 
}

@_cdecl("mc_sw_buf_close") public func mc_sw_buf_close(
        dev_handle: UnsafePointer<mc_dev_handle>,
        buf_handle: UnsafeMutablePointer<mc_buf_handle>) -> RetCode {
    guard let sw_dev = mc_devs[dev_handle[0].id] else { return DeviceNotFound }
    guard sw_dev.bufs.removeValue(forKey: buf_handle[0].id) != nil else {
        return BufferNotFound
    }
    
    return Success
}

@_cdecl("mc_sw_run_open") public func mc_sw_run_open(
        dev_handle: UnsafePointer<mc_dev_handle>, 
        kern_handle: UnsafePointer<mc_kern_handle>, 
        fn_handle: UnsafePointer<mc_fn_handle>, 
        run_handle: UnsafeMutablePointer<mc_run_handle>) -> RetCode {
    guard let sw_dev = mc_devs[dev_handle[0].id] else { return DeviceNotFound }
    guard let sw_kern = sw_dev.kerns[kern_handle[0].id] else { return KernelNotFound }
    guard let sw_fn = sw_kern.fns[fn_handle[0].id] else { return FunctionNotFound }
    guard let commandBuffer = sw_dev.queue.makeCommandBuffer() else { return CannotCreateCommandBuffer }
    guard let encoder = commandBuffer.makeComputeCommandEncoder() else { return CannotCreateCommandEncoder }

    do {
        let pipelineState = try sw_dev.dev.makeComputePipelineState(function:sw_fn.fn)
        encoder.setComputePipelineState(pipelineState);

        for index in 0..<Int(run_handle[0].buf_count) {
            guard let buf_index = run_handle[0].bufs[index] else { return BufferNotFound }
            guard let sw_buf = sw_dev.bufs[buf_index[0].id] else { return BufferNotFound }
            encoder.setBuffer(sw_buf.buf, offset: 0, index: index)
        }

        let w = pipelineState.threadExecutionWidth
        let h = pipelineState.maxTotalThreadsPerThreadgroup / w
        let kcount = run_handle[0].kcount
        let numThreadgroups = MTLSize(width: (Int(kcount)+(w*h-1))/(w*h), height: 1, depth: 1)
        let threadsPerThreadgroup = MTLSize(width: w*h, height: 1, depth: 1)
        encoder.dispatchThreadgroups(numThreadgroups, threadsPerThreadgroup: threadsPerThreadgroup)
        encoder.endEncoding()

        let run = mc_sw_cb(dev_handle[0].id, commandBuffer)
        let id = mc_next_index
        mc_next_index += 1
        mc_cbs[id] = run
        run_handle[0].id = id

        // Completion handler - will run later
        commandBuffer.addCompletedHandler { cb in
            for (cb_id, sw_cb) in mc_cbs {
                if sw_cb.cb === cb {
                    sw_cb.running = false
                    // Could call back to python here...
                    if sw_cb.released {
                        mc_cbs.removeValue(forKey:cb_id)
                    }
                    return
                }
            }
        }

        commandBuffer.commit()

    } catch {
        return CannotCreatePipelineState
    }

    return Success
}

@_cdecl("mc_sw_run_close") public func mc_sw_run_close(
        run_handle: UnsafePointer<mc_run_handle>) -> RetCode {
    guard let sw_run = mc_cbs[run_handle[0].id] else {
        return RunNotFound
    }
    if sw_run.running {
        // Block until completion
        sw_run.cb.waitUntilCompleted()
    }
    return Success
}



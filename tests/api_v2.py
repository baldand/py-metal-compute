import metalcompute as mc

devs = mc.get_devices()

# Create default device
mc.init()
mc.release()

for device_index in range(len(devs)):
    mc.init(device_index)
    mc.release()

try:
    mc.init(len(devs))
    assert(0) # Fail!
except:
    pass # Ok!


print(devs)

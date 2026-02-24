import usb.core
import usb.util

# find our device
dev = usb.core.find(idVendor=0x1D50, idProduct=0xDEAD)

# was it found?
if dev is None:
    raise ValueError('Device not found')

# get an endpoint instance
cfg = dev.get_active_configuration()
intf = cfg[(0, 0)]

outep = usb.util.find_descriptor(
    intf,
    # match the first OUT endpoint
    custom_match= \
        lambda e: \
            usb.util.endpoint_direction(e.bEndpointAddress) == \
            usb.util.ENDPOINT_OUT)

inep = usb.util.find_descriptor(
    intf,
    # match the first IN endpoint
    custom_match= \
        lambda e: \
            usb.util.endpoint_direction(e.bEndpointAddress) == \
            usb.util.ENDPOINT_IN)

assert inep is not None
assert outep is not None

test_string = "Hello World!"
for i in range(1):
    outep.write(test_string)
    from_device = inep.read(len(test_string))
    print("Sent: {}, Received: {}".format(test_string, ''.join([chr(x) for x in from_device])))

# print("Device Says: {}".format(''.join([chr(x) for x in from_device])))

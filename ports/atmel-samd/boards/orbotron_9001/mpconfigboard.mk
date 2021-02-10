LD_FILE = boards/samd21x18-bootloader.ld
USB_VID = 0x1d50
USB_PID = 0x6067
USB_PRODUCT = "Orbotron 9001"
USB_MANUFACTURER = "Thingotron Labs"
USB_DEVICES = "CDC,MSC,HID"
USB_HID_DEVICES = "KEYBOARD,MOUSE,ORBOTRON"
CHIP_VARIANT = SAMD21G18A
CHIP_FAMILY = samd21

INTERNAL_FLASH_FILESYSTEM = 1
LONGINT_IMPL = NONE
CIRCUITPY_SMALL_BUILD = 1

# now turn off features in the default small build
CIRCUITPY_ANALOGIO = 0
CIRCUITPY_DISPLAYIO = 0
CIRCUITPY_NEOPIXEL_WRITE = 0
CIRCUITPY_MATH = 0
# CIRCUITPY_OS = 0
CIRCUITPY_PULSEIO = 0
CIRCUITPY_RANDOM = 0
CIRCUITPY_ROTARYIO = 0
CIRCUITPY_STRUCT = 0
CIRCUITPY_TOUCHIO = 0
CIRCUITPY_USB_MIDI = 0

SUPEROPT_GC = 0
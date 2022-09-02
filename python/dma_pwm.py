
# flake8: noqa
from ctypes import *
import logging
LIBDMAPWM_LOCATION = '/usr/local/lib/libdmapwm.so'

DEFAULT_PAGES = 16  # Number of pages for each control


class pulseWidth:
    DEFAULT_PULSE_WIDTH = 5   # Default PWM pulse width in us
    MOTOR_PULSE_WIDTH = 0.4   # Motor PWM pulse width in us
    SERVO_PULSE_WIDTH = 50    # Servo PWM pulse width in us
    LED_PULSE_WIDTH = 5000  # LED PWM pulse width in us


# Error numbers:
'''
 ECHNLREQ    = 1  # At least one channel has been requested
 EINVPW      = 2  # Invalid pulse width
 ENOFREECHNL = 3  # No free DMA channels available to be requested
 EINVCHNL    = 4  # Invalid or non-requested channel
 EINVDUTY    = 5  # Invalid duty cycle
 EINVGPIO    = 6  # Invalid GPIO pin
 EFREQNOTMET = 7  # Desired frequency cannot be met
 EPWMNOTSET  = 8  # PWM signal on requested channel has not been set
 ENOPIVER    = 9  # Could not get PI board revision
 EMAPFAIL    = 10 # Peripheral memory mapping failed
 ESIGHDNFAIL = 11 # Signal handler failed to setup 
'''

errorDict = {
 0: False,
 -1: Exception("ECHNLREQ # At least one channel has been requested"),
 -2: Exception("EINVPW  # Invalid pulse width"),
 -3: Exception("ENOFREECHNL # No free DMA channels available to be requested"),
 -4: Exception("EINVCHNL  # Invalid or non-requested channel"),
 -5: Exception("EINVDUTY # Invalid duty cycle"),
 -6: Exception("EINVGPIO # Invalid GPIO pin"),
 -7: Exception("EFREQNOTMET # Desired frequency cannot be met"),
 -8: Exception("EPWMNOTSET # PWM signal on requested channel has not been set"),
 -9: Exception("ENOPIVER # Could not get PI board revision"),
 -10: Exception("EMAPFAIL # Peripheral memory mapping failed"),
 -11: Exception("ESIGHDNFAIL # Signal handler failed to setup ")
}


class DMAPWM:
    channelsInUse = []

    def __init__(self):
        self.myChannel: int = None
        try:
            self._dma_pwm = cdll.LoadLibrary(LIBDMAPWM_LOCATION)
        except Exception as e:
            raise Exception("Could not find DWMAPWM shared libarary object. Did you remember to install DMAPWM?")
        self._dma_pwm.config_pwm.restype = c_float
        self._dma_pwm.config_pwm.argtypes = [c_int, c_float]
        self._dma_pwm.set_pwm.argtypes = [c_int, POINTER(c_int), c_size_t, c_float, c_float]
        self._dma_pwm.get_duty_cycle_pwm.restype = c_float
        self._dma_pwm.get_freq_pwm.restype = c_float
        self._dma_pwm.get_pulse_width.restype = c_float

    def _cleanup(self):
        if self.myChannel is not None:
            try:
                self.disable_pwm(self.myChannel)
            except:
                pass
            try:
                self.free_pwm(self.myChannel)
            except:
                return
            self.channelsInUse.remove(self.myChannel)

    def config_pwm(self, pages: int, pulseWidth: float):
        response = self._dma_pwm.config_pwm(pages, pulseWidth)
        if(errorDict[response]):
            raise errorDict[response]

    def request_pwm(self) -> int:
        response = self._dma_pwm.request_pwm()
        if(errorDict[response]):
            raise errorDict[response]
        self.channelsInUse.append(response)
        self.myChannel = response
        return response

    def set_pwm(self, channel: int, gpioPins: list, freq: float, duty: float):
        cPinsArray = (c_int * len(gpioPins))(*gpioPins)
        cPinsPointer = cast(cPinsArray, POINTER(c_int))
        response = self._dma_pwm.set_pwm(channel, cPinsPointer, len(gpioPins), freq, duty)
        if(errorDict[response]):
            raise errorDict[response]

    def enable_pwm(self, channel: int):
        response = self._dma_pwm.enable_pwm(channel)
        if(errorDict[response]):
            raise errorDict[response]

    def disable_pwm(self, channel: int):
        response = self._dma_pwm.disable_pwm(channel)
        if response < 0:
            raise Exception("ERR_INVALID_CHANNEL: Invalid or Non-requested channel")

    def free_pwm(self, channel: int):
        response = self._dma_pwm.free_pwm(channel)
        if response < 0:
            raise Exception("ERR_INVALID_CHANNEL: Invalid or Non-requested channel")

    def get_duty_cycle_pwm(self, channel):
        response = self._dma_pwm.get_duty_cycle_pwm(channel)
        if response < 0.0:
            raise Exception("ERR_INVALID_CHANNEL: Invalid or Non-requested channel")

    def get_freq_pwm(self, channel):
        response = self._dma_pwm.get_freq_pwm(channel)
        if response < 0.0:
            raise Exception("ERR_INVALID_CHANNEL: Invalid or Non-requested channel")

    def get_pulse_width(self) -> float:
        return self._dma_pwm.get_pulse_width()

    def __enter__(self):
        return self

    def __exit__(self, *a):
        self._cleanup()

    def __del__(self):
        self._cleanup()


if __name__ == "__main__":
    print("That doesn't do anything yet, sorry!")

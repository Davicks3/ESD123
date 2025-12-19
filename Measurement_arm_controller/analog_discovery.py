import ctypes                     # import the C compatible data types
from sys import platform, path    # this is needed to check the OS type and get the PATH
from os import sep                # OS specific file path separators
import time
 
# load the dynamic library, get constants path (the path is OS specific)
if platform.startswith("win"):
    # on Windows
    dwf = ctypes.cdll.dwf
    constants_path = "C:" + sep + "Program Files (x86)" + sep + "Digilent" + sep + "WaveFormsSDK" + sep + "samples" + sep + "py"
elif platform.startswith("darwin"):
    # on macOS
    lib_path = sep + "Library" + sep + "Frameworks" + sep + "dwf.framework" + sep + "dwf"
    dwf = ctypes.cdll.LoadLibrary(lib_path)
    constants_path = sep + "Applications" + sep + "WaveForms.app" + sep + "Contents" + sep + "Resources" + sep + "SDK" + sep + "samples" + sep + "py"
else:
    # on Linux
    dwf = ctypes.cdll.LoadLibrary("libdwf.so")
    constants_path = sep + "usr" + sep + "share" + sep + "digilent" + sep + "waveforms" + sep + "samples" + sep + "py"
 
# import constants
path.append(constants_path)
import dwfconstants as constants





class AnalogDiscovery:
    """
        stores the device handle and the device name
    """
    def __init__(self):
        self.handle = ctypes.c_int(0)
        self.sampling_frequency = 20e06
        self.buffer_size = 8192
    
    def _sdk_last_error(self):
        """Return last SDK error string (uses FDwfGetLastErrorMsg)."""
        try:
            buf = ctypes.create_string_buffer(512)
            dwf.FDwfGetLastErrorMsg(buf)
            return buf.value.decode("utf-8", errors="replace")
        except Exception as e:
            return f"<failed to get SDK error: {e}>"
    
    def open(self):
        """Open the first available device and validate the handle."""
        device_handle = ctypes.c_int()
        dwf.FDwfDeviceCloseAll()
        rc = dwf.FDwfDeviceOpen(ctypes.c_int(-1), ctypes.byref(device_handle))
        # rc == 0 -> failure
        if not bool(rc):
            raise RuntimeError(f"FDwfDeviceOpen failed (rc={rc}). SDK error: {self._sdk_last_error()}")
        # store handle explicitly as a ctypes.c_int with the integer value
        self.handle = ctypes.c_int(int(device_handle.value))
        print("FDwfDeviceOpen succeeded, handle.value =", self.handle.value)
    
    
    def init_input(self, offset=0, amplitude_range=5):
        """
            initialize the oscilloscope
            parameters: - device data
                        - sampling frequency in Hz, default is 20MHz
                        - buffer size, default is 8192
                        - offset voltage in Volts, default is 0V
                        - amplitude range in Volts, default is ±5V
        """
        # enable all channels
        dwf.FDwfAnalogInChannelEnableSet(self.handle, ctypes.c_int(0), ctypes.c_bool(True))
        dwf.FDwfAnalogInChannelEnableSet(self.handle, ctypes.c_int(1), ctypes.c_bool(True))
    
        # set offset voltage (in Volts)
        dwf.FDwfAnalogInChannelOffsetSet(self.handle, ctypes.c_int(0), ctypes.c_double(offset))
    
        # set range (maximum signal amplitude in Volts)
        dwf.FDwfAnalogInChannelRangeSet(self.handle, ctypes.c_int(0), ctypes.c_double(amplitude_range))
    
        # set the buffer size (data point in a recording)
        dwf.FDwfAnalogInBufferSizeSet(self.handle, ctypes.c_int(self.buffer_size))
    
        # set the acquisition frequency (in Hz)
        dwf.FDwfAnalogInFrequencySet(self.handle, ctypes.c_double(self.sampling_frequency))
    
        # disable averaging (for more info check the documentation)
        dwf.FDwfAnalogInChannelFilterSet(self.handle, ctypes.c_int(-1), constants.filterDecimate)

    def record(self, channel):
        """
        PC-trigger-only record. Requires device.open() and device.init(...) called first.
        channel: 1-based channel index (1 or 2).
        """
        def _const_to_int(x):
            if isinstance(x, int):
                return x
            if hasattr(x, "value"):
                return int(x.value)
            if isinstance(x, (bytes, bytearray)):
                return int.from_bytes(x, byteorder="little", signed=False)
            return int(x)

        # (1) set acquisition mode to RECORD (if available)
        if hasattr(constants, "acqmodeSingle"):
            acqmode = _const_to_int(constants.acqmodeSingle)
        elif hasattr(constants, "DwfAcquisitionModeRecord"):
            acqmode = _const_to_int(constants.DwfAcquisitionModeRecord)
        else:
            # fallback common value for record mode
            acqmode = 0
        print("Acq mode:", acqmode)
        dwf.FDwfAnalogInAcquisitionModeSet(self.handle, ctypes.c_int(acqmode))

        # (2) set trigger source to PC
        if hasattr(constants, "trigsrcPC"):
            trigsrc_val = _const_to_int(constants.trigsrcPC)
        elif hasattr(constants, "trigsrcPc"):
            trigsrc_val = _const_to_int(constants.trigsrcPc)
        else:
            trigsrc_val = 1  # fallback; adjust if your constants differ
        dwf.FDwfAnalogInTriggerSourceSet(self.handle, ctypes.c_ubyte(trigsrc_val))

        # optional: trigger level and slope (kept minimal)
        try:
            dwf.FDwfAnalogInTriggerLevelSet(self.handle, ctypes.c_double(0.0))
        except Exception:
            pass
        try:
            if hasattr(constants, "DwfTriggerSlopeRise"):
                slope = _const_to_int(constants.DwfTriggerSlopeRise)
            else:
                slope = 0
            dwf.FDwfAnalogInTriggerSlopeSet(self.handle, ctypes.c_int(slope))
        except Exception:
            pass

        # small safety auto-timeout while debugging (0.0 = wait forever)
        dwf.FDwfAnalogInTriggerAutoTimeoutSet(self.handle, ctypes.c_double(0.2))

        # (3) Configure in wait mode: start=False, wait=True -> device arms and waits for trigger
        rc = dwf.FDwfAnalogInConfigure(self.handle, ctypes.c_bool(False), ctypes.c_bool(True))
        if not bool(rc):
            # get SDK error text
            buf = ctypes.create_string_buffer(512)
            try:
                dwf.FDwfGetLastErrorMsg(buf)
                err = buf.value.decode('utf-8', errors='replace')
            except Exception:
                err = "<FDwfGetLastErrorMsg failed>"
            raise RuntimeError(f"FDwfAnalogInConfigure failed (rc={rc}). SDK error: {err}")

        # (4) Fire PC trigger to start acquisition
        rc_trigger = dwf.FDwfDeviceTriggerPC(self.handle)
        # rc_trigger may be ctypes; we don't strictly require checking it, but we do print on failure
        if not bool(rc_trigger):
            buf = ctypes.create_string_buffer(512)
            try:
                dwf.FDwfGetLastErrorMsg(buf)
                err = buf.value.decode('utf-8', errors='replace')
            except Exception:
                err = "<FDwfGetLastErrorMsg failed>"
            raise RuntimeError(f"FDwfDeviceTriggerPC failed (rc={rc_trigger}). SDK error: {err}")

        # (5) Poll until Done
        status = ctypes.c_int(0)
        done_int = _const_to_int(getattr(constants, "DwfStateDone", getattr(constants, "dwfStateDone", 2)))

        start_t = time.time()
        max_wait_seconds = 10.0
        while True:
            dwf.FDwfAnalogInStatus(self.handle, ctypes.c_bool(True), ctypes.byref(status))
            # debug: uncomment to see status progression
            # print("status:", status.value)
            if status.value == done_int:
                break
            if time.time() - start_t > max_wait_seconds:
                # try a reset for cleanup and raise
                try:
                    dwf.FDwfAnalogInReset(self.handle)
                except Exception:
                    pass
                raise TimeoutError(f"Acquisition timeout after {max_wait_seconds:.1f}s, status={status.value}")
            time.sleep(0.001)

        # read buffer into ctypes array and convert to python lists
        buf = (ctypes.c_double * self.buffer_size)()
        dwf.FDwfAnalogInStatusData(self.handle, ctypes.c_int(channel - 1), buf, ctypes.c_int(self.buffer_size))
        times = [i / self.sampling_frequency for i in range(self.buffer_size)]
        data = [float(x) for x in buf]
        return data, times
    
    def reset_input(self):
        """
            reset the scope
        """
        dwf.FDwfAnalogInReset(self.handle)
        
    def generate(self, channel, offset, frequency=1e03, amplitude=1, symmetry=50, wait=0, run_time=0, repeat=0, data=[]):
        """
            generate an analog signal
            parameters: - device data
                        - the selected wavegen channel (1-2)
                        - function - possible: custom, sine, square, triangle, noise, ds, pulse, trapezium, sine_power, ramp_up, ramp_down
                        - offset voltage in Volts
                        - frequency in Hz, default is 1KHz
                        - amplitude in Volts, default is 1V
                        - signal symmetry in percentage, default is 50%
                        - wait time in seconds, default is 0s
                        - run time in seconds, default is infinite (0)
                        - repeat count, default is infinite (0)
                        - data - list of voltages, used only if function=custom, default is empty
        """
        # enable channel
        channel = ctypes.c_int(channel - 1)
        dwf.FDwfAnalogOutNodeEnableSet(self.handle, channel, constants.AnalogOutNodeCarrier, ctypes.c_bool(True))
    
        # set function type
        dwf.FDwfAnalogOutNodeFunctionSet(self.handle, channel, constants.AnalogOutNodeCarrier, constants.funcSine)
    
        # set frequency
        dwf.FDwfAnalogOutNodeFrequencySet(self.handle, channel, constants.AnalogOutNodeCarrier, ctypes.c_double(frequency))
    
        # set amplitude or DC voltage
        dwf.FDwfAnalogOutNodeAmplitudeSet(self.handle, channel, constants.AnalogOutNodeCarrier, ctypes.c_double(amplitude))
    
        # set offset
        dwf.FDwfAnalogOutNodeOffsetSet(self.handle, channel, constants.AnalogOutNodeCarrier, ctypes.c_double(offset))
    
        # set symmetry
        dwf.FDwfAnalogOutNodeSymmetrySet(self.handle, channel, constants.AnalogOutNodeCarrier, ctypes.c_double(symmetry))
    
        # set running time limit
        dwf.FDwfAnalogOutRunSet(self.handle, channel, ctypes.c_double(run_time))
    
        # set wait time before start
        dwf.FDwfAnalogOutWaitSet(self.handle, channel, ctypes.c_double(wait))
    
        # set number of repeating cycles
        dwf.FDwfAnalogOutRepeatSet(self.handle, channel, ctypes.c_int(repeat))
    
        # start
        dwf.FDwfAnalogOutConfigure(self.handle, channel, ctypes.c_bool(True))
    
    def full_reset(self):
        dwf.FDwfDeviceReset(self.handle)
    
    def close(self):
        dwf.FDwfDeviceClose(self.handle)
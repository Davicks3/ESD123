import serial # pyserial
import time


class MotionSystem:
    def __init__(self):
        self.axies = {"P":0, "Y":1}
        self.abs_pos = [None, None]
        
        self.offsets = [0 for _ in self.axies.keys()]
        
        self.axies_range = [[] for _ in self.axies.keys()]
        self.axies_range[self.axies["P"]] = [-81, 106]
        self.axies_range[self.axies["Y"]] = [-90, 90]
        
        self.axies_timeout = [160, 20]
        self.pos_timeout = [sum(abs(n) for n in self.axies_range[self.axies[axis]]) / self.axies_timeout[self.axies[axis]] for axis in self.axies.keys()]
        
        self.ser = serial.Serial(
            port="/dev/tty.usbserial-210",
            baudrate=115200,
            timeout=1
        )
        time.sleep(2)
    
    def move_to(self, axis, pos):
        # guard statements to ensure command is valid.
        if axis not in self.axies.keys():
            return False
        if self.abs_pos[self.axies[axis]] is None:
            raise ValueError("Axis not homed...")
        new_pos = pos + self.offsets[self.axies[axis]]
        if new_pos < self.axies_range[self.axies[axis]][0] or new_pos > self.axies_range[self.axies[axis]][1]:
            return False

        timeout = abs(self.abs_pos[self.axies[axis]] - new_pos) * self.pos_timeout[self.axies[axis]]
        self._send_command(f"_MT_{axis}_{pos}_", timeout=timeout)
        self.abs_pos[self.axies[axis]] = pos
        return True
    
    def move_by(self, axis, pos):
        # guard statements to ensure command is valid.
        if axis not in self.axies.keys():
            return False
        if self.abs_pos[self.axies[axis]] is None:
            raise ValueError("Axis not homed...")
        new_pos = self.abs_pos[self.axies[axis]] + pos
        if new_pos < self.axies_range[self.axies[axis]][0] or new_pos > self.axies_range[self.axies[axis]][1]:
            return False
        
        timeout = abs(self.abs_pos[self.axies[axis]] - new_pos) * self.pos_timeout[self.axies[axis]]
        self._send_command(f"_MB_{axis}_{pos}_", timeout=timeout)
        self.abs_pos[self.axies[axis]] += pos
        return True
    
    def home(self):
        self._send_command(f"_HOME_")
        self.abs_pos = [0 for _ in self.abs_pos]
        for axis in self.axies.keys():
            if self.offsets[axis]:
                self.move_to(axis, 0)
        return True
    
    def _send_command(self, command, timeout=None):
        if not command.endswith('\n'):
            command += '\n'
        t0 = time.now()
        res = False
        count = 0
        while not res:
            if count:
                time.sleep(min(pow(2, count)/10.0, 10.0))
            self.ser.write(command)
            res = self._handle_ack("_ACK")
            count += 1
            if timeout is not None and time.now() - t0 >= timeout:
                raise ValueError(f"Timeout on command: {command}!")
        
        count = 0
        while not self._handle_ack("_ACKOK"):
            if timeout is not None and time.now() - t0 >= timeout:
                raise ValueError(f"Timeout on command: {command}!")
            time.sleep(0.5)
        
    
    def _handle_ack(self, expected="_ACKOK\n"):
        if not expected.endswith('\n'):
            expected += '\n'
        response = self.ser.read(self.ser.in_waiting)
        if self._check_for_ack(response, expected):
            return True
        
        response = self.ser.readline()
        if self._check_for_ack(response):
            return True
        
        return False
        
    def _check_for_ack(self, response, expected):
        if expected in response:
            return True
        return False
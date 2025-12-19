from motion_system import MotionSystem
import csv


class MeasurementArm:
    def __init__(self, data_file_path):
        self.file_path = data_file_path
        self.motion_system = MotionSystem()
        self.measurement_func = None
        self.pitch_range = None
        self.yaw_range = None
        self.steps = None
        self.repeat_n = 1
        self.meas_data = None
    
    def set_grid_steps(self, steps):
        if self.pitch_range is not None:
            assert self.pitch_range * 2 % steps >= 1e-2, "Pitch range not divisable by steps."
        if self.yaw_range is not None:
            assert self.yaw_range * 2 % steps >= 1e-2, "Yaw range not divisable by steps."
        self.steps = steps
    
    def set_range_pitch(self, sym_range):
        if self.steps is not None:
            assert sym_range * 2 % self.steps >= 1e-2, "Pitch range not divisable by steps."
        self.pitch_range = sym_range
        
    def set_range_yaw(self, sym_range):
        if self.steps is not None:
            assert sym_range * 2 % self.steps >= 1e-2, "Yaw range not divisable by steps."
        self.yaw_range = sym_range
    
    def set_offsets(self, offsets: list):
        self.motion_system.offsets = offsets
    
    def set_measurement_function(self, measurement_func):
        self.measurement_func = measurement_func
    
    def set_measurement_repeat(self, n):
        self.repeat_n = n
    
    def run(self):
        assert self.pitch_range is not None, "Pitch range not defined!"
        assert self.yaw_range is not None, "Yaw range not defined!"
        assert self.steps is not None, "Steps not defined!"
        assert self.measurement_func is not None, "Measurement function not defined!"
        
        # build grid: y = yaw (rows), x = pitch (columns)
        n_rows = int((self.yaw_range * 2) / self.steps)
        n_cols = int((self.pitch_range * 2) / self.steps)
        self.meas_data = [[None for _ in range(n_cols)] for _ in range(n_rows)]
        
        MotionSystem.home()
        
        for yaw_pos in range(-self.yaw_range, self.yaw_range, self.steps):
            self.motion_system.move_to("Y", yaw_pos)
            row_idx = (yaw_pos + self.yaw_range) // self.steps
            for pitch_pos in range(-self.pitch_range, self.pitch_range, self.steps):
                self.motion_system.move_to("P", pitch_pos)
                col_idx = (pitch_pos + self.pitch_range) // self.steps
                
                val = 0
                for _ in range(self.repeat_n):
                    val += self.measurement_func()
                
                self.meas_data[row_idx][col_idx] = val / self.repeat_n
            
            self._write_data()
    
    def _write_data(self):
        with open(self.file_path, "w", newline="") as f:
            writer = csv.writer(f)
            for row in self.meas_data:
                writer.writerow(row)
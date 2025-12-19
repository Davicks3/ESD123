from directivity_measurement import DirectivityMeasurement
from measurement_arm import MeasurementArm

measurement = DirectivityMeasurement()


arm = MeasurementArm()
arm.set_range_pitch(85)
arm.set_range_yaw(85)
arm.set_grid_steps(5)
arm.set_offsets([9, 0])
arm.set_measurement_function(measurement.measure())
arm.set_measurement_repeat(2)

arm.run()
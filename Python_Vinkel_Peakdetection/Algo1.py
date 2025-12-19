"""
Algo 1: Interpolation

Signal is bandpassed and normalized.
We find a point by using threshold.
We walk forward until we see 3 rises and 3 falls in values.
We identify A and B point (the two highest values around the peak).
Then step back 1/45kHz and repeat.
When we dont find any peaks except the prevoiusly found peak, we stop.

The A and B point pairs are now interpolated with a windowed interpolation.
The interpolated peak value is found for each peak.

This is repeated for the other signal.

The interpolated peaks of the two signals are now cross correlated (either by multiplication or subtraction, whatever is faster).

The best correlation is used to determine the time offset.
Multiple solutions for time offset calculations:
    Calculate the average time offset
    Calculate the average time offset, make weights for each time offset (further away=lower coefficient) and calculate the weighted average
    Calculate the median

"""
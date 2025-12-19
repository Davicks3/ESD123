class Bandpass:
    def __init__(self, coefficients):
        self.coefficients = coefficients
    
    def filter_signals(self, dataframe, padding=True):
        for s in range(len(dataframe.df)):
            dataframe.df[s] = self.filter_signal(dataframe.df[s], padding)
        return dataframe

    def filter_signal(self, signal, padding=True):
        M = len(self.coefficients)
        N = len(signal)
        
        out = [0.0] * N
        
        val_range = range(N) if padding else range(M-1, N)
        
        for i in val_range:
            val = 0.0
            for j in range(M):
                k = i-j
                if k < 0:
                    break # Next term must be older, thus also less than zero.
                val += signal[k] * self.coefficients[j]
            out[i] = val
        return out
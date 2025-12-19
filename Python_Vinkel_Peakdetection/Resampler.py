import numpy as np

class Resampler:
    def __init__(self):
        self._fs_in = None  # set in resample_signals

    def resample_signals(self, dataframe, target_sample_f):
        # use dataframe.sample_t (exists per your note)
        self._fs_in = 1.0 / float(dataframe.sample_t)
        fs_out = float(target_sample_f)

        out = []
        for i in range(len(dataframe.df)):
            out.append(self.resample_signal(np.asarray(dataframe.df[i], dtype=float), fs_out))
        dataframe.df = out
        dataframe.sample_t = 1.0 / fs_out  # update to new rate
        return dataframe

    def resample_signal(self, signal, target_sample_f):
        """Full-sinc resample of a single 1D array to target_sample_f."""
        fs_in = self._fs_in
        fs_out = float(target_sample_f)

        N = signal.size
        if N <= 1 or fs_in == fs_out:
            return signal.copy()

        # Preserve exact span: last time = (N-1)/fs_in
        T_last = (N - 1) / fs_in
        M = int(round(T_last * fs_out)) + 1

        # Desired positions in input-sample units
        u = np.linspace(0.0, T_last, M) * fs_in  # shape (M,)

        # Full sinc over ALL input samples (no window)
        y = signal.astype(float, copy=False)
        y_out = np.empty(M, dtype=float)

        # Chunk to keep memory bounded (each chunk builds (c x N) matrix)
        # Aim ~ 5e6 elements per chunk (~40 MB in float64)
        chunk = max(1, int(5_000_000 // max(1, N)))
        n = np.arange(N, dtype=float)[None, :]  # (1, N)

        for i0 in range(0, M, chunk):
            i1 = min(M, i0 + chunk)
            ui = u[i0:i1].reshape(-1, 1)         # (c, 1)
            H = np.sinc(ui - n)                  # (c, N)
            numer = H @ y                        # (c,)
            denom = H.sum(axis=1)                # (c,)
            # Normalize to mitigate edge truncation on a finite record
            denom = np.where(np.abs(denom) < 1e-14, 1.0, denom)
            y_out[i0:i1] = numer / denom

        return y_out

    def interpolate_signal(self, x, y, x_new):
        """
        Full-sinc interpolation y(x) -> y(x_new).
        Assumes x is uniformly spaced and monotonic.
        """
        x = np.asarray(x, float)
        y = np.asarray(y, float)
        x_new = np.asarray(x_new, float)

        # Uniform spacing factor
        fs_in = 1.0 / (x[1] - x[0])
        # Map new points to input-sample units relative to y[0]
        u = (x_new - x[0]) * fs_in  # (M,)

        N = y.size
        M = u.size
        out = np.empty(M, dtype=float)

        chunk = max(1, int(5_000_000 // max(1, N)))
        n = np.arange(N, dtype=float)[None, :]

        for i0 in range(0, M, chunk):
            i1 = min(M, i0 + chunk)
            ui = u[i0:i1].reshape(-1, 1)
            H = np.sinc(ui - n)
            numer = H @ y
            denom = H.sum(axis=1)
            denom = np.where(np.abs(denom) < 1e-14, 1.0, denom)
            out[i0:i1] = numer / denom

        return out
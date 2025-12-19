import matplotlib.pyplot as plt
import numpy as np

"""
This is supossed to be coded a bit like it would be coded on a microprocessor.
It is not optimal Python code for that reason :)
"""

"""
Notes:
Min max scaler should not be done from one value (could be noise or measurement error).
cutoff should not be a one-value threshold for the same reason.
"""



class DataFrame:
    def __init__(self, csv_path, separator=',', header_lines=1):
        self.df = []
        with open(csv_path, "r") as f:
            lines = [line.rstrip("\n") for line in f]

        
        rows = [row.split(separator) for row in lines[header_lines:]]
        ncol = len(rows[0]) - 1 # exclude time column
        
        # add sample time.
        self.sample_t = (float(rows[-1][0]) - float(rows[0][0])) / float(len(rows))
        
        self.df = [[] for _ in range(ncol)]
        
        for row in rows:
            for j, val in enumerate(row[1:]): # exclude time column
                self.df[j].append(float(val))
        
                    
    
    def normalize(self):
        for j in range(len(self.df)):
            max_val = max(self.df[j])
            min_val = min(self.df[j])
            for i in range(len(self.df[j])):
                self.df[j][i] = 2 * (self.df[j][i] - min_val) / (max_val - min_val) - 1
            assert max(self.df[j]) == 1.0
            assert min(self.df[j]) == -1.0
    
    def cutout_signal(self, cutoff_threshold=0.1, preserve=50):
        res = []
        for j in range(len(self.df)):
            i_start = 0
            i_end = 0
            for i in range(len(self.df[j])):
                if abs(self.df[j][i]) < cutoff_threshold:
                    continue
                i_start = i
                break
            
            for i in range(len(self.df[j])-1, -1, -1):
                if abs(self.df[j][i]) < cutoff_threshold:
                    continue
                i_end = i
                break
            
            i_start = max(0, i_start-preserve)
            i_end = min(len(self.df[j]), i_end+preserve)
            
            #self.df[j] = self.df[j][i_start:i_end]
            res.append((i_start, i_end))
        return res
    
    
    
    
    def plot_all(self, intervals=None):
        """
        Plot each signal in its own subplot.

        intervals:
          - Per-signal: list of (start_idx, end_idx) with len == number of signals.
                        Example for 2 signals: [(400, 900), (500, 700)]
          - Global:     list of (start_idx, end_idx) applied to every subplot.
        """
        n_signals = len(self.df)
        if n_signals == 0:
            return

        n_samples = len(self.df[0])
        t = np.arange(n_samples) * self.sample_t

        fig, axes = plt.subplots(n_signals, 1, sharex=True, figsize=(10, 2.2 * n_signals))
        if n_signals == 1:
            axes = [axes]

        # Determine if intervals are per-signal
        def _is_pair(x):
            return isinstance(x, (list, tuple)) and len(x) == 2

        per_signal = (
            isinstance(intervals, list)
            and len(intervals) == n_signals
            and all((x is None) or _is_pair(x) for x in intervals)
        ) if intervals is not None else False

        for i, signal in enumerate(self.df):
            ax = axes[i]
            ax.plot(t, signal)
            ax.set_ylabel(f"S{i}")

            # Choose the relevant intervals for this signal
            pairs = []
            if intervals:
                if per_signal:
                    if intervals[i] is not None:
                        pairs = [intervals[i]]
                else:
                    # treat as global list of pairs
                    pairs = [p for p in intervals if _is_pair(p)]

            # Draw vertical lines for valid indices for this signal only
            for s, e in pairs:
                try:
                    si = int(s); ei = int(e)
                except (TypeError, ValueError):
                    continue
                if 0 <= si < n_samples:
                    ax.axvline(t[si])
                if 0 <= ei < n_samples:
                    ax.axvline(t[ei])

        axes[-1].set_xlabel("Time [s]")
        plt.tight_layout()
        plt.show()
    
    
    def plot_signals(self, intervals, markers=None):
        """
        Plot all signals only inside their intervals, with aligned x-axis.
        Also plot dot markers per-signal at given sample indices.

        Parameters
        ----------
        intervals : list[(s,e) or None]   # one per signal
        markers   : list[list[int] or None]  # one list per signal
        """

        n_signals = len(self.df)
        if n_signals == 0 or len(intervals) != n_signals:
            return
        if markers is None:
            markers = [None] * n_signals

        n_samples = len(self.df[0])
        t_full = np.arange(n_samples) * self.sample_t

        # ---- global x-limits from intervals ----
        xs, xe = [], []
        for inter in intervals:
            if inter:
                s,e = map(int, inter)
                if s>e: s,e = e,s
                s = max(0,min(n_samples-1,s))
                e = max(0,min(n_samples-1,e))
                xs.append(t_full[s])
                xe.append(t_full[e])
        if not xs: return

        xmin, xmax = min(xs), max(xe)

        # ---- subplots ----
        fig, axes = plt.subplots(n_signals, 1, sharex=True, figsize=(10,2.2*n_signals))
        if n_signals == 1:
            axes = [axes]

        for i,(y, inter, mks) in enumerate(zip(self.df, intervals, markers)):
            ax = axes[i]

            if inter:
                s,e = map(int,inter)
                if s>e: s,e = e,s
                s = max(0,min(n_samples-1,s))
                e = max(0,min(n_samples-1,e))
                t = t_full[s:e+1]
                ys = y[s:e+1]
                ax.plot(t, ys)

            # --- plot markers for THIS signal ---
            if mks:
                for idx in mks:
                    if 0 <= idx < n_samples:
                        ax.plot(t_full[idx], y[idx], 'r.', markersize=6)

            ax.set_xlim(xmin,xmax)
            ax.set_ylabel(f"S{i}")

        axes[-1].set_xlabel("Time [s]")
        plt.tight_layout()
        plt.show()
    
    def plot_signals_peak(self, intervals, markers=None, scatter_kwargs=None):
        """
        Plot all signals only inside their intervals, with aligned x-axis.
        Also plot per-signal scatter points.

        Parameters
        ----------
        intervals : list[(s,e) or None]          # one per signal
        markers   : list[list[(x,y)] or list[int] or None]
                    # one list per signal, each item either (x,y) pairs (preferred)
                    # or legacy int indices (mapped to time/level)
        scatter_kwargs : dict or None             # forwarded to ax.scatter (e.g. s=30)
        """

        n_signals = len(self.df)
        if n_signals == 0 or len(intervals) != n_signals:
            return

        if markers is None:
            markers = [None] * n_signals
        if scatter_kwargs is None:
            scatter_kwargs = {}

        n_samples = len(self.df[0])
        t_full = np.arange(n_samples) * self.sample_t

        # ---- global x-limits from intervals ----
        xs, xe = [], []
        for inter in intervals:
            if inter:
                s, e = map(int, inter)
                if s > e: s, e = e, s
                s = max(0, min(n_samples - 1, s))
                e = max(0, min(n_samples - 1, e))
                xs.append(t_full[s])
                xe.append(t_full[e])
        if not xs:
            return

        xmin, xmax = min(xs), max(xe)

        # ---- subplots ----
        fig, axes = plt.subplots(n_signals, 1, sharex=True, figsize=(10, 2.2 * n_signals))
        if n_signals == 1:
            axes = [axes]

        for i, (y_sig, inter, mks) in enumerate(zip(self.df, intervals, markers)):
            ax = axes[i]

            if inter:
                s, e = map(int, inter)
                if s > e: s, e = e, s
                s = max(0, min(n_samples - 1, s))
                e = max(0, min(n_samples - 1, e))
                t = t_full[s:e + 1]
                ys = y_sig[s:e + 1]
                ax.plot(t, ys)

            # --- scatter points for THIS signal ---
            if mks:
                xs_sc, ys_sc = [], []
                for item in mks:
                    # Preferred: (x,y) pairs
                    if isinstance(item, (tuple, list)) and len(item) == 2:
                        x, y = item
                        xs_sc.append(float(x))
                        ys_sc.append(float(y))
                    else:
                        # Back-compat: integer index -> (t_full[idx], y_sig[idx])
                        try:
                            idx = int(item)
                            if 0 <= idx < n_samples:
                                xs_sc.append(t_full[idx])
                                ys_sc.append(y_sig[idx])
                        except (TypeError, ValueError):
                            continue

                if xs_sc:
                    ax.scatter(xs_sc, ys_sc, **({"s": 24} | scatter_kwargs))

            ax.set_xlim(xmin, xmax)
            ax.set_ylabel(f"S{i}")

        axes[-1].set_xlabel("Time [s]")
        plt.tight_layout()
        plt.show()
                
from DataFrame import DataFrame
from Resampler import Resampler
from Peak_finder import PeakFinder



df = DataFrame("Angle test data/4vpp/test1.csv")
resampler = Resampler()
print("Sample time:", df.sample_t)

df.normalize()
signal_indicies = df.cutout_signal(0.1, 100)
df.plot_signals(signal_indicies)

df = resampler.resample_signals(df, 192_000.0)

signal_start = min([i for i, _ in signal_indicies])
signal_end = max([i for _, i in signal_indicies])

peak_finder = PeakFinder((signal_start, signal_end))
peaks = peak_finder.find_peaks(df)

signal_indicies = df.cutout_signal(0.1, 100)
df.plot_signals_peak(signal_indicies, peaks)


signal_indicies = df.cutout_signal(0.1, 100)
for i_start, i_end in signal_indicies:
    print(f"{i_start}-{i_end}: {i_end-i_start}")






target_count = 2

extremes = []
for signal_lst in df.df:
    extremes.append([])
    for i in range(target_count, len(signal_lst)-target_count):
        res = True
        res &= all(signal_lst[j] < signal_lst[j+1] for j in range(i-target_count, i))
        res &= all(signal_lst[j+1] < signal_lst[j] for j in range(i, i+target_count))
        if res:
            extremes[-1].append(i)

AB = []
for i, signal in enumerate(df.df):
    AB.append([])
    peaks = peak_finder._identify_peak_indicies(signal)
    for A, B in peaks:
        AB[-1].append(A)
        AB[-1].append(B)
        


df.plot_signals(signal_indicies, AB)
        


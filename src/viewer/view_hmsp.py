import struct
import numpy as np
import matplotlib.pyplot as plt

def read_hmsp(filename):
    with open(filename, 'rb') as f:
        # Read header: sample_rate (int), num_bins (int), min_freq (float), max_freq (float), min_val (float), max_val (float)
        header_data = f.read(6 * 4)  # 6 floats * 4 bytes
        header = struct.unpack('<iiffff', header_data)  # little endian
        sample_rate, num_bins, min_freq, max_freq, min_val, max_val = header
        
        # Read quantized percentiles: num_bins * 9 uint8_t
        data_size = num_bins * 9
        data_bytes = f.read(data_size)
        quantized = np.frombuffer(data_bytes, dtype=np.uint8).reshape(num_bins, 9)
        
        # Dequantize to float
        percentiles = quantized.astype(np.float32) / 255.0 * (max_val - min_val) + min_val
        
        return sample_rate, num_bins, min_freq, max_freq, percentiles

def visualize_hmsp(filename="../../output.hmsp"):
    sample_rate, num_bins, min_freq, max_freq, percentiles = read_hmsp(filename)
    
    # 1. Replicate the Adaptive Hybrid Mapping from fft.cpp
    f_min = min_freq
    f_max = max_freq
    crossover = 436.0
    if crossover > f_max: crossover = f_max
    if crossover < f_min: crossover = f_min

    linear_span = crossover - f_min
    log_span_decades = np.log10(f_max / crossover) if f_max > crossover else 0.0

    # Determine bin allocation
    if f_max <= crossover:
        n_linear = num_bins
    else:
        n_linear = int(linear_span)
        if n_linear > (num_bins * 3) // 4: n_linear = (num_bins * 3) // 4
        if n_linear < 1: n_linear = 1
    n_log = num_bins - n_linear

    # Generate bin centers
    freqs = np.zeros(num_bins)
    # Linear Part
    if n_linear > 0:
        f_step = linear_span / n_linear
        # We use bin centers for plotting
        freqs[:n_linear] = f_min + (np.arange(n_linear) + 0.5) * f_step
    # Log Part
    if n_log > 0:
        log_step = log_span_decades / n_log
        log_indices = np.arange(n_log) + 0.5
        freqs[n_linear:] = crossover * (10.0 ** (log_indices * log_step))

    # Percentile indices: [1%, 5%, 10%, 25%, 50%, 75%, 90%, 95%, 99%]
    # 0=1%, 1=5%, 2=10%, 3=25%, 4=50%, 5=75%, 6=90%, 7=95%, 8=99%
    perc_labels = ['1%', '5%', '10%', '25%', '50%', '75%', '90%', '95%', '99%']
    
    # Plot
    plt.figure(figsize=(14, 8))
    
    # Use a colormap to distinguish the 9 percentile lines
    colors = plt.cm.viridis(np.linspace(0, 1, 9))
    
    for i in range(9):
        # Median (50%) gets a bold black line for contrast
        if i == 4:
            plt.plot(freqs, percentiles[:, i], color='black', linewidth=2.0, label='Median (50%)', zorder=10)
        else:
            # Other lines are slightly thicker now that shading is gone
            plt.plot(freqs, percentiles[:, i], color=colors[i], linewidth=1.0, alpha=0.9, label=perc_labels[i])
    
    plt.xscale('log')
    plt.grid(True, which="both", ls="-", alpha=0.3)
    plt.xlabel('Frequency (Hz)')
    plt.ylabel('dB')
    plt.title(f'Spectral Percentiles (Hybrid Millidecade)\n{filename}')
    
    # Legend outside to the right
    plt.legend(loc='center left', bbox_to_anchor=(1, 0.5), fontsize='small', frameon=True)
    
    # Optional: Highlight the crossover
    if crossover > f_min and crossover < f_max:
        plt.axvline(crossover, color='red', linestyle='--', alpha=0.4, label='Crossover')

    plt.tight_layout()
    plt.show()

if __name__ == '__main__':
    import sys
    if len(sys.argv) != 2:
        visualize_hmsp()
    else:
        visualize_hmsp(sys.argv[1])
    
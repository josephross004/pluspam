import numpy as np
import matplotlib.pyplot as plt

# Updated Python Header to match C++ spec_header
header_dtype = np.dtype([
    ('fs', np.int32),
    ('num_bins', np.int32),
    ('min_f', np.float32),
    ('max_f', np.float32),
    ('min_val', np.float32), # Added
    ('max_val', np.float32)  # Added
])

with open("../../output.hmsb", "rb") as f:
    # Read the first 16 bytes for the header
    header_data = np.frombuffer(f.read(header_dtype.itemsize), dtype=header_dtype)[0]
    
    # Read the rest of the file as the actual FFT data
    data = np.fromfile(f, dtype=np.float32)

# Extract values from header
fs = header_data['fs']
num_bins = header_data['num_bins']
min_f = header_data['min_f']
max_f = header_data['max_f']

print(f"Loaded Spectrogram: {fs}Hz, {num_bins} bins, Range: {min_f}-{max_f}Hz")

# 2. Reshape and Plot
rows = data.reshape(-1, num_bins).T

# Log-frequency axis calculation
bin_indices = np.arange(num_bins)
frequencies = min_f * (max_f / min_f) ** (bin_indices / (num_bins - 1))

plt.figure(figsize=(12, 6))
img = plt.imshow(rows, aspect='auto', origin='lower', cmap='magma',
                 extent=[0, rows.shape[1], 0, num_bins])

tick_indices = np.linspace(0, num_bins - 1, 10).astype(int)
plt.yticks(tick_indices, [f"{int(frequencies[i])} Hz" for i in tick_indices])

plt.colorbar(label='dB')
plt.title('Self-Describing Spectrogram')
plt.show()
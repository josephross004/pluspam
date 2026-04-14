import sys
import tkinter as tk
from tkinter import filedialog, messagebox, ttk
import subprocess
import os
import struct
import numpy as np
import matplotlib.pyplot as plt
from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg, NavigationToolbar2Tk

class SpecGenGUI:
    def __init__(self, root):
        self.root = root
        self.root.title("SpecGen & HMS Viewer")
        self.root.geometry("900x700")
        
        # 1. Detect if we are running inside a PyInstaller bundle
        if getattr(sys, 'frozen', False):
            # If bundled, the binary is inside the temp folder (_MEIPASS)
            self.bundle_dir = sys._MEIPASS
            # The folder where the .exe actually lives (for saving outputs)
            self.exe_dir = os.path.dirname(sys.executable)
        else:
            # If running as script, use standard relative paths
            self.bundle_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
            self.exe_dir = self.bundle_dir

        # 2. Set the Binary Path (bundled inside)
        if os.name == 'nt':
            self.spec_gen_exe = os.path.join(self.bundle_dir, "spec_gen.exe")
        else:
            self.spec_gen_exe = os.path.join(self.bundle_dir, "spec_gen_mac")

        # 3. Set Default Output Paths (to the folder where the .exe lives, not temp folder!)
        self.input_wav = tk.StringVar()
        self.output_hmsb = tk.StringVar(value=os.path.join(self.exe_dir, "output.hmsb"))
        self.output_hmsp = tk.StringVar(value=os.path.join(self.exe_dir, "output.hmsp"))
        
        # DEBUG
        print(f"DEBUG: Bundle Dir: {self.bundle_dir}")
        print(f"DEBUG: Binary Path: {self.spec_gen_exe}")
        
        self.setup_ui()

    def setup_ui(self):
        # Use a style for a more modern look
        style = ttk.Style()
        style.configure("TButton", padding=5, font=('Segoe UI', 10))
        style.configure("TLabel", font=('Segoe UI', 10))
        style.configure("Header.TLabel", font=('Segoe UI', 12, 'bold'))

        main_frame = ttk.Frame(self.root, padding="20")
        main_frame.pack(fill=tk.BOTH, expand=True)

        # --- Section 1: Generation ---
        gen_label = ttk.Label(main_frame, text="Generate HMS Files from WAV", style="Header.TLabel")
        gen_label.grid(row=0, column=0, columnspan=3, sticky=tk.W, pady=(0, 10))

        ttk.Label(main_frame, text="Input WAV:").grid(row=1, column=0, sticky=tk.W)
        ttk.Entry(main_frame, textvariable=self.input_wav, width=60).grid(row=1, column=1, padx=5)
        ttk.Button(main_frame, text="Browse...", command=self.browse_wav).grid(row=1, column=2)

        ttk.Label(main_frame, text="Output HMSB:").grid(row=2, column=0, sticky=tk.W, pady=5)
        ttk.Entry(main_frame, textvariable=self.output_hmsb, width=60).grid(row=2, column=1, padx=5)
        ttk.Button(main_frame, text="Browse...", command=lambda: self.browse_output('hmsb')).grid(row=2, column=2)

        ttk.Label(main_frame, text="Output HMSP:").grid(row=3, column=0, sticky=tk.W, pady=5)
        ttk.Entry(main_frame, textvariable=self.output_hmsp, width=60).grid(row=3, column=1, padx=5)
        ttk.Button(main_frame, text="Browse...", command=lambda: self.browse_output('hmsp')).grid(row=3, column=2)

        self.run_btn = ttk.Button(main_frame, text="Run spec_gen", command=self.run_spec_gen)
        self.run_btn.grid(row=4, column=0, columnspan=3, pady=10)

        ttk.Separator(main_frame, orient=tk.HORIZONTAL).grid(row=5, column=0, columnspan=3, sticky="ew", pady=15)

        # --- Section 2: Viewing ---
        view_label = ttk.Label(main_frame, text="Load and View Existing Files", style="Header.TLabel")
        view_label.grid(row=6, column=0, columnspan=3, sticky=tk.W, pady=(0, 10))

        btn_frame = ttk.Frame(main_frame)
        btn_frame.grid(row=7, column=0, columnspan=3, pady=5)

        ttk.Button(btn_frame, text="View Spectrogram (.hmsb)", command=self.view_hmsb).pack(side=tk.LEFT, padx=10)
        ttk.Button(btn_frame, text="View Percentiles (.hmsp)", command=self.view_hmsp).pack(side=tk.LEFT, padx=10)

        # --- Status Area ---
        self.status_var = tk.StringVar(value="Ready")
        status_label = ttk.Label(main_frame, textvariable=self.status_var, relief=tk.SUNKEN, anchor=tk.W)
        status_label.grid(row=8, column=0, columnspan=3, sticky="ew", pady=(20, 0))

    def browse_wav(self):
        filename = filedialog.askopenfilename(filetypes=[("WAV files", "*.wav"), ("All files", "*.*")])
        if filename:
            self.input_wav.set(filename)

    def browse_output(self, ext):
        filename = filedialog.asksaveasfilename(defaultextension=f".{ext}", filetypes=[(f"{ext.upper()} files", f"*.{ext}"), ("All files", "*.*")])
        if filename:
            if ext == 'hmsb': self.output_hmsb.set(filename)
            else: self.output_hmsp.set(filename)

    def run_spec_gen(self):
        wav = self.input_wav.get()
        hmsb = self.output_hmsb.get()
        hmsp = self.output_hmsp.get()

        if not wav or not os.path.exists(wav):
            messagebox.showerror("Error", "Please select a valid input WAV file.")
            return

        self.status_var.set("Running spec_gen...")
        self.root.update_idletasks()

        try:
            env = os.environ.copy()
            # Only need to hack the PATH on Windows if using dynamic FFTW
            if os.name == 'nt':
                fftw_dir = os.path.join(self.bundle_dir, "fftw_win")
                env["PATH"] = fftw_dir + os.pathsep + env.get("PATH", "")

            cmd = [self.spec_gen_exe, wav, hmsb, hmsp]
            # On Mac/Linux, we might need to make sure the bundled file has exec permissions
            if os.name != 'nt':
                os.chmod(self.spec_gen_exe, 0o755)

            result = subprocess.run(cmd, capture_output=True, text=True, env=env)
        except Exception as e:
            self.status_var.set(f"Error: {str(e)}")
            messagebox.showerror("Error", f"An unexpected error occurred: {str(e)}")

    def view_hmsp(self):
        filename = filedialog.askopenfilename(filetypes=[("HMSP files", "*.hmsp"), ("All files", "*.*")])
        if not filename: return
        
        try:
            self.plot_hmsp(filename)
        except Exception as e:
            messagebox.showerror("Error", f"Failed to plot HMSP: {str(e)}")

    def view_hmsb(self):
        filename = filedialog.askopenfilename(filetypes=[("HMSB files", "*.hmsb"), ("All files", "*.*")])
        if not filename: return
        
        try:
            self.plot_hmsb(filename)
        except Exception as e:
            messagebox.showerror("Error", f"Failed to plot HMSB: {str(e)}")

    def plot_hmsp(self, filename):
        # Adapted logic from view_hmsp.py
        with open(filename, 'rb') as f:
            header_data = f.read(6 * 4)
            header = struct.unpack('<iiffff', header_data)
            sample_rate, num_bins, min_freq, max_freq, min_val, max_val = header
            data_bytes = f.read(num_bins * 9)
            quantized = np.frombuffer(data_bytes, dtype=np.uint8).reshape(num_bins, 9)
            percentiles = quantized.astype(np.float32) / 255.0 * (max_val - min_val) + min_val

        f_min, f_max = min_freq, max_freq
        crossover = 436.0
        if crossover > f_max: crossover = f_max
        if crossover < f_min: crossover = f_min

        linear_span = crossover - f_min
        log_span_decades = np.log10(f_max / crossover) if f_max > crossover else 0.0

        if f_max <= crossover:
            n_linear = num_bins
        else:
            n_linear = int(linear_span)
            if n_linear > (num_bins * 3) // 4: n_linear = (num_bins * 3) // 4
            if n_linear < 1: n_linear = 1
        n_log = num_bins - n_linear

        freqs = np.zeros(num_bins)
        if n_linear > 0:
            f_step = linear_span / n_linear
            freqs[:n_linear] = f_min + (np.arange(n_linear) + 0.5) * f_step
        if n_log > 0:
            log_step = log_span_decades / n_log
            log_indices = np.arange(n_log) + 0.5
            freqs[n_linear:] = crossover * (10.0 ** (log_indices * log_step))

        perc_labels = ['1%', '5%', '10%', '25%', '50%', '75%', '90%', '95%', '99%']
        
        fig, ax = plt.subplots(figsize=(10, 6))
        colors = plt.cm.viridis(np.linspace(0, 1, 9))
        for i in range(9):
            if i == 4:
                ax.plot(freqs, percentiles[:, i], color='black', linewidth=2.0, label='Median (50%)', zorder=10)
            else:
                ax.plot(freqs, percentiles[:, i], color=colors[i], linewidth=1.0, alpha=0.9, label=perc_labels[i])
        
        ax.set_xscale('log')
        ax.grid(True, which="both", ls="-", alpha=0.3)
        ax.set_xlabel('Frequency (Hz)')
        ax.set_ylabel('dB')
        ax.set_title(f'Spectral Percentiles: {os.path.basename(filename)}')
        ax.legend(loc='center left', bbox_to_anchor=(1, 0.5), fontsize='small')
        fig.tight_layout()
        plt.show()

    def plot_hmsb(self, filename):
        # Adapted logic from view.py
        header_dtype = np.dtype([
            ('fs', np.int32), ('num_bins', np.int32),
            ('min_f', np.float32), ('max_f', np.float32),
            ('min_val', np.float32), ('max_val', np.float32)
        ])
        
        with open(filename, "rb") as f:
            header_data = np.frombuffer(f.read(header_dtype.itemsize), dtype=header_dtype)[0]
            data = np.fromfile(f, dtype=np.float32)

        fs, num_bins = header_data['fs'], header_data['num_bins']
        min_f, max_f = header_data['min_f'], header_data['max_f']
        
        rows = data.reshape(-1, num_bins).T
        bin_indices = np.arange(num_bins)
        frequencies = min_f * (max_f / min_f) ** (bin_indices / (num_bins - 1))

        fig, ax = plt.subplots(figsize=(10, 6))
        img = ax.imshow(rows, aspect='auto', origin='lower', cmap='magma',
                        extent=[0, rows.shape[1], 0, num_bins])
        
        tick_indices = np.linspace(0, num_bins - 1, 10).astype(int)
        ax.set_yticks(tick_indices)
        ax.set_yticklabels([f"{int(frequencies[i])} Hz" for i in tick_indices])
        
        plt.colorbar(img, label='dB')
        ax.set_title(f'Spectrogram: {os.path.basename(filename)}')
        ax.set_xlabel('Time (bins)')
        fig.tight_layout()
        plt.show()

if __name__ == "__main__":
    root = tk.Tk()
    app = SpecGenGUI(root)
    root.mainloop()

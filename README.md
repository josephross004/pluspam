## Testing on TS-7553-v2

1. Put an audio file somewhere on disk. For example, say I had a .wav in the folder:
   `\home\engineer\test.wav`

2. Run the program: You can generate just percentiles or both percentile and spectrogram.

```bash
$    ./spec_gen /home/engineer/test.wav spectrogram.hmsb percentiles.hmsp
```

or for just percentiles: 
```bash
$    ./spec_gen -p /home/engineer/test.wav percentiles_streamed.hmsp
```

They're both binary files so the file extension doesn't matter so much, but, I've been using `.hmsb` for `H`ybrid `M`illidecade `S`pectrogram `B`inary and `.hmsp` for `H`ybrid `M`illidecade `S`pectrogram `P`ercentiles. 

On windows x64 you can run `python ./src/viewer/gui.py` for a basic visual interface or rendering the `.hmsb` or `.hmsp` data. 

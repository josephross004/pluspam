## Set Up on Linux ARM x32 (Ts-7553-v2)

Have a directory with your `in` and `out` (input and output) folders.
```bash
$ ls -l
total 0
drwxr-xr-x 1 dev dev 4096 Mar 27 18:56 in
drwxr-xr-x 1 dev dev 4096 Mar 27 18:54 out
```
Then clone this repo into that folder.
```bash
$ git clone https://github.com/josephross004/pluspam
Cloning into 'pluspam'...
remote: Enumerating objects: 52, done.
remote: Counting objects: 100% (52/52), done.
remote: Compressing objects: 100% (42/42), done.
remote: Total 52 (delta 11), reused 43 (delta 6), pack-reused 0 (from 0)
Receiving objects: 100% (52/52), 28.95 MiB | 6.40 MiB/s, done.
Resolving deltas: 100% (11/11), done.
```
Go into the `pluspam` folder. Then run this command to set the permissions correctly.
```bash
$ chmod +x spec_gen batch_process.sh
```
To batch process, execute the `batch_process.sh` file.
```bash
$ ./batch_process.sh
```
all of the files in the `in` folder will be processed into the `out` folder. 



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

On windows x64 you can run `python ./src/viewer/gui.py` for a basic visual interface or rendering the `.hmsb` or `.hmsp` data. You do need to have some dependencies installed, you need `matplotlib`, `tkinter`, `subprocess`, `numpy`, `struct`, `os` installed (some of those come with Python by default depending on how the interpreter was installed.)


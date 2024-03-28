
# VGM-TapePlay

Sneptile is port of AVR-PSG that runs on the SG-1000 / SC-3000

Usage: `./build.sh [--pal] <my_music.vgm>`

The output files are:
 * `VGM-TapePlay.sg` - A ROM file for running as a cartridge
 * `VGM-TapePlay.wav` - A cassette image that can be loaded into BASIC IIIa or BASIC IIIb

To load over tape, the following steps are used on the SC-3000:

 1. `LOAD` on the SC-3000
 2. Play the .wav file
 3. `CALL &H9800` on the SC-3000

## Dependencies
 * zlib

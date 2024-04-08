#!/bin/sh
echo ""
echo "VGM-TapePlay Build Script"
echo "-------------------------------"

# Exit on first error.
set -e

INPUT_FILE="none"
PAL_MODE="no"

sdcc="${HOME}/Code/sdcc-4.3.0/bin/sdcc"
devkitSMS="${HOME}/Code/devkitSMS"
SMSlib="${devkitSMS}/SMSlib"
SGlib="${devkitSMS}/SGlib"
ihx2sms="${devkitSMS}/ihx2sms/Linux/ihx2sms"
sneptile="./tools/Sneptile-0.4.0/Sneptile"

# SC-3000 Tape Support
tapewave="./tools/SC-TapeWave/tapewave"

build_sneptile ()
{
    # Early return if we've already got an up-to-date build
    if [ -e $sneptile \
         -a "./tools/Sneptile-0.4.0/source/main.c" -ot $sneptile \
         -a "./tools/Sneptile-0.4.0/source/sms_vdp.c" -ot $sneptile \
         -a "./tools/Sneptile-0.4.0/source/tms9928a.c" -ot $sneptile ]
    then
        return
    fi

    echo "Building Sneptile..."
    (
        cd "tools/Sneptile-0.4.0"
        ./build.sh
    )
}


build_tapewave ()
{
    # Early return if we've already got an up-to-date build
    if [ -e $tapewave -a "./tools/SC-TapeWave/source/main.c" -ot $tapewave ]
    then
        return
    fi

    echo "Building SC-TapeWave..."
    (
        cd "tools/SC-TapeWave"
        ./build.sh
    )
    return
}


build_vgm_convert ()
{
    gcc source/vgm_convert/vgm_convert.c \
    source/vgm_convert/vgm_read.c \
    -o vgm_convert -lz
}


build_vgm_tapeplay ()
{
    echo "Building VGM-TapePlay for SC-3000 Tape..."
    rm -rf build tile_data music_data

    echo "  Generating tile data..."
    mkdir -p tile_data
    $sneptile --mode-2 --output tile_data tiles/player.png

    echo "  Generating music data... (${INPUT_FILE})"
    mkdir -p music_data
    if [ "${PAL_MODE}" = "yes" ]
    then
        ./vgm_convert --pal "${INPUT_FILE}" > music_data/music.h
    else
        ./vgm_convert "${INPUT_FILE}" > music_data/music.h
    fi

    mkdir -p build
    echo "  Compiling..."
    for file in main
    do
        echo "   -> ${file}.c"
        ${sdcc} -c -mz80 -I ${SGlib}/src -o "build/${file}.rel" "source/${file}.c"
    done

    # Also generate an SG-1000 ROM for quick testing.
    echo ""
    echo "  Linking (ROM)..."
    ${sdcc} -o build/VGM-TapePlay.ihx -mz80 --no-std-crt0 --data-loc 0xC000 ${devkitSMS}/crt0/crt0_sg.rel build/*.rel ${SGlib}/SGlib.rel

    echo ""
    echo "  Generating ROM..."
    ${ihx2sms} build/VGM-TapePlay.ihx VGM-TapePlay.sg


    # Tape Memory layout:
    #
    #   0x0000 -- 0x7fff BASIC ROM
    #   0x8000 -- 0x97ff RAM, previously reserved for use by BASIC
    #   0x9800 -- 0x989f Header area. Setup code at 0x9800, interrupt vector at 0x9898.
    #   0x98a0 -- 0xc800 Program storage. 12 kB for BASIC IIIa, or 26 kB for BASIC IIIb
    #
    # A special crt0 is used to handle the new addresses and set up interrupt-mode 2.

    echo ""
    echo "  Linking (tape)..."
    ${sdcc} -o build/VGM-TapePlay-tape.ihx -mz80 --no-std-crt0 --code-loc 0x98a0 --data-loc 0x8000 \
        ${devkitSMS}/crt0/crt0_BASIC.rel build/*.rel ${SGlib}/SGlib.rel

    echo ""
    echo "  Generating Tape..."
    objcopy -Iihex -Obinary build/VGM-TapePlay-tape.ihx build/VGM-TapePlay-tape.bin
    ${tapewave} "VGM-TapePlay" build/VGM-TapePlay-tape.bin VGM-TapePlay.wav

    # Sanity-check the size
    SIZE="$(wc -c build/VGM-TapePlay-tape.bin | cut -d ' ' -f 1)"
    echo "    Size is ${SIZE} bytes."
    if [ ${SIZE} -gt 26624 ]
    then
        echo "    WARNING: Cassette too large for BASIC IIIa or BASIC IIIb."
    elif [ ${SIZE} -gt 12288 ]
    then
        echo "    WARNING: Cassette too large for BASIC IIIa. Okay for BASIC IIIb."
    fi

    echo ""
    echo "  Done"
}


# Check parameters.
if [ $# -eq 0 ]
then
    echo  "Usage: $0 [--pal] <input_file.vgm>"
    exit
fi

if [ "${1}" = "--pal" ]
then
    PAL_MODE="yes"
    shift
fi

INPUT_FILE="${1}"

build_sneptile
build_tapewave
build_vgm_convert
build_vgm_tapeplay

#!/bin/sh
echo ""
echo "VGM-TapePlay Build Script"
echo "-------------------------------"

# Exit on first error.
set -e

sdcc="${HOME}/Code/sdcc-4.3.0/bin/sdcc"
devkitSMS="${HOME}/Code/devkitSMS"
SMSlib="${devkitSMS}/SMSlib"
SGlib="${devkitSMS}/SGlib"
ihx2sms="${devkitSMS}/ihx2sms/Linux/ihx2sms"
sneptile="./tools/Sneptile-0.4.0/Sneptile"

# SC-3000 Tape Support
crt0_sc_tape="./tools/crt0_sc_tape"
SGlib_sc_tape="./tools/SGlib_sc_tape"
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


build_vgm_tapeplay ()
{
    echo "Building VGM-TapePlay for SC-3000 Tape..."
    rm -rf build tile_data

    echo "  Generating tile data..."
    mkdir -p tile_data
    (
        # Note, tiles are organized so that similar-coloured files are
        # together, as they can share a mode-0 colour-table entry.
        $sneptile --mode-2 --output tile_data \
            tiles/player.png
    )

    mkdir -p build
    echo "  Compiling..."
    for file in main
    do
        echo "   -> ${file}.c"
        ${sdcc} -c -mz80 --peep-file ${devkitSMS}/SGlib/peep-rules.txt -I ${SGlib}/src \
            -o "build/${file}.rel" "source/${file}.c"
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
    #   0x9800 -- 0xc800 Program storage. 12 kB for BASIC IIIa, or 26 kB for BASIC IIIb
    #
    # A special crt0 is used to handle the new addresses, and a special SGlib is used
    # to poll for the VDP interrupt status bit.

    echo ""
    echo "  Linking (tape)..."
    ${sdcc} -o build/VGM-TapePlay-tape.ihx -mz80 --no-std-crt0 --code-loc 0x9800 --data-loc 0x8000 \
        ${crt0_sc_tape}/crt0_sg.rel build/*.rel ${SGlib_sc_tape}/SGlib.rel

    echo ""
    echo "  Generating Tape..."
    objcopy -Iihex -Obinary build/VGM-TapePlay-tape.ihx build/VGM-TapePlay-tape.bin
    ${tapewave} "VGM-TapePlay" build/VGM-TapePlay-tape.bin VGM-TapePlay.wav

    echo ""
    echo "  Done"
}

build_sneptile
build_tapewave
build_vgm_tapeplay

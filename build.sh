#!/bin/sh

# Exit on error
set -e

# Set working directory
WORKSPACE=/spaceinvader
cd $WORKSPACE

# Make directories for hexadecimal files and the final binary
mkdir $WORKSPACE/hex
mkdir $WORKSPACE/dist

# Initialize and update git submodules
git submodule init
git submodule update

# Compile micro-ecc library
make --directory=$WORKSPACE/apps/secure_bootloader/micro-ecc-build

# Set the public key for the secure bootloader
sed -i "s/\/\* PUBLIC_KEY_PLACEHOLDER \*\//$PUBLIC_KEY_HEX/" $WORKSPACE/apps/firmware/dfu_images/public_key.c

# Compile secure bootloader
make --directory=$WORKSPACE/apps/secure_bootloader/build
mv $WORKSPACE/apps/secure_bootloader/build/_build/secure_bootloader_moko.hex $WORKSPACE/hex

# Compile firmware
make --directory=$WORKSPACE/apps/firmware/build
mv $WORKSPACE/apps/firmware/build/_build/nrf52810_xxaa.hex $WORKSPACE/hex

# Make hexadecimal files
cd $WORKSPACE/hex

# Generate DFU settings
nrfutil settings generate --family NRF52810 --application nrf52810_xxaa.hex --application-version 1 --bootloader-version 1 --bl-settings-version 2 bl_settings.hex

# Merge hex files
cp $WORKSPACE/nRF5_SDK_17.0.2_d674dde/components/softdevice/s112/hex/s112_nrf52_7.2.0_softdevice.hex $WORKSPACE/hex
mergehex --merge bl_settings.hex secure_bootloader_moko.hex nrf52810_xxaa.hex s112_nrf52_7.2.0_softdevice.hex --output firmware.hex

# debug
ls -alh firmware.hex

# Produce the DFU release artifact: the APPLICATION-ONLY binary.
#
# The backend's DFU pipeline (blu-transmogrifier) patches a per-device key into
# this binary and packages it as a buttonless *application* DFU, so it expects
# ONLY the application image (the same ~50 KB shape as the legacy
# nrf52810_xxaa.bin) -- NOT the merged MBR+SoftDevice+bootloader+app image.
#
# nrf52810_xxaa.hex is the app-only build output (linked above the SoftDevice).
# objcopy -O binary writes it starting at the app's first byte, so the file
# begins with the application vector table. The app hex has no UICR/FICR record,
# so the old 256 MB blow-up cannot happen here -- no crop/fill needed.
arm-none-eabi-objcopy -I ihex -O binary nrf52810_xxaa.hex $WORKSPACE/dist/firmware.bin

# Keep the full merged image (MBR + SoftDevice + bootloader + settings + app)
# for first-time factory provisioning over SWD. Not used by the DFU pipeline.
cp firmware.hex $WORKSPACE/dist/firmware.hex

# debug
ls -alh $WORKSPACE/dist/firmware.bin $WORKSPACE/dist/firmware.hex

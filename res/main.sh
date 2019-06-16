#!/bin/sh
cd $(dirname $(readlink -f "${0}")) || exit 1

convert main.png \
  '(' -clone 0 -filter Lanczos -resize 18x18 -crop 16x16+1+1 ')' \
  '(' -clone 0 -filter Lanczos -resize 22x22 -crop 20x20+1+1 ')' \
  '(' -clone 0 -filter Lanczos -resize 26x26 -crop 24x24+1+1 ')' \
  '(' -clone 0 -filter Lanczos -resize 32x32 ')' \
  '(' -clone 0 -filter Lanczos -resize 40x40 ')' \
  '(' -clone 0 -filter Lanczos -resize 48x48 ')' \
  '(' -clone 0 -filter Lanczos -resize 60x60 ')' \
  '(' -clone 0 -filter Lanczos -resize 72x72 ')' \
  '(' -clone 0 -filter Lanczos -resize 256x256 ')' \
  -delete 0 -background none main.ico

# Open `res/main.ico` in GIMP and fix low resolution icons.
# Export as `src/main.ico` and select "32 bpp, 8-bit alpha, no palette" for all sizes.

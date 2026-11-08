#!/bin/sh
# Portable checks only: these validate algorithms and mock RSX command usage.
# They DO NOT measure frame rate or constitute PS3/RPCS3 runtime tests.
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/host
cc -std=c11 -O2 -g -Wall -Wextra -Werror -fsanitize=undefined -Iinclude \
 src/pixel_convert.c tests/test_pixel_convert.c -o build/host/test_pixel_convert
build/host/test_pixel_convert
cc -std=c11 -O2 -g -Wall -Wextra -Werror -fsanitize=undefined \
 -Itests/mock -Iinclude src/rsx_renderer.c src/pixel_convert.c \
 tests/test_rsx_upload.c -o build/host/test_rsx_upload
build/host/test_rsx_upload
cc -std=c11 -O2 -g -Wall -Wextra -Werror -fsanitize=undefined \
 -Iinclude src/controls.c src/profile.c src/browser.c tests/test_platform.c -lm \
 -o build/host/test_platform
build/host/test_platform
cc -std=c11 -O2 -g -Wall -Wextra -Werror -fsanitize=undefined \
 -Iinclude -Ivendor/rsxkit/include src/ps3me_ui.c \
 tests/test_ps3me_ui.c -lm -o build/host/test_ps3me_ui
build/host/test_ps3me_ui
cc -std=c11 -O2 -g -Wall -Wextra -Werror -fsanitize=undefined \
 -Ivendor/rsxkit/include vendor/rsxkit/src/ui_geom.c vendor/rsxkit/src/text.c \
 tests/test_ui_atlas.c -lm -o build/host/test_ui_atlas
build/host/test_ui_atlas

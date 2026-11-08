#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/host
cc -std=c11 -O2 -g -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude \
    src/pixel_convert.c tests/test_pixel_convert.c -o build/host/test_pixel_convert
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" build/host/test_pixel_convert
cc -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined \
    -Itests/mock -Iinclude src/rsx_renderer.c src/pixel_convert.c \
    tests/test_rsx_upload.c -o build/host/test_rsx_upload
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" build/host/test_rsx_upload
cc -std=c11 -O1 -g -Wall -Wextra -Werror -fsanitize=address,undefined -Iinclude src/controls.c src/profile.c src/browser.c tests/test_platform.c -lm -o build/host/test_platform
# This sandbox cannot enumerate /proc for LeakSanitizer; ASan/UBSan remain enabled.
ASAN_OPTIONS="${ASAN_OPTIONS:-detect_leaks=0}" build/host/test_platform
cc -std=c11 -O2 -Wall -Wextra -Werror -fsanitize=undefined -Iinclude -Ivendor/rsxkit/include \
    src/ps3me_ui.c tests/test_ps3me_ui.c -lm -o build/host/test_ps3me_ui
build/host/test_ps3me_ui
if [ ! -d deps/ruffle ]; then echo 'Run scripts/get-ruffle.sh before the Ruffle integration tests.' >&2; exit 1; fi
cargo build --locked --manifest-path host-runner/Cargo.toml
runner=host-runner/target/debug/ruffle-ps3-host-runner
for vm in avm1 avm2; do
    root="deps/ruffle/tests/tests/swfs/$vm/trace"
    "$runner" "$root/test.swf" 1 > "build/host/$vm.txt"
    diff -u "$root/output.txt" "build/host/$vm.txt"
    echo "$vm trace: passed"
done
"$runner" deps/ruffle/tests/tests/swfs/avm1/focus_keyboard_press/test.swf 1 --input-smoke > build/host/input.txt
grep -Fq '_level0.clip.onPress: 32' build/host/input.txt
grep -Fq '_level0.clip.onRelease: 32' build/host/input.txt
echo 'AVM1 mapped keyboard input smoke: passed'
printf 'bad' > build/host/invalid.swf
if "$runner" build/host/invalid.swf 1 >/dev/null 2>&1; then
    echo 'truncated SWF was accepted' >&2; exit 1
fi
echo 'truncated SWF rejection: passed'

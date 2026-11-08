fn main() {
    cc::Build::new().include("../include").file("../src/controls.c")
        .file("src/input_fixture.c").compile("ruffle_ps3_input_fixture");
    println!("cargo:rerun-if-changed=../src/controls.c");
    println!("cargo:rerun-if-changed=../include/controls.h");
    println!("cargo:rerun-if-changed=src/input_fixture.c");
}

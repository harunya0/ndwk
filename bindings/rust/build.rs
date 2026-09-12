use std::env;
use std::path::PathBuf;

fn main() {
    let manifest_dir = PathBuf::from(env::var("CARGO_MANIFEST_DIR").unwrap());
    let build_dir = manifest_dir.join("../../build");
    println!("cargo:rustc-link-search=native={}", build_dir.display());
    println!("cargo:rustc-link-lib=dylib=ndwk");

    println!("cargo:rustc-link-arg=-Wl,-rpath,$ORIGIN/../../build");
    println!("cargo:rustc-link-arg=-Wl,-rpath,{}", build_dir.display());
}

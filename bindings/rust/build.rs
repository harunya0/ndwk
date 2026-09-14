use std::env;
use std::fs;
use std::path::PathBuf;

const NDWK_VERSION: &str = "0.1.3";

fn main() {
    println!("cargo:rerun-if-env-changed=NDWK_LIB_DIR");

    let lib_dir = if let Ok(dir) = env::var("NDWK_LIB_DIR") {
        // Local development: use provided library directory
        PathBuf::from(dir)
    } else {
        // Production: download prebuilt binaries
        download_prebuilt()
    };

    println!("cargo:rustc-link-search=native={}", lib_dir.display());
    println!("cargo:rustc-link-lib=dylib=ndwk");

    // Set RPATH for runtime library loading
    let target_os = env::var("CARGO_CFG_TARGET_OS").unwrap_or_default();
    match target_os.as_str() {
        "linux" => {
            println!("cargo:rustc-link-arg=-Wl,-rpath,$ORIGIN");
            println!("cargo:rustc-link-arg=-Wl,-rpath,{}", lib_dir.display());
        }
        "macos" => {
            println!("cargo:rustc-link-arg=-Wl,-rpath,@executable_path");
            println!("cargo:rustc-link-arg=-Wl,-rpath,{}", lib_dir.display());
        }
        _ => {} // Windows uses PATH
    }
}

fn download_prebuilt() -> PathBuf {
    let out_dir = PathBuf::from(env::var("OUT_DIR").expect("OUT_DIR not set"));
    let lib_dir = out_dir.join("ndwk-native");

    // Skip download if already extracted
    if lib_dir.exists() {
        return lib_dir;
    }

    let target_os = env::var("CARGO_CFG_TARGET_OS").expect("CARGO_CFG_TARGET_OS not set");
    let target_arch = env::var("CARGO_CFG_TARGET_ARCH").expect("CARGO_CFG_TARGET_ARCH not set");

    let platform = match (target_os.as_str(), target_arch.as_str()) {
        ("linux", "x86_64")  => "linux-x64",
        ("linux", "aarch64") => "linux-arm64",
        ("macos", "aarch64") => "osx-arm64",
        ("windows", "x86_64") => "win-x64",
        _ => panic!("Unsupported platform: {target_os}-{target_arch}. Supported: linux-x64, linux-arm64, osx-arm64, win-x64"),
    };

    let is_windows = target_os == "windows";
    let ext = if is_windows { "zip" } else { "tar.gz" };
    let url = format!(
        "https://github.com/harunya0/ndwk/releases/download/v{NDWK_VERSION}/ndwk-v{NDWK_VERSION}-{platform}.{ext}"
    );

    eprintln!("Downloading ndwk native libraries from: {url}");

    let response = reqwest::blocking::Client::builder()
        .timeout(std::time::Duration::from_secs(300))
        .build()
        .expect("Failed to create HTTP client")
        .get(&url)
        .send()
        .unwrap_or_else(|e| panic!("Failed to download ndwk native libraries from {url}: {e}"));

    if !response.status().is_success() {
        panic!("Failed to download {url}: HTTP {}", response.status());
    }

    let bytes = response.bytes().expect("Failed to read response body");

    fs::create_dir_all(&lib_dir).expect("Failed to create lib directory");

    if is_windows {
        extract_zip(&bytes, &lib_dir);
    } else {
        extract_tar_gz(&bytes, &lib_dir);
    }

    eprintln!("ndwk native libraries extracted to: {}", lib_dir.display());
    lib_dir
}

fn extract_tar_gz(data: &[u8], dest: &PathBuf) {
    let decoder = flate2::read::GzDecoder::new(data);
    let mut archive = tar::Archive::new(decoder);
    archive.unpack(dest).expect("Failed to extract tar.gz archive");
}

fn extract_zip(data: &[u8], dest: &PathBuf) {
    let reader = std::io::Cursor::new(data);
    let mut archive = zip::ZipArchive::new(reader).expect("Failed to open zip archive");
    archive.extract(dest).expect("Failed to extract zip archive");
}

use ndwk::{NdwkEngine, NdwkLang};
use std::env;
use std::fs::File;
use std::io::{self, Read, Seek, SeekFrom, Write};
use std::thread;
use std::time::Duration;

fn main() -> Result<(), Box<dyn std::error::Error>> {
    println!("=== ndwk rust binding Demo ===");

    let args: Vec<String> = env::args().collect();
    let wav_path = args.get(1).map(|s| s.as_str()).unwrap_or("test/ja/ja_033.wav");
    let models_dir = args.get(2).map(|s| s.as_str()).unwrap_or("models");

    let samples = load_wav_pcm16(wav_path)?;
    println!(
        "Loaded: {} ({:.2}s, {} samples)",
        wav_path,
        samples.len() as f64 / 16000.0,
        samples.len()
    );

    let mut engine = NdwkEngine::new(
        models_dir,
        NdwkLang::Ja,
        true,
        |text|{
            print!("\r\x1b[32m[Partial(rust)]\x1b[0m: {}\x1b[K", text);
            let _ = io::stdout().flush();
        },
        |lang, text|{
            println!("\r\x1b[33m[Final(rust)]\x1b[0m: [{:?}] {}\x1b[K", lang, text);
        },
    )?;

    println!("Start feeding audio...");

    let chunk_size = 16000; // 1 second of audio at 16kHz
    for chunk in samples.chunks(chunk_size) {
        engine.feed_audio(chunk);
        thread::sleep(Duration::from_millis(50)); // Simulate real-time feeding
    }

    engine.flush();
    println!("Finished feeding audio.");
    Ok(())
}

fn load_wav_pcm16(path: &str) -> io::Result<Vec<f32>> {
    let mut file = File::open(path)?;
    let mut header = [0u8; 12];
    file.read_exact(&mut header)?;

    if &header[0..4] != b"RIFF" || &header[8..12] != b"WAVE" {
        return Err(io::Error::new(io::ErrorKind::InvalidData, "Not a valid WAV file"));
    }

    loop {
        let mut chunk_header = [0u8; 8];
        if file.read_exact(&mut chunk_header).is_err() {
            break;
        }
        let chunk_id = &chunk_header[0..4];
        let chunk_size = u32::from_le_bytes(chunk_header[4..8].try_into().unwrap()) as u64;

        if chunk_id == b"data" {
            let sample_count = (chunk_size / 2) as usize; // 16-bit PCM
            let mut raw_bytes = vec![0u8; chunk_size as usize];
            file.read_exact(&mut raw_bytes)?;

            let mut samples = Vec::with_capacity(sample_count);
            for chunk in raw_bytes.chunks_exact(2) {
                let s = i16::from_le_bytes([chunk[0], chunk[1]]);
                samples.push(s as f32 / 32768.0);
            }
            return Ok(samples);
        } else {
            file.seek(SeekFrom::Current(chunk_size as i64))?;
        }
    }
    Err(io::Error::new(io::ErrorKind::InvalidData, "No data chunk found in WAV file"))
}

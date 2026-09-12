package main

import (
	"encoding/binary"
	"fmt"
	"io"
	"ndwk"
	"os"
	"time"
)

func main() {
	fmt.Println("NDWK Sample (Go)")
	wavPath := "test/ja/ja_033.wav"
	if len(os.Args) > 1 {
		wavPath = os.Args[1]
	}
	modelsDir := "models"
	if len(os.Args) > 2 {
		modelsDir = os.Args[2]
	}

	samples, err := loadWavPCM16(wavPath)
	if err != nil {
		fmt.Fprintf(os.Stderr, "Failed to load WAV: %v\n", err)
		os.Exit(1)
	}
	fmt.Printf("Loaded: %s (%.2fs, %d samples)\n", wavPath, float64(len(samples))/16000.0, len(samples))

	engine, err := ndwk.New(ndwk.Config{
		ModelsDir:   modelsDir,
		Lang:        ndwk.LangJa,
		EnablePunct: true,
		OnPartial: func(text string) {
			fmt.Printf("\r\x1b[32m[Partial(Go)]\x1b[0m: %s\x1b[K", text)
		},
		OnFinal: func(lang ndwk.Lang, text string) {
			fmt.Printf("\r\x1b[33m[Final(Go / %s)]\x1b[0m: %s\x1b[K\n", lang, text)
		},
	})
	if err != nil {
		fmt.Fprintf(os.Stderr, "Engine error: %v\n", err)
		os.Exit(1)
	}
	defer engine.Close()
	fmt.Println("Streaming audio to ndwk...")
	// 3. 疑似ストリーミング投入 (100ms = 1600 サンプルずつ)
	chunkSize := 1600
	for i := 0; i < len(samples); i += chunkSize {
		end := i + chunkSize
		if end > len(samples) {
			end = len(samples)
		}
		engine.FeedAudio(samples[i:end])
		time.Sleep(50 * time.Millisecond)
	}
	// 4. フラッシュ
	engine.Flush()
	fmt.Println("\nDone.")
}

func loadWavPCM16(path string) ([]float32, error) {
	file, err := os.Open(path)
	if err != nil {
		return nil, err
	}
	defer file.Close()
	header := make([]byte, 12)
	if _, err := io.ReadFull(file, header); err != nil {
		return nil, err
	}
	if string(header[0:4]) != "RIFF" || string(header[8:12]) != "WAVE" {
		return nil, fmt.Errorf("not a valid WAV file")
	}
	chunkHeader := make([]byte, 8)
	for {
		if _, err := io.ReadFull(file, chunkHeader); err != nil {
			return nil, fmt.Errorf("data chunk not found")
		}
		chunkID := string(chunkHeader[0:4])
		chunkSize := binary.LittleEndian.Uint32(chunkHeader[4:8])
		if chunkID == "data" {
			sampleCount := int(chunkSize / 2)
			rawBytes := make([]byte, chunkSize)
			if _, err := io.ReadFull(file, rawBytes); err != nil {
				return nil, err
			}
			samples := make([]float32, sampleCount)
			for i := 0; i < sampleCount; i++ {
				s := int16(binary.LittleEndian.Uint16(rawBytes[i*2 : i*2+2]))
				samples[i] = float32(s) / 32768.0
			}
			return samples, nil
		}
		if _, err := file.Seek(int64(chunkSize), io.SeekCurrent); err != nil {
			return nil, err
		}
	}
}

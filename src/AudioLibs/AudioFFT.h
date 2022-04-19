#pragma once

#include "AudioTools/AudioOutput.h"

namespace audio_tools {

// forward declaration
class AudioFFTBase;
MusicalNotes AudioFFTNotes;

/// Result of the FFT
struct AudioFFTResult {
    int bin;
    float magnitude;
    float frequency;

    int frequncyAsInt(){
        return round(frequency);
    }
    const char* frequencyAsNote() {
        return AudioFFTNotes.note(frequncyAsInt());
    }
    const char* frequencyAsNote(int &diff) {
        return AudioFFTNotes.note(frequncyAsInt(), diff);
    }
};

/// Configuration for AudioFFT
struct AudioFFTConfig : public  AudioBaseInfo {
    AudioFFTConfig(){
        channels = 2;
        bits_per_sample = 16;
        sample_rate = 44100;
    }
    /// Callback method which is called after we got a new result
    void (*callback)(AudioFFTBase &fft) = nullptr;
    /// Channel which is used as input
    uint8_t channel_used = 0; 
};

/**
 * @brief Abstract Class which defines the basic FFT functionality 
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class FFTDriver {
    public:
        virtual void begin(int len) =0;
        virtual void end() =0;
        virtual void setValue(int pos, int value) =0;
        virtual void fft() = 0;
        virtual float magnitude(int idx) = 0;
        virtual bool isValid() = 0;
};

/**
 * @brief Executes FFT using audio data. The Driver which is passed in the constructor selects a specifc FFT implementation
 * @author Phil Schatzmann
 * @copyright GPLv3
 */
class AudioFFTBase : public AudioPrint {
    public:
        /// Default Constructor. The len needs to be of the power of 2 (e.g. 512, 1024, 2048, 4096, 8192)
        AudioFFTBase(uint16_t fft_len, FFTDriver* driver){
            len = fft_len;
            p_driver = driver;
        }

        ~AudioFFTBase() {
            end();
        }

        AudioFFTConfig defaultConfig() {
            AudioFFTConfig info;
            return info;
        }

        /// starts the processing
        bool begin(AudioFFTConfig info) {
            cfg = info;
            if (!isPowerOfTwo(len)){
                LOGE("Len must be of the power of 2: %d", len);
                return false;
            }
            p_driver->begin(len);
            current_pos = 0;
            return p_driver->isValid();
        }

        void setAudioInfo(AudioBaseInfo info) override {
            cfg.bits_per_sample = info.bits_per_sample;
            cfg.sample_rate = info.sample_rate;
            cfg.channels = info.channels;
            begin(cfg);
        }

        /// Release the allocated memory
        void end() {
            p_driver->end();
        }

        /// Provide the audio data as FFT input
        size_t write(const uint8_t*data, size_t len) override {
            size_t result = 0;
            if (p_driver->isValid()){
                result = len;
                switch(cfg.bits_per_sample){
                    case 16:
                        processSamples<int16_t>(data, len);
                        break;
                    case 24:
                        processSamples<int24_t>(data, len);
                        break;
                    case 32:
                        processSamples<int32_t>(data, len);
                        break;
                    default:
                        LOGE("Unsupported bits_per_sample: %d",cfg.bits_per_sample);
                        break;
                }
            }
            return result;
        }

        /// We try to fill the buffer at once
        int availableForWrite() {
            return cfg.bits_per_sample/8*len;
        }

        /// The number of bins used by the FFT 
        int size() {
            return len;
        }

        /// time when the last result was provided - you can poll this to check if we have a new result
        unsigned long resultTime() {
            return timestamp;
        }

        /// Determines the frequency of the indicated bin
        float frequency(int bin){
            return static_cast<float>(bin) * cfg.sample_rate / len;
        }

        /// Determines the result values in the max magnitude bin
        AudioFFTResult result() {
            AudioFFTResult ret_value;
            ret_value.magnitude = 0;
            ret_value.bin = 0;
            // find max value and index
            for (int j=1;j<len/2;j++){
                float m = magnitude(j);
                if (m>ret_value.magnitude){
                    ret_value.magnitude = m;
                    ret_value.bin = j;
                }
            }
            ret_value.frequency = frequency(ret_value.bin);
            return ret_value;
        }


        /// Determines the N biggest result values
        template<int N>
        void resultArray(AudioFFTResult (&result)[N]){
            // initialize to negative value
            for (int j=0;j<N;j++){
                result[j].fft = -1000000;
            }
            // find top n values
            AudioFFTResult act;
            for (int j=1;j<len/2;j++){
                act.magnitude = magnitude(j);
                act.bin = j;
                act.frequency = frequency(j);
                insertSorted(result, act);
            }
        }

        FFTDriver *driver() {
            return p_driver;
        }

    protected:
        FFTDriver *p_driver=nullptr;
        int len;
        int current_pos = 0;
        AudioFFTConfig cfg;
        unsigned long timestamp=0l;

        // calculate the magnitued of the fft result to determine the max value
        float magnitude(int idx){
            return p_driver->magnitude(idx);
        }

        // Add samples to input data p_x - and process them if full
        template<typename T>
        void processSamples(const void *data, size_t byteCount) {
            T *dataT = (T*) data;
            int samples = byteCount/sizeof(T);
            for (int j=0; j<samples; j+=cfg.channels){
                p_driver->setValue(current_pos, dataT[j+cfg.channel_used]);
                if (++current_pos>=len){
                    fft();
                }
            }
        }

        void fft() {
            p_driver->fft();
            current_pos = 0;
            timestamp = millis();

            if (cfg.callback!=nullptr){
                cfg.callback(*this);
            }
        }

        /// make sure that we do not reuse already found results
        template<int N>
        bool InsertSorted(AudioFFTResult(&result)[N], AudioFFTResult tmp){
            for (int j=0;j<N;j++){
                if (tmp.magnitude>result[j].magnitude){
                    // shift existing values right
                    for (int i=N-2;i>=j;i--){
                        result[i+1] = result[i];
                    }
                    // insert new value
                    result[j]=tmp;
                }
            }
            return false;
        }

        bool isPowerOfTwo(uint16_t x) {
            return (x & (x - 1)) == 0;
        }

};



}
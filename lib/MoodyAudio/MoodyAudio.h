#ifndef _MOODY_AUDIO_H_
#define _MOODY_AUDIO_H_

#include <PDM.h>
#include <cmath>
#include <cfloat>
#include <climits>

#define CHANNELS 1
#define CHUNK_SIZE 256
#define SAMPLE_RATE 16000
#define SILENCE_RATE    0.8
#define MUSIC_THRESHOLD 2.2
#define SILENCE_CHECK_DURATION 5000

enum AudioType {
    SILENCE = 0,
    NOISE   = 1,
    MUSIC   = 2
};


class ChunkReader {
    protected:
        static volatile int samples;
        static short buffer[CHUNK_SIZE];
        static void receiveCallback();
    public:
        ChunkReader() = default;
        bool begin();
        float chunkRms();
};

volatile int ChunkReader::samples;
short ChunkReader::buffer[CHUNK_SIZE];


void ChunkReader::receiveCallback() {
    int bytesAvailable = PDM.available();
    PDM.read(ChunkReader::buffer, bytesAvailable);
    ChunkReader::samples = bytesAvailable / 2;
}

bool ChunkReader::begin() {
    PDM.setGain(240);
    PDM.onReceive(this->receiveCallback);
    return PDM.begin(CHANNELS, SAMPLE_RATE);
}


float ChunkReader::chunkRms() {
    double rms = 0, op = 0;
    int i;
    for (i = 0; i < ChunkReader::samples; i++) {
        op = (double)ChunkReader::buffer[i] / SHRT_MAX;
        rms += pow(op, 2);
    }
    return 20 * log10(sqrt((double)rms/2));
}


template <int WINDOW_SIZE>
class MoodyAudio : public ChunkReader {
    private:
        bool isReady;
        int index;
        uint8_t zeroEnergy;
        float silenceThreshold;
        float window[WINDOW_SIZE];
    public:
        MoodyAudio();
        void setSilenceThreshold();
        void listen();
        bool ready();
        int analyze();
};


template <int WINDOW_SIZE>
MoodyAudio<WINDOW_SIZE>::MoodyAudio() : ChunkReader(), 
    isReady(false),
    index(0),
    silenceThreshold(-200),
    zeroEnergy(0)
 {}


template <int WINDOW_SIZE>
void MoodyAudio<WINDOW_SIZE>::setSilenceThreshold() {
    Serial.println("Setting silence threshold...");
    // Wait for the transient to finish
    delay(2000);

    unsigned long start = millis();
    while(millis() - start < SILENCE_CHECK_DURATION) {
        if(ChunkReader::samples){
            float bufferRms = this->chunkRms();
            Serial.print("Chunk rms: ");
            Serial.print(bufferRms);
            if(bufferRms >= silenceThreshold) {
                silenceThreshold = bufferRms;
            }
            Serial.print(" - Sthresh: ");
            Serial.println(silenceThreshold);
            ChunkReader::samples = 0;
        }
    }
    Serial.print("Silence Threshold set at: ");
    Serial.print(silenceThreshold);
    Serial.println(" dB");
}

template <int WINDOW_SIZE>
void MoodyAudio<WINDOW_SIZE>::listen() {
    if(samples && !isReady) {
        float rms = chunkRms();
        if (rms < silenceThreshold){
            zeroEnergy++;
        }
        window[index++] = rms;
        if(index == WINDOW_SIZE-1) {
            isReady = true;
        }
        ChunkReader::samples = 0;
    }
}


template <int WINDOW_SIZE>
bool MoodyAudio<WINDOW_SIZE>::ready() {
    return isReady;
}

template <int WINDOW_SIZE>
int MoodyAudio<WINDOW_SIZE>::analyze() {
    int result;
    Serial.print("Zero_frames: ");
    Serial.println(zeroEnergy);
    if(zeroEnergy >= WINDOW_SIZE * SILENCE_RATE){
        result = SILENCE;
    } else {
        float diff = 0, meanRms = 0;
        for(int i=0;i<WINDOW_SIZE-1;i++) {
            diff += abs(window[i+1] - window[i]);
            meanRms +=window[i];
        }
        diff = (float)diff/(WINDOW_SIZE-1);
        meanRms = (meanRms + window[WINDOW_SIZE-1])/WINDOW_SIZE;
        if(diff <= MUSIC_THRESHOLD) {
            result = MUSIC;
        } else {
            result = NOISE;
        }
        Serial.print("Avg rms: ");
        Serial.print(meanRms);
        Serial.print(" - ");
        Serial.print("Chunk_val: ");
        Serial.println(diff);
    }

    isReady = false;
    index = 0;
    zeroEnergy = 0;
    return result;
}


#endif

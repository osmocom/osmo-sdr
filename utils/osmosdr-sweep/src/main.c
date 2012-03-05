#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <math.h>
#include <portaudio.h>
#include "serial.h"
#include "utils.h"

struct SweepState {
	int serialFd;
	int settleTime;
	int sampleTime;
	uint64_t freq;
	double acc;
	double num;
	double start;
	double stop;
};

static int printSyntax()
{
	fprintf(stderr, "Error: Invalid command line!\n\n"
		"syntax: sdr-sweep /dev/ttyUSB0 start stop"
		"  - start = start frequency in MHz, e.g 88.5\n"
		"  - stop = stop frequency in MHz, e.g. 108\n"
		"frequency range of OsmoSDR is 64MHz - 1700MHz\n");

	return EXIT_FAILURE;
}

static void setFreq(int fd, uint64_t freq)
{
	char str[128];
	char* c;
	char str2[2] = { '\0', '\0' };

	while(serialGetS(fd, str, sizeof(str), 1) >= 0)
		;

	sprintf(str, "tuner.freq=%llu\r", freq);

	for(c = str; *c != '\0'; c++) {
		serialPutC(fd, *c);
		if(*c == '\r')
			str2[0] = '\n';
		else str2[0] = *c;
		if(serialExpect(fd, str2, 1) < 0) {
			serialPutC(fd, *c);
			if(serialExpect(fd, str2, 1) < 0) {
				fprintf(stderr, "serial port broken (%d = %c)\n", *c, *c);
				return;
			}
		}
	}
}

static int audioCallback(const void* inputBuffer, void* outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void* userData)
{
	struct SweepState* state = (struct SweepState*)userData;
	int i;
	const int16_t* input = (const int16_t*)inputBuffer;
	uint8_t c;
	double v;
	int max;

	//fprintf(stderr, "block %lu\n", framesPerBuffer);

	while(serialGetC(state->serialFd, &c, 0) >= 0)
		//fprintf(stderr, "%c", c);
		;

	if(state->settleTime > 0) {
		state->settleTime -= framesPerBuffer;
		if(state->settleTime <= 0) {
			state->sampleTime = 100000;
			state->acc = 0.0;
			state->num = 0.0;
		}
		return 0;
	}

	if(state->sampleTime > 0) {
		max = 0;
		for(i = 0; i < framesPerBuffer; i++) {
			if(abs(*input) > max)
				max = abs(*input);
			v = *input / 32768.0;
			state->acc += v * v;
			input++;
			if(abs(*input) > max)
				max = abs(*input);
			v = *input / 32768.0;
			state->acc += v * v;
			input++;
			state->num += 2;
		}
		//fprintf(stderr, "-> %d\n", max);

		state->sampleTime -= framesPerBuffer;
		if(state->sampleTime <= 0) {
			printf("%.1f\t%.1f\n", state->freq / 1000000.0, 20.0 * log10(sqrt(state->acc / state->num)));
			fflush(stdout);
			state->freq += 100000;
			setFreq(state->serialFd, state->freq);
			state->settleTime = 500000;
		}
	}

	return 0;
}

static PaStream* audioOpen(struct SweepState* state)
{
	int numDevices;
	const PaDeviceInfo* deviceInfo;
	PaError err;
	int i;
	int dev = -1;
	PaStream* stream = NULL;
	PaStreamParameters inputParameters;

	err = Pa_Initialize();
	if(err != paNoError)
		goto error_noinit;

	numDevices = Pa_GetDeviceCount();
	if(numDevices < 0) {
		err = numDevices;
		goto error;
	}

	for(i = 0; i < numDevices; i++) {
		deviceInfo = Pa_GetDeviceInfo(i);
		fprintf(stderr, "#%02d -> [%s]\n", i, deviceInfo->name);
		if(strncmp(deviceInfo->name, "OsmoSDR", 7) == 0)
			dev = i;
	}
	if(dev < 0) {
		fprintf(stderr, "OsmoSDR not found!\n");
		Pa_Terminate();
		return NULL;
	}

	fprintf(stderr, "Using device #%02d (sample rate %.0f Hz)\n", dev, Pa_GetDeviceInfo(dev)->defaultSampleRate);

	bzero(&inputParameters, sizeof(inputParameters));
	inputParameters.channelCount = 2;
	inputParameters.device = dev;
	inputParameters.hostApiSpecificStreamInfo = NULL;
	inputParameters.sampleFormat = paInt16;
	inputParameters.suggestedLatency = Pa_GetDeviceInfo(dev)->defaultHighInputLatency ;
	inputParameters.hostApiSpecificStreamInfo = NULL;

	err = Pa_OpenStream(&stream, &inputParameters, NULL, Pa_GetDeviceInfo(dev)->defaultSampleRate, 20000,
		paClipOff | paDitherOff, audioCallback, state);
	if(err != paNoError)
		goto error_noinit;

	err = Pa_StartStream(stream);
	if(err != paNoError)
		goto error_noinit;

	return stream;

error:
	Pa_Terminate();

error_noinit:
	fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
	return NULL;
}

static void audioClose(PaStream* stream)
{
	PaError err;

	err = Pa_StopStream(stream);
	if(err != paNoError)
		fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));

	err = Pa_CloseStream(stream);
	if(err != paNoError)
		fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));

	err = Pa_Terminate();
	if(err != paNoError)
		fprintf(stderr, "PortAudio error: %s\n", Pa_GetErrorText(err));
}

int main(int argc, char* argv[])
{
	HANDLE fd;
	PaStream* stream;
	struct SweepState state;

	if(argc < 4)
		return printSyntax();

	if((fd = serialOpen(argv[1])) == INVALID_HANDLE_VALUE)
		return EXIT_FAILURE;

	memset(&state, 0x00, sizeof(struct SweepState));
	state.serialFd = fd;
	state.settleTime = 100000;
	state.start = atof(argv[2]);
	state.stop = atof(argv[3]);
	state.freq = state.start * 1000000.0;
	serialPutS(fd, "\r\r\rtuner.init!\r\r\r");
	setFreq(fd, state.freq);

	if((stream = audioOpen(&state)) == NULL) {
		serialClose(fd);
		return EXIT_FAILURE;
	}

	fprintf(stderr, "sweep running...\n");

	while(1) {
		sleep(1);
		if(state.freq > state.stop * 1000000.0)
			break;
		fprintf(stderr, "...%.1f MHz...\n", state.freq / 1000000.0);
	}

	audioClose(stream);
	serialClose(fd);

	return EXIT_FAILURE;
}

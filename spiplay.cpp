////////////////////////////////////////////////////////////////
//nakedsoftware.org, spi@oifii.org or stephane.poirier@oifii.org
//
//
//2012juneXX, creation for playing a stereo wav file using portaudio
//nakedsoftware.org, spi@oifii.org or stephane.poirier@oifii.org
////////////////////////////////////////////////////////////////

#include "stdafx.h"

#include <stdio.h>
#include <stdlib.h>
#include "portaudio.h"
#include "pa_asio.h"
#include <map>
#include <string>
using namespace std;

#define BUFF_SIZE	2048

#include <ctime>
#include <iostream>
#include "spiws_WavSet.h"
#include <assert.h>

#include <windows.h>

//Select sample format. 
#if 1
#define PA_SAMPLE_TYPE  paFloat32
typedef float SAMPLE;
#define SAMPLE_SILENCE  (0.0f)
#define PRINTF_S_FORMAT "%.8f"
#elif 1
#define PA_SAMPLE_TYPE  paInt16
typedef short SAMPLE;
#define SAMPLE_SILENCE  (0)
#define PRINTF_S_FORMAT "%d"
#elif 0
#define PA_SAMPLE_TYPE  paInt8
typedef char SAMPLE;
#define SAMPLE_SILENCE  (0)
#define PRINTF_S_FORMAT "%d"
#else
#define PA_SAMPLE_TYPE  paUInt8
typedef unsigned char SAMPLE;
#define SAMPLE_SILENCE  (128)
#define PRINTF_S_FORMAT "%d"
#endif

//The event signaled when the app should be terminated.
HANDLE g_hTerminateEvent = NULL;
//Handles events that would normally terminate a console application. 
BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType);
int Terminate();
PaStream* global_pPaStream;
WavSet* global_pWavSet = NULL;
bool global_stopallstreams = false;

map<string,int> global_devicemap;


// This routine will be called by the PortAudio engine when audio is needed.
// It may called at interrupt level on some machines so don't do anything
// that could mess up the system like calling malloc() or free().
static int patestCallback( const void *inputBuffer, void *outputBuffer,
                           unsigned long framesPerBuffer,
                           const PaStreamCallbackTimeInfo* timeInfo,
                           PaStreamCallbackFlags statusFlags,
                           void *userData )
{
	 if(global_stopallstreams) return 1; //to stop all streams

    //Cast data passed through stream to our structure. 
	WavSet* pWavSet = (WavSet*)userData;//paTestData *data = (paTestData*)userData;
    float *out = (float*)outputBuffer;
    unsigned int i;
    (void) inputBuffer; // Prevent unused variable warning. 

	int idSegmentToPlay = pWavSet->idSegmentSelected+1;
	if(idSegmentToPlay>(pWavSet->numSegments-1)) idSegmentToPlay=0;
#ifdef _DEBUG
	printf("idSegmentToPlay=%d\n",idSegmentToPlay);
#endif //_DEBUG
	float* pSegmentData = pWavSet->GetPointerToSegmentData(idSegmentToPlay);
	assert(pWavSet->numChannels==2);
	for( i=0; i<framesPerBuffer; i++ )
    {
		*out++ = *(pSegmentData+2*i);  // left 
		*out++ = *(pSegmentData+2*i+1);  // right 
        /*
		// Generate simple sawtooth phaser that ranges between -1.0 and 1.0.
        data->left_phase += 0.01f;
        // When signal reaches top, drop back down. 
        if( data->left_phase >= 1.0f ) data->left_phase -= 2.0f;
        // higher pitch so we can distinguish left and right. 
        data->right_phase += 0.03f;
        if( data->right_phase >= 1.0f ) data->right_phase -= 2.0f;
		*/
    }
	pWavSet->idSegmentSelected = idSegmentToPlay;
    return 0;
}

//////
//main
//////
int main(int argc, char *argv[]);
int main(int argc, char *argv[])
{
	int nShowCmd = false;
	ShellExecuteA(NULL, "open", "begin.bat", "", NULL, nShowCmd);

    PaStreamParameters outputParameters;
    //PaStream* stream;
    PaError err;
	//WavSet* pWavSet = NULL;

	///////////////////
	//read in arguments
	///////////////////
	//char charBuffer[2048] = {"testbeat.wav"}; //usage: spiplay testbeat2.wav 10
	char charBuffer[2048] = {"testsound.wav"}; //usage: spiplay testsound.wav 10 0.25
	//char charBuffer[2048] = {"JAVADRUM 3 (Wave 1).WAV"};
	//float fSecondsPlay = 30; //positive for number of seconds to play/loop
	float fSecondsPlay = -1.0; //negative for playing only once
	//double fSecondsPerSegment = 1.0; //non-zero for spliting sample into sub-segments
	//double fSecondsPerSegment = 0.0625; //non-zero for spliting sample into sub-segments
	double fSecondsPerSegment = 0.010; //non-zero for spliting sample into sub-segments
	if(argc>1)
	{
		//first argument is the filename
		sprintf_s(charBuffer,2048-1,argv[1]);
	}
	if(argc>2)
	{
		//second argument is the time it will play
		fSecondsPlay = atof(argv[2]);
	}
	if(argc>3)
	{
		//third argument is the segment length in seconds
		fSecondsPerSegment = atof(argv[3]);
	}
	//use audio_spi\spidevicesselect.exe to find the name of your devices, only exact name will be matched (name as detected by spidevicesselect.exe)  
	//string audiodevicename="E-MU ASIO"; //"Speakers (2- E-MU E-DSP Audio Processor (WDM))"
	string audiodevicename="Speakers (2- E-MU E-DSP Audio P"; //"E-MU ASIO"
	if(argc>4)
	{
		audiodevicename = argv[4]; //for spi, device name could be "E-MU ASIO", "Speakers (2- E-MU E-DSP Audio Processor (WDM))", etc.
	}
    int outputAudioChannelSelectors[2]; //int outputChannelSelectors[1];
	
	outputAudioChannelSelectors[0] = 0; // on emu patchmix ASIO device channel 1 (left)
	outputAudioChannelSelectors[1] = 1; // on emu patchmix ASIO device channel 2 (right)
	
	
	/*
	outputAudioChannelSelectors[0] = 2; // on emu patchmix ASIO device channel 3 (left)
	outputAudioChannelSelectors[1] = 3; // on emu patchmix ASIO device channel 4 (right)
	*/

	/*
	outputAudioChannelSelectors[0] = 4; // on emu patchmix ASIO device channel 5 (left)
	outputAudioChannelSelectors[1] = 5; // on emu patchmix ASIO device channel 6 (right)
	*/
	/*
	outputAudioChannelSelectors[0] = 6; // on emu patchmix ASIO device channel 7 (left)
	outputAudioChannelSelectors[1] = 7; // on emu patchmix ASIO device channel 8 (right)
	*/
	if(argc>5)
	{
		outputAudioChannelSelectors[0]=atoi(argv[5]); //0 for first asio channel (left) or 2, 4, 6 and 8 for spi (maxed out at 10 asio output channel)
	}
	if(argc>6)
	{
		outputAudioChannelSelectors[1]=atoi(argv[6]); //1 for second asio channel (right) or 3, 5, 7 and 9 for spi (maxed out at 10 asio output channel)
	}

    //Auto-reset, initially non-signaled event 
    g_hTerminateEvent = ::CreateEvent(NULL, FALSE, FALSE, NULL);
    //Add the break handler
    ::SetConsoleCtrlHandler(ConsoleCtrlHandler, TRUE);

	/////////////////
	//read a WAV file 
	/////////////////
	global_pWavSet = new WavSet;
	global_pWavSet->ReadWavFile(charBuffer);
	if(global_pWavSet->numChannels!=2)
	{
		printf("exiting because sample is mono\n");
		delete global_pWavSet;
		exit(0);
	}

	
	///////////////////////
	// Initialize portaudio 
	///////////////////////
    err = Pa_Initialize();
    if( err != paNoError ) 
	{
		//goto error;
		Pa_Terminate();
		fprintf( stderr, "An error occured while using the portaudio stream\n" );
		fprintf( stderr, "Error number: %d\n", err );
		fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
		return Terminate();
	}

	////////////////////////
	//audio device selection
	////////////////////////
	const PaDeviceInfo* deviceInfo;
    int numDevices = Pa_GetDeviceCount();
    for( int i=0; i<numDevices; i++ )
    {
        deviceInfo = Pa_GetDeviceInfo( i );
		string devicenamestring = deviceInfo->name;
		global_devicemap.insert(pair<string,int>(devicenamestring,i));
	}

	int deviceid = Pa_GetDefaultOutputDevice(); // default output device 
	map<string,int>::iterator it;
	it = global_devicemap.find(audiodevicename);
	if(it!=global_devicemap.end())
	{
		deviceid = (*it).second;
		printf("%s maps to %d\n", audiodevicename.c_str(), deviceid);
		deviceInfo = Pa_GetDeviceInfo(deviceid);
		//deviceInfo->maxInputChannels
		assert(outputAudioChannelSelectors[0]<deviceInfo->maxOutputChannels);
		assert(outputAudioChannelSelectors[1]<deviceInfo->maxOutputChannels);
	}
	else
	{
		for(it=global_devicemap.begin(); it!=global_devicemap.end(); it++)
		{
			printf("%s maps to %d\n", (*it).first.c_str(), (*it).second);
		}
		//Pa_Terminate();
		//return -1;
		printf("error, audio device not found, will use default\n");
		deviceid = Pa_GetDefaultOutputDevice();
	}


	//outputParameters.device = Pa_GetDefaultOutputDevice(); // default output device 
	outputParameters.device = deviceid; 
	if (outputParameters.device == paNoDevice) 
	{
		fprintf(stderr,"Error: No default output device.\n");
		//goto error;
		Pa_Terminate();
		fprintf( stderr, "An error occured while using the portaudio stream\n" );
		fprintf( stderr, "Error number: %d\n", err );
		fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
		return Terminate();
	}
	outputParameters.channelCount = global_pWavSet->numChannels;
	outputParameters.sampleFormat =  PA_SAMPLE_TYPE;
	outputParameters.suggestedLatency = Pa_GetDeviceInfo( outputParameters.device )->defaultLowOutputLatency;
	//outputParameters.hostApiSpecificStreamInfo = NULL;

	//Use an ASIO specific structure. WARNING - this is not portable. 
	PaAsioStreamInfo asioOutputInfo;
	asioOutputInfo.size = sizeof(PaAsioStreamInfo);
	asioOutputInfo.hostApiType = paASIO;
	asioOutputInfo.version = 1;
	asioOutputInfo.flags = paAsioUseChannelSelectors;
	asioOutputInfo.channelSelectors = outputAudioChannelSelectors;
	//outputChannelSelectors[0] = 0; // ASIO device channel 1 (left)
	//outputChannelSelectors[1] = 1; // ASIO device channel 2 (right)
	if(deviceid==Pa_GetDefaultOutputDevice())
	{
		outputParameters.hostApiSpecificStreamInfo = NULL;
	}
	else if(Pa_GetHostApiInfo(Pa_GetDeviceInfo(deviceid)->hostApi)->type == paASIO) 
	{
		outputParameters.hostApiSpecificStreamInfo = &asioOutputInfo;
	}
	else if(Pa_GetHostApiInfo(Pa_GetDeviceInfo(deviceid)->hostApi)->type == paWDMKS) 
	{
		/*
		//Use an WDMKS specific structure. WARNING - this is not portable. 
		PaWDMKSStreamInfo wdmksOutputInfo;
		asioOutputInfo.size = sizeof(PaWDMKSStreamInfo);
		asioOutputInfo.hostApiType = paWDMKS;
		asioOutputInfo.version = 1;
		asioOutputInfo.flags = paAsioUseChannelSelectors;
		//outputChannelSelectors[0] = 0; // ASIO device channel 1 (left)
		//outputChannelSelectors[1] = 1; // ASIO device channel 2 (right)
		outputParameters.hostApiSpecificStreamInfo = &wdmksOutputInfo;
		*/
		outputParameters.hostApiSpecificStreamInfo = NULL;
	}
	else
	{
		//assert(false);
		outputParameters.hostApiSpecificStreamInfo = NULL;
	}


#ifdef _DEBUG //debug: playback the loaded sample file (as is)
	if(0)
	{
		printf("Begin playback.\n"); fflush(stdout);
		err = Pa_OpenStream(
					&global_pPaStream,
					NULL, // no input
					&outputParameters,
					global_pWavSet->SampleRate,
					BUFF_SIZE/global_pWavSet->numChannels, //FRAMES_PER_BUFFER,
					paClipOff,      // we won't output out of range samples so don't bother clipping them 
					NULL, // no callback, use blocking API 
					NULL ); // no callback, so no callback userData 
		if( err != paNoError ) goto error;

		if( global_pPaStream )
		{
			err = Pa_StartStream( global_pPaStream );
			if( err != paNoError ) goto error;
			printf("Waiting for playback to finish.\n"); fflush(stdout);

			err = Pa_WriteStream( global_pPaStream, global_pWavSet->pSamples, global_pWavSet->totalFrames );
			if( err != paNoError ) goto error;

			err = Pa_CloseStream( global_pPaStream );
			if( err != paNoError ) goto error;
			printf("Done.\n"); fflush(stdout);
		}
	}
#endif //_DEBUG

	

	///////////////////////////
	// split WavSet in segments 
	///////////////////////////
	global_pWavSet->SplitInSegments(fSecondsPerSegment);
#ifdef _DEBUG
	printf("numSegments=%d\n",global_pWavSet->numSegments);
#endif //_DEBUG

	//////////////////////////
	//initialize random number
	//////////////////////////
	srand((unsigned)time(0));
	
	///////////////////////////////////////////////////////////////////
	//play WavSet's segments sequentially using portaudio with callback
	///////////////////////////////////////////////////////////////////
	err = Pa_OpenStream(
				&global_pPaStream,
				NULL, // no input
				&outputParameters,
				global_pWavSet->SampleRate,
				global_pWavSet->numSamplesPerSegment/global_pWavSet->numChannels, //BUFF_SIZE/pWavSet->numChannels, //FRAMES_PER_BUFFER,
				paClipOff,      // we won't output out of range samples so don't bother clipping them 
				patestCallback, // no callback, use blocking API 
				global_pWavSet ); // no callback, so no callback userData 
	if( err != paNoError ) goto error;

    err = Pa_StartStream( global_pPaStream );
    if( err != paNoError ) goto error;

	if(fSecondsPlay>0.0)
	{
		//Sleep for fSecondsPlay seconds. 
		Pa_Sleep(fSecondsPlay*1000);
	}
	else
	{
		//Sleep for wavfile's length
		float fSegmentsLength = global_pWavSet->GetSegmentsLength();
		printf("sleep for %f sec\n", fSegmentsLength);
		Pa_Sleep(fSegmentsLength*1000);
	}

	return Terminate();

error:
    Pa_Terminate();
    fprintf( stderr, "An error occured while using the portaudio stream\n" );
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
    return -1;

}

int Terminate()
{
	/*
	//terminate all playing streams
	global_stopallstreams=true;
	Sleep(1000); //would have to wait at least fSecondsPerSegment
	*/
	//terminate the only playing stream
    PaError err = Pa_StopStream( global_pPaStream );
    if( err != paNoError ) goto error;


	if( global_pPaStream )
	{
		err = Pa_CloseStream( global_pPaStream );
		if( err != paNoError ) goto error;
		printf("Done.\n"); fflush(stdout);
	}
	Pa_Terminate();
	if(global_pWavSet) delete global_pWavSet;
	printf("Exiting!\n"); fflush(stdout);

	int nShowCmd = false;
	ShellExecuteA(NULL, "open", "end.bat", "", NULL, nShowCmd);
	return 0;
error:
    Pa_Terminate();
    fprintf( stderr, "An error occured while using the portaudio stream\n" );
    fprintf( stderr, "Error number: %d\n", err );
    fprintf( stderr, "Error message: %s\n", Pa_GetErrorText( err ) );
	return -1;
}

//Called by the operating system in a separate thread to handle an app-terminating event. 
BOOL WINAPI ConsoleCtrlHandler(DWORD dwCtrlType)
{
    if (dwCtrlType == CTRL_C_EVENT ||
        dwCtrlType == CTRL_BREAK_EVENT ||
        dwCtrlType == CTRL_CLOSE_EVENT)
    {
        // CTRL_C_EVENT - Ctrl+C was pressed 
        // CTRL_BREAK_EVENT - Ctrl+Break was pressed 
        // CTRL_CLOSE_EVENT - Console window was closed 
		Terminate();
        // Tell the main thread to exit the app 
        ::SetEvent(g_hTerminateEvent);
        return TRUE;
    }

    //Not an event handled by this function.
    //The only events that should be able to
	//reach this line of code are events that
    //should only be sent to services. 
    return FALSE;
}

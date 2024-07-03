#ifndef SOUNDMANAGER_H
#define SOUNDMANAGER_H

#include <SDL.h>
#include <vector>
#include <mutex>

class SoundManager {
public:
	~SoundManager();
	void Initialize();
	void BeginPlay();
	void StopPlay();
	void SoundOn();
	void SoundOff();
	
	// public singleton code
	static SoundManager* GetInstance()
	{
		if (NULL == s_instance)
			s_instance = new SoundManager(44100, 100);
		return s_instance;
	}
private:
	static SoundManager* s_instance;
	SoundManager(int sampleRate, int bufferSize);
	
	static void AudioCallback(void* userdata, Uint8* stream, int len);
	void GenerateTone(int len, Uint8* stream);
	void GenerateSilence(int len, Uint8* stream);
	
	std::mutex bufferMutex;
	SDL_AudioSpec audioSpec;
	SDL_AudioDeviceID audioDevice;
	int sampleRate;
	int bufferSize;
	bool isDevicePaused;
	bool isPlaying;
	int phase;
};

#endif // SOUNDMANAGER_H

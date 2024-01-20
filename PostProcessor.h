#pragma once
#ifndef POSTPROCESSOR_H
#define POSTPROCESSOR_H

/*
	Singleton postprocessing class whose job is to:
		-remember if there needs to be postprocessing
		- decide which shaders need to be used for postprocessing and in which order
		- provide the necessary uniforms(parameters) to the shaders


*/

#include "common.h"

class PostProcessor
{
public:
	// public singleton code
	static PostProcessor* GetInstance()
	{
		if (NULL == s_instance)
			s_instance = new PostProcessor();
		return s_instance;
	}
	~PostProcessor();

	void Render();
	bool IsEnabled() { return false; };	// TODO
private:
	//////////////////////////////////////////////////////////////////////////
	// Singleton pattern
	//////////////////////////////////////////////////////////////////////////
	void Initialize();

	static PostProcessor* s_instance;
	PostProcessor()
	{
		Initialize();
	}
};

#endif	// POSTPROCESSOR_H
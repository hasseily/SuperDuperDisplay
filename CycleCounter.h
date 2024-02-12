#pragma once

#ifndef CYCLECOUNTER_H
#define CYCLECOUNTER_H

#include <stdint.h>
#include <stddef.h>

class CycleCounter
{
public:
	void IncrementCycles(int inc);
	bool IsNotInBlank();
	bool IsVBL();
	bool IsHBL();
	uint8_t GetScanline();
	uint8_t GetByteYPos();


	// public singleton code
	static CycleCounter* GetInstance()
	{
		if (NULL == s_instance)
			s_instance = new CycleCounter();
		return s_instance;
	}
private:
//////////////////////////////////////////////////////////////////////////
// Singleton pattern
//////////////////////////////////////////////////////////////////////////
	void Initialize();

	static CycleCounter* s_instance;
	CycleCounter()
	{
		Initialize();
	}

	uint32_t m_cycle;	// Current cycle
};

#endif // CYCLECOUNTER_H
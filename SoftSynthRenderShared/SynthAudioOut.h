#pragma once

#include "SynthCadDefines.h"
#include "SynthObject.h"
#include "SynthDataPath.h"
#include "SynthParameters.h"



class CSynthAudioOut:public CSynthObject
{
	//data input
	CSynthDataPath *m_pIN;
	float m_Data;
public:
	CSynthAudioOut(CSynthParameters *pParams, CSynthObject *pParent);
	virtual ~CSynthAudioOut();
	virtual bool Create( CSynthObject *pParent);
	virtual void Run();
	void Open();
	void Close();
	//----------------------------------
	// Getter Functions
	//----------------------------------
	inline void SetDataInput(CSynthDataPath *pD) { m_pIN = pD; }
	inline float GetData(void) { return m_Data; }
	void RunSynthEngine(short *pB);
};


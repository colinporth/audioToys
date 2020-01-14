#pragma once

#include "SynthParameters.h"
#include "SynthObject.h"
#include "SynthPolyBlepOsc.h"
#include "SynthMidiIN.h"
#include "SynthAudioOut.h"
#include "SynthEGadsr.h"
#include "SynthVCA.h"
#include "SynthSVFilter.h"
#include "SynthMixer.h"

class CSynthEngine :public CSynthObject
{
	HANDLE m_hThread;
	DWORD m_ThreadID;
public:
	CSynthAudioOut *m_pAudioOut;	//audio outpupt
	CSynthDataPath *m_pOutPatch;	//VCA->output
	CSynthMidiIN *m_pMidiIn;		//midi input
	CSynthPolyBLEPOsc *m_pOsc1;		//VCO	
	CSynthDataPath *m_pPatch1;		//patch VCO->VCF
	CSynthDataPath *m_pOscPitch;	//VCO pitch
	CSynthDataPath *m_pOscWaveSel;	//VCO wave select
	CSynthDataPath *m_pPWM;			//VCO pwm
	CSynthEGadsr *m_pEnv;			//ADSR
	CSynthDataPath *m_pEVOut;		//ADSR Output
	CSynthDataPath *m_pA;			//ADSR Attack
	CSynthDataPath *m_pD;			//ADSR Decay
	CSynthDataPath *m_pS;			//ADSR Sustain
	CSynthDataPath *m_pR;			//ADSR Release
	CSynthDataPath *m_pGate;		//ADSR Trigger
	CSynthVCA *m_pVca;				//VCA
	CSynthDataPath *m_pVCA;			//Patch VCF->VCA
	CSynthSVFilter *m_pFilt;		//VCF
	CSynthDataPath *m_pFCmix;		//patch Mixer->VCF fc
	CSynthDataPath *m_pQ;			//VCF Q
	CSynthDataPath *m_pF;			//VCF Fc
	CSynthMixer *m_pFiltFreqMix;	//VCF freq summer
	CSynthDataPath *m_pMixEVLevel;	//Envelope level control EV->VCF 
	CSynthDataPath *m_pMixFcLevel;	//level control for FC control

	static UINT SynthEngWorker(LPVOID pD);
public:
	//---------------------------------------
	// Getter Functions
	//---------------------------------------
	inline CSynthPolyBLEPOsc *GetOsc1() { return m_pOsc1; }
	//---------------------------------------
	// Implementation
	//---------------------------------------
	CSynthEngine(CSynthParameters *pParams, CSynthObject *pParent);
	~CSynthEngine();
	virtual bool Create( CSynthObject *pParent);
	//----------------------------------------
	// sound generating methods
	//----------------------------------------
	float RunIt();
	void GenerateSamples(BYTE *buff, int nSamples,int ch, DWORD freq);
	//-------------------------------------------
	// Implementation
	//-----------------------------------------
	void Init();
	void Stop();
	//--------------------------------------------
	// Messaging
	//--------------------------------------------
	void BeginThread();
	void TerminateThread();
	DWORD GetID() { return m_ThreadID; }
};


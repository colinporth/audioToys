//{{{  includes
#include "stdafx.h"

#include "SynthObject.h"
#include "SynthEngine.h"
#include "SynthAudioOut.h"

#include <functiondiscoverykeys.h>
//}}}

//{{{
UINT CSynthEngine::SynthEngWorker(LPVOID pD)
{
	//------------------------------------
	// This message handler is used for
	// dispatching messages among the various
	// modules, until I can come up with a
	// better way to do it.
	//------------------------------------
	CSynthEngine *pSE = (CSynthEngine *)pD;
	printf("Snth Engine Thread Started\n");
	int loop = 1;
	while (loop)
	{
		MSG message;

		GetMessage(&message, NULL, 0, 0);
		switch (message.message)
		{
			case MSG_KILLTHREAD:
				loop = 0;
				break;
			case WM_LK25_KNOB_1:  //Attack
				pSE->m_pA->SetData(NoteToBiLevel(message.lParam));
				break;
			case WM_LK25_KNOB_2:  //decay
				pSE->m_pD->SetData(NoteToBiLevel(message.lParam));
				break;
			case WM_LK25_KNOB_3:  //sustain
				printf("Knob 1 %d  %d\n", message.wParam, message.lParam);
				pSE->m_pS->SetData(NoteToLevel(message.lParam));
				break;
			case WM_LK25_KNOB_4:  //release
				pSE->m_pR->SetData(NoteToBiLevel(message.lParam));
				break;
			case WM_LK25_KNOB_5:  //filter Q
				pSE->m_pQ->SetData(NoteToLevel(message.lParam));
				break;
			case WM_LK25_KNOB_6:  //filter freq
				pSE->m_pF->SetData(NoteToLevel(message.lParam));
				break;
			case WM_LK25_KNOB_7:  //Osc PWM
				pSE->m_pPWM->SetData(NoteToLevel(message.lParam));
				break;
			case WM_LK25_KNOB_8:  //fiter ADSR atn
				pSE->m_pMixEVLevel->SetData(NoteToLevel(message.lParam));
				break;
			case WM_NOTE_ON:
				pSE->m_pOscPitch->SetData(NoteToLevel(message.wParam));
//        printf("Note On %d %d\n", message.wParam, message.lParam);
				break;
			case WM_NOTE_OFF:
//        printf("Note Off %d %d\n", message.wParam, message.lParam);
				break;
			case WM_ADSR_GATE:
				if (message.wParam)
					pSE->m_pGate->SetData((float)1.0);
				else
					pSE->m_pGate->SetData((float)0.0);
				break;
		}

	}
	return 0;
}
//}}}

CSynthEngine::CSynthEngine(CSynthParameters *pParams, CSynthObject *pParent) : 
	CSynthObject(OBJECT_TYPE_ENGINE, pParams,pParent) {}
CSynthEngine::~CSynthEngine() {}

//{{{
float CSynthEngine::RunIt()
{
	//------------------------------------
	// RunIt
	//    Generates audio output
	//------------------------------------
	float d=0.0;

	CSynthObject *pSO = GetHead();
	while (pSO)
	{
		if (OBJECT_TYPE_AUDIO == pSO->GetType())
		{
			pSO->Run();
			d = ((CSynthAudioOut *)pSO)->GetData();
		}
		else
			pSO->Run();
		pSO = pSO->GetNext();
	}
	return d;
}
//}}}

//{{{
void CSynthEngine::GenerateSamples(BYTE *buff,int nSamples,int nCh, DWORD /*freq*/)
{
	float *databuff = (float *)buff;
	float d;
	int i, index,j;

	for (i = 0, index = 0; i < nSamples; ++i)
	{
		d = RunIt();
		for (j = 0; j < nCh; ++j)
			databuff[index++] = d;
	}
}
//}}}

//{{{
bool CSynthEngine::Create( CSynthObject *pParent)
{
	Init();
	return CSynthObject::Create(pParent);
}
//}}}
//{{{
void CSynthEngine::Init()
{
	//---------------------------------
	// Create the synth here.
	//----------------------------------
	// Audio Output
	m_pAudioOut = new CSynthAudioOut(GetParams(), this);
	m_pAudioOut->Create(this);
	// Midi Input
	m_pMidiIn = new CSynthMidiIN(GetParams(), this);
	m_pMidiIn->Create( this);

	m_pPatch1 = new CSynthDataPath(GetParams(), this);
	m_pPatch1->Create(this);
	AddObject(m_pPatch1);
	//VCO
	m_pOsc1 = new CSynthPolyBLEPOsc(GetParams(), this);
	m_pOsc1->Create(this);
	AddObject(m_pOsc1);
	m_pOscPitch = new CSynthDataPath(GetParams(), this);
	m_pOscPitch->Create(this);
	m_pOscWaveSel = new CSynthDataPath(GetParams(), this);
	m_pOscWaveSel->Create(this);
	m_pPWM = new CSynthDataPath(GetParams(), this);
	//ADSR
	m_pEnv = new CSynthEGadsr(GetParams(), this);
	m_pEnv->Create(this);
	AddObject(m_pEnv);
	m_pA = new CSynthDataPath(GetParams(), this);
	m_pA->Create(this);
	m_pD = new CSynthDataPath(GetParams(), this);
	m_pD->Create(this);
	m_pS = new CSynthDataPath(GetParams(), this);
	m_pS->Create(this);
	m_pR = new CSynthDataPath(GetParams(), this);
	m_pR->Create(this);
	m_pGate = new CSynthDataPath(GetParams(), this);
	m_pGate->Create(this);
	m_pEVOut = new CSynthDataPath(GetParams(), this);
	m_pEVOut->Create(this);
	//Filter Fc Mixer
	m_pFiltFreqMix = new CSynthMixer(2,GetParams(), this);
	m_pFiltFreqMix->Create(this);
	AddObject(m_pFiltFreqMix);
	m_pFCmix = new CSynthDataPath(GetParams(), this);
	m_pFCmix->Create(this);
	m_pMixEVLevel = new CSynthDataPath(GetParams(), this);
	m_pF = new CSynthDataPath(GetParams(), this);
	m_pMixFcLevel = new CSynthDataPath(GetParams(), this);
	// VCF
	m_pFilt = new CSynthSVFilter(GetParams(), this);
	m_pFilt->Create(this);
	AddObject(m_pFilt);
	m_pQ = new CSynthDataPath(GetParams(), this);
	m_pVCA = new CSynthDataPath(GetParams(), this);
	//VCA
	m_pVca = new CSynthVCA(GetParams(), this);
	m_pVca->Create(this);
	m_pOutPatch = new CSynthDataPath(GetParams(), this);
	AddObject(m_pVca);
	AddObject(m_pAudioOut);

	//-----------------------------
	// Connect the objects together
	//-----------------------------
	m_pOsc1->SetOutput(m_pPatch1);
	m_pOsc1->SetPitch(m_pOscPitch);
	m_pOscPitch->SetData(FreqToLevel(440));
	m_pOsc1->SetWaveSel(m_pOscWaveSel);
	m_pOscWaveSel->SetData((float)0.8); //sine wave
	m_pOsc1->SetPW(m_pPWM);
	//envelope generator
	m_pEnv->SetAttack(m_pA);
	m_pEnv->SetDecay(m_pD);
	m_pEnv->SetSustain(m_pS);
	m_pEnv->SetRelease(m_pR);
	m_pEnv->SetTrigger(m_pGate);
	m_pEnv->SetOut(m_pEVOut);
	//filter
	m_pFilt->SetIn(m_pPatch1);
	m_pFilt->SetFc(m_pFCmix);
	m_pFilt->SetQ(m_pQ);
	m_pFilt->SetBPo(m_pVCA);
	//mixer
	m_pFiltFreqMix->SetIn(0, m_pF);
	m_pFiltFreqMix->SetIn(1, m_pEVOut);
	m_pFiltFreqMix->SetControl(0, m_pMixFcLevel);
	m_pMixFcLevel->SetData((float)0.8);
	m_pFiltFreqMix->SetControl(1, m_pMixEVLevel);
	m_pFiltFreqMix->SetOut(m_pFCmix);
	//VCA
	m_pVca->SetIN(m_pVCA);
	m_pVca->SetControl(m_pEVOut);
	m_pVca->SetOut(m_pOutPatch);
	m_pAudioOut->SetDataInput(m_pOutPatch);
	//-----------------------------
	// Start the synth
	//-----------------------------
	BeginThread();
}
//}}}

//{{{
void CSynthEngine::Stop()
{
//  m_pAudio->TerminateThread();
	m_pMidiIn->KillThead();
	TerminateThread();
}
//}}}

//{{{
void CSynthEngine::BeginThread()
{
	m_hThread = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)CSynthEngine::SynthEngWorker, (LPVOID)this, 0, NULL);
	m_ThreadID = GetThreadId(m_hThread);
}
//}}}
//{{{
void CSynthEngine::TerminateThread()
{
	PostThreadMessageW(GetThreadId(m_hThread), MSG_KILLTHREAD, 0, 0);
	if (m_hThread)
	{
		WaitForSingleObject(m_hThread, INFINITE);

		CloseHandle(m_hThread);
		m_hThread = NULL;
	}

//  m_pSynthThread->PostThreadMessageW(MSG_KILLTHREAD, 0, 0);
//  Sleep(1);
//  DWORD dwExitCode;
//  do
//  {
//    ::GetExitCodeThread(m_pSynthThread->m_hThread, &dwExitCode);
//    //--------------------------------------------------------
//    // in order fot the exit code to change, we need to force
//    // a context switch.  We do this by going to sleep for
//    // zero milliseconds...I personally find this to be
//    // klugey, but this is windows we are talking about here
//    //--------------------------------------------------------
//    Sleep(0L);
//  } while (dwExitCode);
//  if (dwExitCode == 0)  // Is The thread is still running.
//  {
//    delete m_pSynthThread;
//  }

}
//}}}

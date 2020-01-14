//{{{  incl;udes
#include "stdafx.h"
#include "SynthCadDefines.h"
#include <mmsystem.h>
#include <mmsyscom.h>
#include "SynthAudioOut.h"
#include "SynthEngine.h"
#include <avrt.h>
//}}}

//{{{
CSynthAudioOut::CSynthAudioOut(CSynthParameters *pParams,CSynthObject *pParent):CSynthObject(OBJECT_TYPE_AUDIO,pParams,pParent)
{
	m_pIN = 0;
}
//}}}
CSynthAudioOut::~CSynthAudioOut() {}

void CSynthAudioOut::Open() {}
void CSynthAudioOut::Close() {}

void CSynthAudioOut::Run() {
	if (m_pIN) {
		float d = m_pIN->GetData();
		if (d > 1.0) d = 1.0;
		else if (d < -1.0) d = -1.0;
		m_Data = d;
		}
	}

void CSynthAudioOut::RunSynthEngine(short * pBuf) {
	CSynthEngine *pENG = (CSynthEngine *)GetParent();
	int i, n;
	//  m_CurrentBuffer = pBuf; //BAD!
	n = GetParams()->GetSamplesPerBlock();
	for (i = 0; i < n; ++i) {
		pENG->Run();  //run one sysnth cycle n times
		pBuf[i] = (short)GetData();
		}
	}

bool CSynthAudioOut::Create( CSynthObject *pParent) {
	return CSynthObject::Create( pParent);
	}

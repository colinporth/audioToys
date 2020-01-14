///-------------------------------------
/// Mixer Module
/// Can also be used to invert signals
/// and as a multiple VCA
///--------------------------------------
#include "stdafx.h"
#include "SynthMixer.h"

CSynthMixer::CSynthMixer(int nChannels,CSynthParameters *params, CSynthObject *pParent):CSynthObject(OBJECT_TYPE_MIXER,params,pParent)
{
	m_nChannels = nChannels;
	m_ppIn = new CSynthDataPath *[m_nChannels];
	m_ppControl = new CSynthDataPath *[m_nChannels];
}

CSynthMixer::~CSynthMixer()
{
	delete[] m_ppControl;
	delete[] m_ppIn;
}

void CSynthMixer::Run()
{
	int i;
	float out=0.0;

	for(i=0;i<m_nChannels;++i)
	{
		out += m_ppIn[i]->GetData() * m_ppControl[i]->GetData();
	}
	///------------------------
	/// clip
	///-----------------------
	if(out > 1.0) out = 1.0;
	else if(out < -1.0) out = -1.0;
	//--------------------------
	// Send ot next module
	//---------------------------
	m_pOut->SetData(out);
}

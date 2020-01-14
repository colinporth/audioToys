#include "stdafx.h"
#include "SynthCadDefines.h"
#include "SynthSVFilter.h"

CSynthSVFilter::CSynthSVFilter(CSynthParameters *pParams, CSynthObject *pParent):CSynthObject(OBJECT_TYPE_SVFILTER,pParams,pParent)
{
	m_Z1 = 0.0;
	m_Z2 = 0.0;
	m_pIn = 0;
	m_pHPo = 0;
	m_pBPo = 0;
	m_pLPo = 0;
	m_pFc = 0;
	m_pQ = 0;
}

CSynthSVFilter::~CSynthSVFilter()
{
}

void CSynthSVFilter::Run()
{
	///--------------------------------------------
	/// Run
	///		Implements the Hal Chamberlin State
	///	variable digital filter.
	/// Page 489 of Musical Applications of Micro Processors
	///---------------------------------------------
	float tZ1,tZ2;
	float Fc,Q;
	float FiltIn;
	float Hp;

	FiltIn = m_pIn->GetData();
	Fc = LevelToFreq(m_pFc->GetData());
	Fc = this->FreqToFc(Fc);
	Q = m_pQ->GetData();

	tZ2 = Integrator(Fc,m_Z2,m_Z1);
	Hp = Q * FiltIn - Q * m_Z1 - m_Z2;
	tZ1 = Integrator(Fc,m_Z1,Hp);
	m_Z1 = tZ1;
	m_Z2 = tZ2;

	if(m_pHPo)
		m_pHPo->SetData(Hp);
	if(m_pBPo)
		m_pBPo->SetData(m_Z1);
	if(m_pLPo)
		m_pLPo->SetData(m_Z2);
}

inline float CSynthSVFilter::Integrator(float fc, float Z, float In)
{
	float rV = fc * In + Z;
	if (rV > 1.0) rV = 1.0;
	else if (rV < -1.0) rV = -1.0;
	return rV;
}

float CSynthSVFilter::FreqToFc(float freq)
{
	///---------------------------------------------
	/// FreqToFc
	///		Function converts frequency to a control
	/// value which is always POSITIVE.  Negative
	/// values will do BAD things
	///---------------------------------------------
	float Fc;

	Fc =(float)( twoPI * freq/this->GetParams()->GetSampleRate());
	return Fc;
}

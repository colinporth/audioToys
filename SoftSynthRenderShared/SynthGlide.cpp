#include "stdafx.h"
#include "SynthGlide.h"
#include <math.h>

CSynthGlide::CSynthGlide(CSynthParameters *params, CSynthObject *pParent):CSynthObject(OBJECT_TYPE_GLIDE,params,pParent)
{
	m_pIn = 0;
	m_pOut = 0;
	m_pDownRate = 0;
	m_pUpRate = 0;
	m_Goal = 0.0;
	m_Z = 0.0;
}

CSynthGlide::~CSynthGlide()
{
}

void CSynthGlide::Run(void)
{
	///-----------------------------------------
	/// Run
	///		Generate a linear (slew) glide
	///-----------------------------------------
	m_Goal = m_pIn->GetData();
	if(m_Goal < m_Z)	//glide down
	{
		m_Z -= ConvertRate(m_pDownRate->GetData()) ;
		if(m_Z < m_Goal)
			m_Z = m_Goal;
	}
	else if(m_Goal > m_Z)	//glade up
	{
		m_Z += ConvertRate(m_pUpRate->GetData()) ;
		if(m_Z > m_Goal)
			m_Z = m_Goal;

	}
	m_pOut->SetData(m_Z);
}

float CSynthGlide::ConvertRate(float v)
{
	//--------------------------------------------
	// ConvertRate
	//	Control signals range in value from -1.0 to +1.0
	// so we must convert those values into some sort of
	// usable increment for the glide.
	// Negative values are the slowest,
	// Positive values are the fastest.
	// Rates will be controllable over 20 "octaves"
	//--------------------------------------------
	float incr;
	float k;
	k = (float) pow(TWELTHROOT2,v/0.1);
	incr = k / 10240;
	return incr;
}


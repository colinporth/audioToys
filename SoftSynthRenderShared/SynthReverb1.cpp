#include "stdafx.h"
#include "SynthReverb1.h"

CSynthReverb1::CSynthReverb1(CSynthParameters *pParams,float Delay, CSynthObject *pParent):CSynthObject(OBJECT_TYPE_REVERB1,pParams,pParent)
{
	///-----------------------------------------------
	/// CSynthReverb1 constructor
	///		Creates a Reverb Object
	///
	///	parameters:
	///		pParams.......Audio parameters
	///		Delay.........Maximum Delay in Seconds
	///-------------------------------------------------
	m_BuffSize = int(Delay * (float)GetParams()->GetSampleRate());
	m_pBuff = new float[m_BuffSize];
	m_Head = 0;
	m_pDelay = 0;
	m_Delay = 1;	///maximum delay
	int i;
	for(i=0;i<m_BuffSize;++i)
		m_pBuff[i] = 0.0;
}

CSynthReverb1::~CSynthReverb1()
{
	delete [] m_pBuff;
}

void CSynthReverb1::Run()
{
	///--------------------------------------------------
	///	Run
	///		Implement the reverb block diagram on page
	/// 508, fig 14-20 in Musical Application of
	/// Microprocessors by Hal Chamberlin 2nd Ed
	///--------------------------------------------------
	if(m_pDelay)
	{
		int NewDelay = (int)((m_pDelay->GetData() - 1.0 ) * (float)(m_BuffSize-1));

		if(NewDelay < 0)
		{
			m_Delay = m_BuffSize - NewDelay - 1;
		}
		else
			m_Delay = NewDelay;
	}
	float out;
	out = m_pIn->GetData();	//get input
	int index = m_Head + m_Delay;
	if(index >= m_BuffSize) index -= m_BuffSize;
//	if(index < 0) index+= m_BuffSize;
	m_pBuff[m_Head] = m_pBuff[index] * m_pFeedBack->GetData() + float(0.75 * out);
	out += m_pBuff[index] * m_pMix->GetData();
	m_pOut->SetData(out);
	m_Head++;
	if(m_Head == m_BuffSize) m_Head = 0;
}

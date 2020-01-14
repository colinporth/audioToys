#include "stdafx.h"
#include <math.h>
#include "SynthCadDefines.h"
#include "SynthEGadsr.h"

CSynthEGadsr::CSynthEGadsr(CSynthParameters *Params, CSynthObject *pParent):CSynthObject(OBJECT_TYPE_EGADSR,Params,pParent)
{
	m_Z = 0.0;
	m_pTrigger = 0;
	m_pAttack = 0;
	m_pDecay = 0;
	m_pOut = 0;
	m_pRelease = 0;
	m_pSustain = 0;
	m_State = EGSTATE_RELEASE;
}

CSynthEGadsr::~CSynthEGadsr()
{
	//dtor
}

void CSynthEGadsr::Run(void)
{
	float TimeConstant=1.0;
	float Level=0.0;

	//---------------------------------------
	// State Machine for ADSR Envelope
	// Generator
	//---------------------------------------
	switch (m_State)
	{
	case EGSTATE_RELEASE:
		if (m_pTrigger->GetData() > 0.5)
			m_State = EGSTATE_ATTACK;
		Level = 0.0;
		TimeConstant = m_pRelease->GetData();
		break;
	case EGSTATE_ATTACK:
		if (m_Z > 0.99)m_State = EGSTATE_DECAY;
		else if (m_pTrigger->GetData() < 0.5)
			m_State = EGSTATE_RELEASE;
		Level = (float)1.1;
		TimeConstant = m_pAttack->GetData();
		break;
	case EGSTATE_DECAY:
		if (m_pTrigger->GetData() < 0.5)
			m_State = EGSTATE_RELEASE;
		Level = m_pSustain->GetData();
		TimeConstant = m_pDecay->GetData();
		break;
	}
	//------------------------------------------
	// The core of the EG is a one pole filter
	//------------------------------------------
	float Fc = LevelToFC(TimeConstant);
	m_Z = m_Z + (double(Level) - m_Z) *double( Fc);
	m_pOut->SetData((float)m_Z);
}

float CSynthEGadsr::TimeToLevel(float time)
{
	///-----------------------------------------------
	/// TimeToLevel
	///
	/// 1.0 -> .001 Seconds
	/// 0.0 -> 1.024 Seconds
	/// -1.0 -> 1048 Seconds
	/// parameters:
	///		time.......tim constant in seconds
	///------------------------------------------------
	float Level;
	Level = float(log(time/1.024) / (log(TWELTHROOT2) * 120));
	if (Level > 1.0) Level = float(1.0);
	else if (Level < -1.0) Level = float(1.0);
	return Level;
}

float CSynthEGadsr::LevelToTime(float Level)
{
	float Time;
	Time =float( 1.024 * pow(TWELTHROOT2,Level * 120.0));
	return Time;
}

float CSynthEGadsr::LevelToFC(float level)
{
	float Fc;
	float Time = LevelToTime(level);
	Fc = float(twoPI / (Time* this->GetParams()->GetSampleRate()));
	return Fc;
}

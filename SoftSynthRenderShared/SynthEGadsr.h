#ifndef CSYNTHEGADSR_H
#define CSYNTHEGADSR_H

#include "SynthDataPath.h"
#include "SynthObject.h"

enum EGStates {
	EGSTATE_RELEASE,
	EGSTATE_ATTACK,
	EGSTATE_DECAY
};

class CSynthEGadsr : public CSynthObject
{
public:
	CSynthEGadsr(CSynthParameters *Params, CSynthObject *pParent);
	virtual ~CSynthEGadsr();
	virtual void Run(void);

	CSynthDataPath *GetTrigger() { return m_pTrigger; }
	void SetTrigger(CSynthDataPath * val) { m_pTrigger = val; }
	CSynthDataPath *GetAttack(){return m_pAttack;}
	void SetAttack(CSynthDataPath *pd){m_pAttack = pd;}
	CSynthDataPath *GetDecay(){return m_pDecay;}
	void SetDecay(CSynthDataPath *pD){m_pDecay = pD;}
	CSynthDataPath *GetSustain(){return m_pSustain;}
	void SetSustain(CSynthDataPath *pD){m_pSustain = pD;}
    CSynthDataPath *GetRelease(){return m_pRelease;}
    void SetRelease(CSynthDataPath *pD){m_pRelease = pD;}
    CSynthDataPath *GetOut(){return m_pOut;}
    void SetOut(CSynthDataPath *pD){m_pOut = pD;}
	inline void SetFc(float Fc) {
			m_a1 = (float)exp(-twoPI * Fc);
			m_b0 = float(1.0 - m_a1);
	}
	float TimeToLevel(float time);
	float LevelToTime(float Level);
	float LevelToFC(float level);
protected:

private:
	CSynthParameters *m_pParams;
	CSynthDataPath *m_pTrigger;
	CSynthDataPath *m_pAttack;
	CSynthDataPath *m_pDecay;
	CSynthDataPath *m_pSustain;
	CSynthDataPath *m_pRelease;
	CSynthDataPath *m_pOut;
	int m_State;
	double m_Z;
	double m_a1,m_b0;
};

#endif // CSYNTHEGADSR_H

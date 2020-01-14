#ifndef CSYNTHVCA_H
#define CSYNTHVCA_H

#include "SynthDataPath.h"
#include "SynthParameters.h"
#include "SynthObject.h"


class CSynthVCA : public CSynthObject
{
	public:
		CSynthVCA(CSynthParameters *params, CSynthObject *pParent);
		virtual ~CSynthVCA();
		virtual void Run(void);
		inline CSynthDataPath *GetIN() { return m_pIN; }
		inline void SetIN(CSynthDataPath *val) { m_pIN = val; }
		inline CSynthDataPath *GetControl() { return m_pControl; }
		inline void SetControl(CSynthDataPath *val) { m_pControl = val; }
		inline CSynthDataPath *GetOut() { return m_pOut; }
		inline void SetOut(CSynthDataPath *val) { m_pOut = val; }

	protected:

	private:
		CSynthDataPath *m_pIN;
		CSynthDataPath *m_pControl;
		CSynthDataPath *m_pOut;
};

#endif // CSYNTHVCA_H

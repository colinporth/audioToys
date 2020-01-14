#include "stdafx.h"
#include "SynthVCA.h"

CSynthVCA::CSynthVCA(CSynthParameters *params, CSynthObject *pParent):CSynthObject(OBJECT_TYPE_VCA,params,pParent)
{
	m_pIN = 0;
	m_pOut = 0;
	m_pControl = 0;
}

CSynthVCA::~CSynthVCA()
{
}

void CSynthVCA::Run(void)
{
	if(m_pIN && m_pOut && m_pControl)
	{
		m_pOut->SetData(m_pIN->GetData() * m_pControl->GetData());
	}
}

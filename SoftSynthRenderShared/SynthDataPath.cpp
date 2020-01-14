#include "stdafx.h"

#include "SynthDataPath.h"

CSynthDataPath::CSynthDataPath(CSynthParameters *pParams, CSynthObject *pParent):CSynthObject(OBJECT_TYPE_DATAPATH,pParams,pParent)
{
    //ctor
    m_Data = 0.0;
}

CSynthDataPath::~CSynthDataPath()
{
    //dtor
}

//{{{  includes
#include "stdafx.h"
#include "SynthObject.h"
//}}}

//{{{
CSynthObject::CSynthObject(int nType, CSynthParameters *pParams, CSynthObject *pParent)
{
		m_pNext = 0;
		m_pPrev = 0;
	m_pHead = 0;
	m_pTail = 0;
	m_pListNext = 0;
		m_Type = nType;
		m_pParams = pParams;
	m_pParent = pParent;

}
//}}}
CSynthObject::CSynthObject(const CSynthObject& /*other */) {}
CSynthObject::~CSynthObject() {}

//{{{
bool CSynthObject::Create(CSynthObject *pParent)
{
	bool rV=1;
	m_pParent = pParent;
	return rV;
}
//}}}

void CSynthObject::Run(void) {}

//{{{
void CSynthObject::AddObject(CSynthObject *pObj)
{
	//***************************************************
	// AddObject
	//  Adds object to the tail of the list
	//  Adds a child object to an object.  This function
	// is not supported by all objects, because they just
	// can have children.  A good example of an object
	// that has children would be a library object
	//
	// parameters:
	//  pObj.....pointer to object to add.
	//
	// return value:none
	//--------------------------------------------------
	if (m_pHead == 0) //nothing in drawing
	{
		m_pHead = pObj;
		m_pTail = pObj;
	}
	else        //add part to end of drawing
	{
		m_pTail->SetNext(pObj);
		pObj->SetPrev(m_pTail);
		m_pTail = pObj;
	}
}
//}}}
//{{{
void CSynthObject::InsertObject(CSynthObject *pObj)
{
	//***************************************************
	// InsertObject
	//  Add the object to the Head of the list
	//  Adds a child object to an object.  This function
	// is not supported by all objects, because they just
	// can have children.  A good example of an object
	// that has children would be a library object
	//
	// parameters:
	//  pObj.....pointer to object to add.
	//
	// return value:none
	//--------------------------------------------------
	if (m_pHead == 0) //nothing in drawing
	{
		m_pHead = pObj;
		m_pTail = pObj;
	}
	else        //add object to front
	{
		m_pHead->SetPrev(pObj);
		pObj->SetNext(m_pHead);
		m_pHead = pObj;
	}
}
//}}}
//{{{
void CSynthObject::RemoveObject(CSynthObject *pObj)
{
	//***************************************************
	// RemoveObject
	//  Removes a child object from an object
	//
	// parameters:
	//  pO.....pointer to object to remove
	// return value:none
	//--------------------------------------------------
	int rV = 0;
	if (pObj == m_pHead)
	{
		m_pHead = (CSynthObject *)m_pHead->GetNext();
		if (m_pHead) m_pHead->SetPrev(0);
		rV = 1;
	}
	else if (pObj == m_pTail)
	{
		m_pTail = (CSynthObject*)m_pTail->GetPrev();
		if (m_pTail) m_pTail->SetNext(0);
	}
	else
	{
		pObj->GetPrev()->SetNext(pObj->GetNext());
		pObj->GetNext()->SetPrev(pObj->GetPrev());
	}
	pObj->SetPrev(0);
	pObj->SetNext(0);
}
//}}}

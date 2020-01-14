#ifndef CSYNTHOBJECT_H
#define CSYNTHOBJECT_H

#include "SynthCadDefines.h"
#include "SynthParameters.h"
#include <Windows.h>

class CSynthParameters;

class CSynthObject
{
	int m_Type;
	int m_Selected;
	//-------------------------------------
	// Variables to manage all CSynthObjs
	//-------------------------------------
	CSynthObject *m_pListNext;
	CSynthObject *m_pNext;	//next object in the list of children
	CSynthObject *m_pPrev;	//previous object in the list of children
	CSynthObject *m_pHead;	//head of the list of childern
	CSynthObject *m_pTail;	//tail of the list of children
	CSynthObject *m_pParent;	//the parent object, if any
	CSynthObject *m_pSelNext;	//next selected object
	CSynthParameters *m_pParams;
public:
    CSynthObject(int nType, CSynthParameters *pParams,CSynthObject *pParent);
    virtual ~CSynthObject();
    CSynthObject(const CSynthObject& other);
	static CSynthObject *FromHandle(HWND hWnd);
	virtual bool Create(CSynthObject *pParent);
	//-------------------------------
	// sound generating functions
	//-------------------------------
    virtual void Run(void);
	virtual LPCWSTR GetTypeString(void) { return L"None"; }
	virtual void AddObject(CSynthObject *pO);
	virtual void InsertObject(CSynthObject *pO);
	virtual void RemoveObject(CSynthObject *pO);
	//--------------------------------
	// Getter functions
	//--------------------------------

     inline int GetType() { return m_Type; }
    inline void SetType(int val) { m_Type = val; }
    inline CSynthObject* GetNext() { return m_pNext; }
    inline void SetNext(CSynthObject *val) { m_pNext = val; }
    inline CSynthObject *GetPrev() { return m_pPrev; }
    inline void SetPrev(CSynthObject *val) { m_pPrev = val; }
	inline CSynthParameters* GetParams(void){return m_pParams;}
	inline CSynthObject * GetParent(void) { return m_pParent; }
	inline CSynthObject *GetHead() { return m_pHead; }
};

#endif // CSYNTHOBJECT_H

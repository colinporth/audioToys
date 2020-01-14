#ifndef CDATAPATH_H
#define CDATAPATH_H

#include "SynthObject.h"


class CSynthDataPath : public CSynthObject
{
    public:
        CSynthDataPath(CSynthParameters *pParams, CSynthObject *pParent);
        virtual ~CSynthDataPath();

        float GetData() { return m_Data; }
        void SetData(float val) { m_Data = val; }

    protected:

    private:
        float m_Data;
};

#endif // CDATAPATH_H

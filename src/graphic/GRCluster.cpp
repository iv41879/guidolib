/*
GUIDO Library
Copyright (C) 2002  Holger Hoos, Juergen Kilian, Kai Renz

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

*/

#include "GRCluster.h"

#include "GRStaff.h"

#include "ARCluster.h"

#include "VGDevice.h"
#include "GRTagARNotationElement.h"
#include "GRSingleNote.h"
#include "GRGLobalStem.h"
#include "ARNoteFormat.h"
#include "TagParameterString.h"
#include "TagParameterFloat.h"

GRCluster::GRCluster(GRStaff * stf, ARCluster * arcls, GRSingleNote *sngNote, ARNoteFormat * curnoteformat) :
						GRARCompositeNotationElement(arcls),
						GRPositionTag(arcls->getEndPosition(), arcls),
                        gStaff(stf),
                        gARCluster(arcls),
                        gDuration(0),
                        gClusterColor(NULL)
{
	assert(stf);
	GRSystemStartEndStruct * sse = new GRSystemStartEndStruct;
	sse->grsystem = stf->getGRSystem();
	sse->startflag = GRSystemStartEndStruct::LEFTMOST;

	sse->p = (void *) getNewGRSaveStruct();

	mStartEndList.AddTail(sse);

    gDuration = sngNote->getDurTemplate();

    const TagParameterString *tmpColor;

    mTagSize = stf->getSizeRatio();

    if (curnoteformat)
    {
        // - Size
        const TagParameterFloat * tmp = curnoteformat->getSize();
        if (tmp)
            mTagSize = tmp->getValue();

        // - Color
        tmpColor = curnoteformat->getColor();
        if (tmpColor)
        {
            if (!mColRef)
                mColRef = new unsigned char[4];
            tmpColor->getRGB(mColRef);
        }

        // - Offset
		const TagParameterFloat * tmpdx = curnoteformat->getDX();
		const TagParameterFloat * tmpdy = curnoteformat->getDY();

		if (tmpdx)
			mTagOffset.x = (GCoord)(tmpdx->getValue(stf->getStaffLSPACE()));
		if (tmpdy)
			mTagOffset.y = (GCoord)(tmpdy->getValue(stf->getStaffLSPACE()));
    }

    const TagParameterString *tmps = arcls->getColor();
    if (tmps)
    {
        gClusterColor = new unsigned char[4];
        tmps->getRGB(gClusterColor);

        if (strcmp(tmps->getValue(), ""))
            mColRef = NULL;
    }

    gdx = arcls->getadx();
    gdy = arcls->getady();
    ghdx = arcls->getahdx();
    ghdy = arcls->getahdy();
    gSize = arcls->getSize();

    GREvent *grEvent = dynamic_cast<GREvent *>(sngNote);
    if (grEvent)
    {
        grEvent->getGlobalStem()->setOffsetXY(gdx, -gdy);
        grEvent->getGlobalStem()->setMultiplicatedSize(gSize);
    }

    gdy += mTagOffset.y;
    gdx += mTagOffset.x;
    
    int *firstNoteParameters = gARCluster->getFirstNoteParameters();

    gFirstNoteYPosition = stf->getNotePosition(firstNoteParameters[0], firstNoteParameters[1]);
    gSecondNoteYPosition = gFirstNoteYPosition;

    gNoteCount = gARCluster->getNoteCount();
}

GRCluster::~GRCluster() {}

void GRCluster::OnDraw(VGDevice &hdc) const
{
    const VGColor prevTextColor = hdc.GetFontColor();

    if (mColRef)
        hdc.SelectFillColor(VGColor(mColRef));
    else if (gClusterColor)
        hdc.SelectFillColor(VGColor(gClusterColor));

    NVRect r = getBoundingBox();
    r += getPosition();

    float x = r.left + gdx + ghdx;

    float curLSpace = gStaff->getStaffLSPACE();

    // - Quarter notes and less
    if (gDuration < DURATION_2 )
    {
        const float xCoords [] = {x - 31 * mTagSize * gSize,
            x + 29 * mTagSize * gSize,
            x + 29 * mTagSize * gSize,
            x - 31 * mTagSize * gSize};
        const float yCoords [] = {gFirstNoteYPosition - gdy - ghdy - curLSpace / 2,
            gFirstNoteYPosition - gdy - ghdy - curLSpace / 2,
            gSecondNoteYPosition - gdy - ghdy + curLSpace / 2,
            gSecondNoteYPosition - gdy - ghdy + curLSpace / 2};

        hdc.Polygon(xCoords, yCoords, 4);
    }
    else
    {
        const float xCoords1 [] = {x - 31 * mTagSize * gSize,
            x + 29 * mTagSize * gSize,
            x + 29 * mTagSize * gSize,
            x - 31 * mTagSize * gSize};
        const float yCoords1 [] = {gFirstNoteYPosition - gdy - ghdy - curLSpace / 2,
            gFirstNoteYPosition - gdy - ghdy - curLSpace / 2,
            gFirstNoteYPosition - gdy - ghdy - curLSpace / 2 + 6,
            gFirstNoteYPosition - gdy - ghdy - curLSpace / 2 + 6};
        const float xCoords2 [] = {x + 23 * mTagSize * gSize,
            x + 29 * mTagSize * gSize,
            x + 29 * mTagSize * gSize,
            x + 23 * mTagSize * gSize};
        const float yCoords2 [] = {gFirstNoteYPosition - gdy - ghdy - curLSpace / 2,
            gFirstNoteYPosition - gdy - ghdy - curLSpace / 2,
            gSecondNoteYPosition - gdy - ghdy + curLSpace / 2,
            gSecondNoteYPosition - gdy - ghdy + curLSpace / 2};
        const float xCoords3 [] = {x - 31 * mTagSize * gSize,
            x + 29 * mTagSize * gSize,
            x + 29 * mTagSize * gSize,
            x - 31 * mTagSize * gSize};
        const float yCoords3 [] = {gSecondNoteYPosition - gdy - ghdy + curLSpace / 2 - 6,
            gSecondNoteYPosition - gdy - ghdy + curLSpace / 2 - 6,
            gSecondNoteYPosition - gdy - ghdy + curLSpace / 2,
            gSecondNoteYPosition - gdy - ghdy + curLSpace / 2};
        const float xCoords4 [] = {x - 31 * mTagSize * gSize,
            x - 25 * mTagSize * gSize,
            x - 25 * mTagSize * gSize,
            x - 31 * mTagSize * gSize};
        const float yCoords4 [] = {gFirstNoteYPosition - gdy - ghdy - curLSpace / 2,
            gFirstNoteYPosition - gdy - ghdy - curLSpace / 2,
            gSecondNoteYPosition - gdy - ghdy + curLSpace / 2,
            gSecondNoteYPosition - gdy - ghdy + curLSpace / 2};

        hdc.Polygon(xCoords1, yCoords1, 4);
        hdc.Polygon(xCoords2, yCoords2, 4);
        hdc.Polygon(xCoords3, yCoords3, 4);
        hdc.Polygon(xCoords4, yCoords4, 4);
    }

    // - Restore context
    if (mColRef || gClusterColor)
        hdc.SelectFillColor(prevTextColor);  //(TODO: in a parent method)
}

void GRCluster::addAssociation(GRNotationElement *p)
{
	GRARCompositeNotationElement::addAssociation(p);
}

GuidoPos GRCluster::AddTail(GRNotationElement * el)
{
	GuidoPos mypos = GRARCompositeNotationElement::AddTail(el);

	// now, we remove the associatons ...
	// of those elements, that are members of 
	// the composite-sructure ...

	GuidoPos elpos = NULL;
	if (mAssociated && (elpos = mAssociated->GetElementPos(el)) != NULL)
	{
		mAssociated->RemoveElementAt(elpos);
	}

	return mypos;
}

//-------------------------------------------------------------------
void GRCluster::tellPosition(GObject * caller, const NVPoint & np)
{
	GREvent *ev = dynamic_cast<GREvent *>(caller);
	
    if (ev)
	{
        NVPoint newPoint (np);
        newPoint.y = -LSPACE;

        setPosition(newPoint);
	}
}

void GRCluster::setSecondNoteYPosition()
{
    int *secondNoteParameters = gARCluster->getSecondNoteParameters();

    gSecondNoteYPosition = gStaff->getNotePosition(secondNoteParameters[0], secondNoteParameters[1]);

    if (gFirstNoteYPosition > gSecondNoteYPosition)
    {
        float tmpSwitch = gSecondNoteYPosition;
        gSecondNoteYPosition = gFirstNoteYPosition;
        gFirstNoteYPosition = tmpSwitch;
    }
}
/*
  GUIDO Library
  Copyright (C) 2002  Holger Hoos, Juergen Kilian, Kai Renz
  Copyright (C) 2002-2013 Grame

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.

  Grame Research Laboratory, 11, cours de Verdun Gensoul 69002 Lyon - France
  research@grame.fr

*/

#ifdef WIN32
# pragma warning (disable : 4800)
#endif
// - Standard C++
#include <typeinfo>
#include <cmath>
#include <iostream>

// - Guido Misc
#include "kf_vect.h"

// - Guido AR
#include "TagParameterFloat.h"

#include "ARClef.h"
#include "ARKey.h"

#include "ARMusic.h"
#include "ARMusicalTag.h"
#include "ARMusicalVoice.h"
#include "ARStaff.h"
#include "ARPageFormat.h"
#include "ARSystemFormat.h"
#include "ARAuto.h"
#include "ARDisplayDuration.h"
#include "ARAccolade.h"

// - Guido GR
#include "GRSystem.h"
#include "GRClef.h"
#include "GRKey.h"
#include "GRSpring.h"
#include "GRSpringForceIndex.h"
#include "GRVoice.h"
#include "GRMusic.h"
#include "GRPossibleBreakState.h"
#include "GRBar.h"
#include "GRTempo.h"
#include "GRPageText.h"
#include "GRRepeatEnd.h"
#include "GREmpty.h"
#include "GRMeter.h"
#include "GRChordTag.h"
#include "GRSystemSlice.h"
#include "GRSpacingMatrix.h"
#include "GRPage.h"
#include "GRBreakMatrix.h"
#include "GRSliceHeight.h"
#include "GRVoiceManager.h"
#include "GRStaffManager.h"

// if _DEBUGSFF is set, then the temporary SpaceForceFunctions
// for all potential system breaks are written into files.
// also look into GRSystem for a similar output routine for
// the actual SpaceForceFunction being realized.
//#undef _DEBUGSFF
// #define _DEBUGSFF

#if 0
#define traceslice(a)	a
#else
#define traceslice(a)
#endif

using namespace std;

const bool kIsGiesekingSpacing = true;

/** \brief The StaffManager is responsible for traversing the voice and 
	really doing everything. 

	The systems are created whenever real NewLines are 
	encountered (or automatic once are introduced).
*/
GRStaffManager::GRStaffManager(GRMusic * p_grmusic, const ARPageFormat * inPageFormat, const GuidoLayoutSettings * aSettings) : 
    mGrMusic(p_grmusic),
    mArMusic(nullptr),
    mGrSystem(nullptr),
    mStaffStateVect(nullptr),
    staffposvect(0),
    mSystemDistancePrev(-1.0f),
    mSystemDistance(-1.0f),
    mCurSysFormat(nullptr),
    mSystemSlices(new sysslicelist(0)),
    //! SPRING-ROD SPACING ALGORITHM
    beg_sff_list(new bsfflist(1)),
    beg_spr_list(new sprlist(1)),
    staffTopVector(0),
    staffBottomVector(0),
    lastrod(nullptr),
    firstrod(nullptr),
    voiceSpringArr(-1),
    evlist(1),
    mTempSpringID(1),
    mSpringID(1),
    mLastSpringID(0),
    mMaxClef(nullptr),
    mMaxKey(nullptr)
{
    mArMusic = mGrMusic->getARMusic();

	if(aSettings) 
    {
		settings = *aSettings;

		settings.neighborhoodSpacing = aSettings->neighborhoodSpacing == 1;
		settings.optimalPageFill = aSettings->optimalPageFill == 1;
		settings.proportionalRenderingForceMultiplicator =
				(aSettings->proportionalRenderingForceMultiplicator < 0.0001 ? 0 : aSettings->proportionalRenderingForceMultiplicator);
	} else {
		// Apply default layout settings
		GuidoGetDefaultLayoutSettings (&settings);
	}

	// now we create the very first page and also the very first system
    mGrPage = new GRPage(mGrMusic, this, DURATION_0, settings, nullptr);
	if (inPageFormat)
		mGrPage->setPageFormat(inPageFormat);
	mGrMusic->addPage(mGrPage);

	// a GRSystem is build later from the slices ...
	// Or should we start with at least one system this is not yet clear -> we leave it nullptr at first.
	// the very first SystemSlice ....
	mGrSystemSlice = new GRSystemSlice(this, DURATION_0);
	// this is arguable: the 0 is the first glue, this is only for the very first start ....
	mGrSystemSlice->mStartSpringID = 0;
	mMyStaffs = new VStaff(0); // the Staves are not owned by the Manager

	// An Array of Voice-Managers
	mVoiceMgrList = new VoiceManagerList(1); // owns elements

    //! SPRING-ROD SPACING ALGORITHM initialization
	// now we create the lists and vector. does own the elements!
	mSimpleRods   = new IRodList(1);
	mComplexRods  = new IRodList(1);
	mSpringVector = new ISpringVector(1);

	// put one single (beginning) spring in the spring vector, with id 0.
	GRSpring * spr = new GRSpring(TYPE_TIMEPOSITION(), DURATION_0, settings.spring, settings.proportionalRenderingForceMultiplicator);
	spr->setID(0);
	spr->change_const(50);
	mSpringVector->Set(0, spr);

    //! PROPORTIONAL SPACING ALGORITHM initialization
    //...
}

// ----------------------------------------------------------------------------
GRStaffManager::~GRStaffManager()
{
	if (mSystemSlices)
	{
		// This is important for ABORT issues ...
		// if the parse was aborted, the system slices have not been transfered to the Systems yet 
		if (mSystemSlices->GetCount() > 0)
			mSystemSlices->setOwnership( true );
		delete mSystemSlices;
		mSystemSlices = nullptr;
	}

	beg_sff_list->RemoveAll();
	delete beg_sff_list;
	delete beg_spr_list;

	// never owned ...
	delete mStaffStateVect;
	// the staffs are not owned directly but are handled by the mGrSystemSlice which is deleted below .
	delete mMyStaffs;
    mMyStaffs = nullptr;

	// already has ownership ... 
    delete mVoiceMgrList; 
    mVoiceMgrList = nullptr;

	delete mSimpleRods;
	delete mComplexRods;
	delete mSpringVector;
	delete mGrSystemSlice;
}

// ----------------------------------------------------------------------------
/** \brief Creates and fills the staves.

it uses GRVoiceManagers to go through the individual voices.
*/
void GRStaffManager::createStaves()
{
    // We have a ARmusic (armusic) and start from the beginning ...
    // now we go through all voices and create graphical representations. We also
    // add these to the respective staves (standard or handled by staff-tag)
    int voicesCount = initVoices();

    // this parameter is used to capture explicit \newSystem tags. The height
    // of the page is returned by FindOptimum in the case of a newSystem-call 
    float beginheight = 0.0;

    // This controls if the VoiceManagers go to the next event.
    // It becomes false if the next event is not tag event.
    bool filltagmode = true;

    // This is set to true if all voices ends.
    // It is first set to true and if there is at least one voice still
    // active it is cleared to false.
    bool allVoicesRead = false;				

    TCreateStavesState state;

    do
    {
        state.continueWithFillTagMode = false;
        state.newline = kNormalState;
        state.pbreakval = 0.0;

        // the time is incremented by the minimum amount of time...
        state.mintp = MAX_DURATION;
        state.minswitchtp = MAX_DURATION;

        // now, we go through all the voices sequentially
        allVoicesRead = nextTimePosition(voicesCount, filltagmode, state);

        // now we need to check whether we need to switch to non-filltagmode
        if (filltagmode)
        {
            // we had tags; now we check, whether tags follow
            if (!state.continueWithFillTagMode)
            {
                filltagmode = false;

                //! SPRING-ROD SPACING ALGORITHM
                // no tags follow, so now we finish the Synchronization-Slice at the current tp.
                // This handles spring-stuff...
                FinishSyncSlice(state.timePos);

                GRSpaceForceFunction2 * sff = nullptr;
                if (state.newline != kNormalState)
                {
                    // this builds the current Spring-Force-Function (that is, rods and springs and stuff is created).
                    sff = BuildSFF();

                    GRPossibleBreakState * pbs = new GRPossibleBreakState();
                    pbs->sff = sff;
                    pbs->copyofcompletesff = nullptr;
                    float force = 0.0;
                    pbs->SaveState(mMyStaffs, mVoiceMgrList, this, state.timePos, force, state.pbreakval);

                    mGrSystemSlice->addPossibleBreakState(pbs);

                    mGrSystemSlice->setNumber(mSystemSlices->GetCount() + 1);
                    mGrSystemSlice->mEndSpringID = mSpringID - 1;

                    // this is the location, where the systemslice is "officially" finished.
                    mGrSystemSlice->Finish();
                    mSystemSlices->AddTail(mGrSystemSlice);

                    // then we build a new mGrSystemSlice, and start a new ...
                    mGrSystemSlice = nullptr;
                    mLastSpringID = mSpringID;
                }

                if (state.newline == kNewSystem || state.newline == kNewPage)
                    beginheight = systemBreak(state.newline, beginheight);

                if (state.newline == kNewPage)
                {
                    mGrPage = new GRPage(mGrMusic, this, state.timePos, settings, mGrPage);
                    mGrMusic->addPage(mGrPage);
                    beginheight = 0.0;
                }

                if (state.newline != kNormalState)
                {
                    // not sure of this ....
                    // now we tell each voice, that we have handled the break-condition
                    // remember that we are breaking right now mIsBreak = true;
                    // we actually have to create a new systemslice so that we can put something together ....
                    mGrSystemSlice = new GRSystemSlice(this, state.timePos);

                    // This is important: a newline == 3 means, that this is only a potential breakpoint
                    // newline != 3 is a user-imposed real breakpoint
                    // In this case, the begin elements of all staves are already there and will not be 
                    // added manually therefore, the first spring is the glue spring and will be present.
                    if (state.newline == kPBreak)
                        mGrSystemSlice->mStartSpringID = mSpringID;
                    else
                        mGrSystemSlice->mStartSpringID = 0;
                    delete mMyStaffs;
                    mMyStaffs = new VStaff(0); // the Staves are no longer valid and need to be created a new ...

                    for (int i = 0; i < voicesCount; i++)
                    {
                        GRVoiceManager * voiceManager = mVoiceMgrList->Get(i);
                        voiceManager->DoBreak(state.timePos, state.newline);
                    }

                    filltagmode = true;
                }
            }
        }
        // no filltagmode (eventmode)
        else 
        {
            if (state.mintp != MAX_DURATION)
                state.timePos = state.mintp;		// we increment the timeposition ...
            else if (!allVoicesRead)
                assert(false);			            // there must have been a timeposition

            // if we need to switch, then we switch
            if (state.minswitchtp == state.timePos)
                filltagmode = true;

            //! SPRING-ROD SPACING ALGORITHM
            FinishSyncSlice(state.timePos);
        }

        // new: we do a very crude test, if the current space runs low ... This is in no ways accurate ( but it needn't be).
        // It works as follows:
        // Each time, an element is added to a staff we add the left- and right-space and compare
        // it to a maximum value. All this is summed up for the whole system (the maximum for each slice).
        // If we reach a value of twice the page-size, we issue a warning and FORCE A NEW SYSTEM! -> otherwise the algorithms run too long.
    } while (!allVoicesRead);

    //! SPRING-ROD SPACING ALGORITHM
    // I have to rethink the following ...
    // it may be, that I have to finish the last slice before calling the optimum-Break-Routine ....
    if (mSpringVector->GetMaximum() >= mSpringVector->GetMinimum())
    {
        // there is more than one spring ...
        GRSpaceForceFunction2 * sff = 0;

        if (mSpringID > mLastSpringID)
        {
            sff = BuildSFF();

            // add the thing to the slice-list 
            GRPossibleBreakState * pbs = new GRPossibleBreakState();
            pbs->sff = sff;
            pbs->copyofcompletesff = nullptr;

            float force = 0.0;
            pbs->SaveState(mMyStaffs, mVoiceMgrList, this, state.timePos, force, state.pbreakval);
            mGrSystemSlice->addPossibleBreakState(pbs);

            // now we need to add the systemslice to the list of available system-slices ....
            mGrSystemSlice->setNumber(mSystemSlices->GetCount() + 1);
            mGrSystemSlice->mEndSpringID = mSpringID - 1;
            mGrSystemSlice->Finish();
            mSystemSlices->AddTail(mGrSystemSlice);
            mGrSystemSlice = nullptr;
        }
    }

    FindOptimumBreaks(0, beginheight);
}

// ----------------------------------------------------------------------------
// This method has been created to structure the createStaves method
// D.F. Nov. 2016
// This method computes time positions as well as new line information,
// possible break value and a flag for tags management (continueWithFillTagMode).
// Returns a boolean value to indicate the end of all voices.
bool GRStaffManager::nextTimePosition (int voicesCount, bool filltagmode, TCreateStavesState& state)
{
    TYPE_TIMEPOSITION currentTimePos;
	bool endOfVoice = true;

    for (int i = 0; i < voicesCount; i++)
	{
        currentTimePos = state.timePos;

		GRVoiceManager * voiceManager = mVoiceMgrList->Get(i);

        //! 1. Iterates thru current voice and create related GR notation elements
        int status = voiceManager->Iterate(currentTimePos, filltagmode);

        if (status != GRVoiceManager::ENDOFVOICE)
            endOfVoice = false;

        //! 2. Updating the TCreateStavesState

		// minswitchtp remembers the next timeposition for an event. If the 
		// filltagmode is 0 after this loop, this value is taken for incrementing the curTP.
        if (status == GRVoiceManager::CURTPBIGGER_ZEROFOLLOWS || status == GRVoiceManager::DONE_ZEROFOLLOWS)
		{
            if (currentTimePos < state.minswitchtp)
                state.minswitchtp = currentTimePos;
		}

        if (status != GRVoiceManager::MODEERROR)
        {
			if (filltagmode)
			{
                // if there has been at last one tag -> continue with filltagmode
                if (!state.continueWithFillTagMode && status == GRVoiceManager::DONE_ZEROFOLLOWS)
                    state.continueWithFillTagMode = true;
			
                if (status == GRVoiceManager::NEWSYSTEM) {
					if (state.newline == 0 || state.newline == 3)
						state.newline = kNewSystem;
				}
                else if (status == GRVoiceManager::NEWPAGE)
					state.newline = kNewPage;				// this overrides any other newline setting
                else if (status == GRVoiceManager::PBREAK)
				{
                    if (state.newline == kNormalState) {
						state.newline = kPBreak;
						state.pbreakval = voiceManager->pbreakval;
					}
				}
			}
            // no filltagmode (that is eventmode)
			else { 
                if (status == GRVoiceManager::DONE_ZEROFOLLOWS || status == GRVoiceManager::DONE_EVFOLLOWS ||
                    status == GRVoiceManager::DONE || status == GRVoiceManager::CURTPBIGGER_EVFOLLOWS ||
                    status == GRVoiceManager::CURTPBIGGER_ZEROFOLLOWS)
				{
					// set the increment of timeposition
                    if (currentTimePos < state.mintp) state.mintp = currentTimePos;
				}
			}
		}
	}

    return endOfVoice;
}

// ----------------------------------------------------------------------------
int GRStaffManager::initVoices()
{
    int voiceCount = 0;

	ARMusicalVoice * voice;
	GuidoPos pos = mArMusic->GetHeadPosition();

	while (pos) {
		voice = mArMusic->GetNext(pos);
		// this is important, so that chords are not over-read but the explicit events are read.
		voice->setReadMode(ARMusicalVoice::EVENTMODE);

        GRVoiceManager * voiceManager = new GRVoiceManager(mGrMusic, this, voice, voiceCount);
		// This call initializes the GRVoiceManager
		// ATTENTION: realize the significance of STAFF-Tags at the very start!
		voiceManager->BeginManageVoice();

        mVoiceMgrList->Set(voiceCount++, voiceManager);
	}

    return voiceCount;
}

/** \brief Called by the GRVoiceManager to add those tags that are placed directly on
	the page (like e.g. \\title, \\composer etc.).

	The elements are just put where they would be put by default.
	At the moment, there is no check, whether information is provided more than once ... this
	could/should be done later.
*/
int GRStaffManager::AddPageTag(GRNotationElement * grel, GRStaff * grstaff, int voiceid)
{
	GRPageText * grpgtxt = dynamic_cast<GRPageText *>(grel);
	if (grpgtxt)
	{
		// the current page (this is known only here, not really in the voice-manager)
		grpgtxt->setGRPage(mGrPage);
		// this calls the routine to calculate the concrete Position within the page
		grpgtxt->calcPosition();
	}
	// this adds the tag to the page ...
	mGrPage->AddTail(grel);
	return 0;
}

/** \brief Sets the pageformat for a newPage
*/
void GRStaffManager::setPageFormat( const ARPageFormat * pform )
{
	mGrPage->setPageFormat(pform);
}

/** \brief Sets the systemFormat or a newSystem 
*/
void GRStaffManager::setSystemFormat(const ARSystemFormat * sysfrm)
{
	// we have to set the system-Format
	// I need to save this in a variable and remember this setting later ....
	mCurSysFormat = sysfrm;

	// Important for Advanced Files!
	// At this point, mGrSystem = 0. We may need to create an initial mGrSystem
	// mGrSystem->setSystemFormat(sysfrm);
}

void GRStaffManager::setSystemDistance( float distance,
									   const GRVoiceManager & p_vcmgr)
{
	// The previous value must be saved, because the call to
	// setSystemDistance comes before the FindOptimumBreak
	// Routine has been called ....
	mSystemDistance = distance;
}

GRGlobalStem * GRStaffManager::getOtherGlobalStem(GRSystemSlice * psys, GRVoiceManager * curvcmgr, const NVstring & label)
{
	// to do this, I have to go through all voice-managers
	// and find the ones that have an active chord-tag with
	// the same label as presented and also a curglobalstem ...

	int i;
	const int mini = mVoiceMgrList->GetMinimum();
	const int maxi = mVoiceMgrList->GetMaximum();

	for( i = mini; i <= maxi; ++i )
	{
		GRVoiceManager *vcmgr = mVoiceMgrList->Get(i);
		if (vcmgr && vcmgr != curvcmgr)
		{
			if (vcmgr->curchordtag && vcmgr->curglobalstem)
			{
				if (vcmgr->curchordtag->getLabel() == label)
				{
					const ARMusicalVoiceState * vstate = vcmgr->getVoiceState();
					const ARMusicalVoiceState * cvstate = curvcmgr->getVoiceState();
					if (vstate->fCurdispdur && cvstate->fCurdispdur)
					{
						if (vstate->fCurdispdur->getDisplayDuration() == cvstate->fCurdispdur->getDisplayDuration())
							return vcmgr->curglobalstem;
					}
					else
						return vcmgr->curglobalstem;
				}
			}
		}
	}
	return 0;
}

/** \brief Called to take care of resuming the open tags
from the previous system.
*/
void GRStaffManager::ResumeOpenTags(const GRSystemSlice * lastslice, GRSystemSlice * beginslice)
{
    // now we retrieve the open grtags ....
    GRPossibleBreakState * pbs = lastslice->mPossibleBreakState;
    if (pbs)
    {
        const int mini = pbs->vtsvect->GetMinimum();
        const int maxi = pbs->vtsvect->GetMaximum();

        for (int i = mini; i <= maxi; ++i)
        {
            // These are the GRVoiceTagsAndStaff
            GRPossibleBreakState::GRVoiceTagsAndStaff * vts = pbs->vtsvect->Get(i);

            if (vts && vts->grtags)
            {
                int newstaffnum = vts->staffnum;
                vts->BeginAfterBreak(beginslice->mStaffs->Get(newstaffnum), (GuidoPos)0);
            }
        }
    }
}

// ----------------------------------------------------------------------------
// (JB) Each system should have a list of accolade tags.
void
GRStaffManager::notifyAccoladeTag(ARAccolade * inAccoladeTag)
{
    //	if( mGrSystem )
    //		mGrSystem->notifyAccoladeTag( inAccoladeTag );

    // at this point, mGrSystem == 0, so we need to store input tag
    // for futur use.

    mCurAccoladeTag.push_back(inAccoladeTag);
}

/** \brief Called from the voice-managers.

The StaffManager has to make sure that the staff is there, otherwise it needs to create it.
*/
void GRStaffManager::prepareStaff(int staff)
{
    GRStaff * curstaff = mMyStaffs->Get(staff);

    if (curstaff == nullptr)
    {
        curstaff = new GRStaff(mGrSystemSlice, settings.proportionalRenderingForceMultiplicator);

        if (mStaffStateVect) {
            // this just copies the state information ...
            GRStaffState * myss = mStaffStateVect->Get(staff);
            if (myss) {
                // this sets the information explicitly that would otherwise be automatically
                // copied with CreateBeginElements if an automatic Break instead of a 
                // forces break would occur.
                curstaff->setStaffState(myss);
            }
        }

        mGrSystemSlice->addStaff(curstaff, staff);

        // We apply potential staff size defined with GuidoSetStaffSize API call
        applyStaffSize(curstaff, staff);
    }

    // set the staff in  Vector mMyStaffs.
    mMyStaffs->Set(staff, curstaff);
}


GRStaff * GRStaffManager::getStaff(int staff)
{
    GRStaff * curstaff = mMyStaffs->Get(staff);
    if (curstaff == nullptr) prepareStaff(staff);
    return mMyStaffs->Get(staff);
}

/** \brief Apply potential staff size defined with GuidoSetStaffSize API call
to given staff with given staff number.
*/
void GRStaffManager::applyStaffSize(GRStaff *staff, int staffNum)
{
    float staffSize = mGrMusic->getStaffSize(staffNum);

    if (staffSize >= 0)
        staff->setStaffSize(staffSize);
}

/** \brief Sets a staff-state-tag. (like clef, meter, key)

If the staff is not already there, it is created first.
Remember, this just sets the state; no graphical elements are created here.

return false on error (i.e: input tag was not a staff-state tag)
*/
bool GRStaffManager::setStaffStateTag(ARMusicalTag * tag, int staffnum)
{
    if (tag == nullptr) return false;

    GRStaff * grstaff = getStaff(staffnum);
    bool result = true;
    const type_info & ti = typeid(*tag);

    // this is set if the beginning sff needs to be updated ....
    bool needupdate = false;
    GRStaffState & mystate = grstaff->getGRStaffState();

    if (ti == typeid(ARClef))	// we should consider using a dynamic cast only
    {
        ARClef * tmp = static_cast<ARClef *>(tag);
        needupdate = true;
        // now check, wether the curclef is already set for the same timeposition and with different parameters.
        if (mystate.curclef) {
            if (mystate.curclef->getRelativeTimePosition() == tmp->getRelativeTimePosition() && !(*mystate.curclef == *tmp))
                GuidoWarn("Adding a different clef to a staff at the same timeposition");
        }
        mystate.curclef = tmp;
    }
    else if (ti == typeid(ARMeter)) {
        mystate.curmeter = static_cast<ARMeter *>(tag); // was dynamic cast<ARMeter *>(tag);
    }
    else if (ti == typeid(ARKey)) {
        needupdate = true;
        mystate.curkey = static_cast<ARKey *>(tag); // was dynamic cast<ARKey *>(tag);
    }
    else result = false;		// was not a staff-state tag.

    if (needupdate) UpdateBeginningSFF(staffnum);
    return result;
}


//! SPRING-ROD SPACING & LINE BREAKING ALGORITHM (functions)


/** \brief Takes the current systemslices-list and
	iterates through different possibilities to break the systems.

	This function is first implemented as a very simple first-fit
	algorithm and then elaborated to cover more features (like CYK
	and Dynamic Programming)

	the algorithm works as follows:
	For each systemslice, I figure how much it costs to
	break from the beginning to some slices in the future. If the costs
	become too high. I am finished with this slice.
	I then update the overall-entry in the array to
	set the optimum value. 
*/

class Entry;

/** \brief An array of Entries. One Entry for each page-position (need
	to experiment with values for that).
*/
typedef KF_IVector<Entry> entryvect;

typedef KF_IVector<entryvect> breakvect;

class Entry 
{
public:
	float penalty;
	int   precessorcount; // the slice of the precessor
	int   precessorslot;  // the slot within the slice ...

	int   pagebreaktoprev; // this is set, if there is a pagebreak 
				// to the precessor ....
	float curheight; // the total height ... this is begin used for
			// optimum pagebreaks ....

	ostream & operator<<(ostream & os)
	{
		os << "penalty:   " << penalty << endl;
		os << "pre-count: " << precessorcount << endl;
		os << "pre-slot:  " << precessorslot  << endl;
		os << "pagebreak: " << pagebreaktoprev << endl;
		os << "curheight: " << curheight << endl;
		return os;

	}
	int getPageNumlines(const breakvect &mybreakvect) const
	{
		// this determines the number of lines on the current page ....
		int numlines = 1;
		const Entry * tmpent = this;
		while (tmpent && !tmpent->pagebreaktoprev)
		{
			numlines++;
			entryvect * myentvect = mybreakvect.Get(tmpent->precessorcount);
			if (myentvect)
				tmpent = myentvect->Get(tmpent->precessorslot);
			else
				tmpent = nullptr;
		}
		return numlines;

	}
};

class breakentry
{
public:
	int begslice;
	int numslices;
	int followedbypagebreak;
};

typedef KF_IPointerList<breakentry> breaklist;


#ifdef _DEBUG
class edge
{
public:
	int source;
	int target;
	float penalty;
};

typedef KF_IPointerList<edge> edgelist;
#endif

/** \brief Finds the optimum break-sequence in our list
	of systemslices.  
	
	When the optimum sequence has been found, the Systems are created. If necessary, new beginslices are
	created, which contain the necessary clefs and key signatures for the new staves ....
	the parameter beginheight is needed, so that explicit systemBreaks can be captured ... 
	Then the optimum page-breaking must take into account where we begin on the page. 
	the parameter pageorsystembreak is set to to get either an optimal pagebreak or an optimal systembreak. 
	It is set, when explicit \\newSystem or \\newPage tags are encountered. 
	In the case of systembreaks, we will not look for an optimum page -break but end the page wherever it is best.
	
	\return the height of the entries on the last page
	 this is used for repeated calls to FindOptimumBreaks, if explicit newsystemcalls are encountered ....
*/
float GRStaffManager::FindOptimumBreaks(int inPageOrSystemBreak, float inBeginHeight)
{
	int numpageareas = 12;
	if( inPageOrSystemBreak == 1 )	numpageareas = 1;
	if( settings.optimalPageFill == false )		numpageareas = 1;

#ifdef _DEBUG
	edgelist edges(1);
#endif

	// this owns the entries ....
	breakvect * mybreakvect = new breakvect(1);
	int numslices = mSystemSlices->GetCount();

traceslice(cout << "GRStaffManager::FindOptimumBreaks num slices is " << numslices << endl);
	for (int i = 0; i <= numslices; ++i ) {
		// the individual entryvects own the entries ....
		mybreakvect->Set(i, new entryvect(1));
	}

	float pageheight = mGrPage->getInnerHeight();
	float usedsystemdistance = -1.0f;
	if (inBeginHeight > 0 && mSystemDistancePrev > 0)
	{
		usedsystemdistance = mSystemDistancePrev;
		inBeginHeight -= settings.systemsDistance;
		inBeginHeight += mSystemDistancePrev;				// the new distance 
		if (inBeginHeight < 0 ) 	inBeginHeight = 0;
	}
	mSystemDistancePrev = mSystemDistance;
	mSystemDistance = -1.0f;								// has to be reset with newSystem otherwise ....

	int count = -1;
	GuidoPos pos = mSystemSlices->GetHeadPosition();		// then I just iterate through the systemslices ....
	while (pos)
	{
		// no longer needed because of sliceheight
		float curheight = 0;
		GRSliceHeight sliceheight;		// this takes care of managing the correct height of collection of slices ....
		float curxmin = 0;
		float curconstant = -1;

		count++;
		GuidoPos tmppos = pos;
		int tmpcount = count;
		entryvect * myentvect = mybreakvect->Get(count);	// this retrieves the entryvect for a slice

		// there are several possibilites:
		// either, we are at the very beginning ... then we have to calculate every possible beginning ... 
		// If we are not at the beginning, we have to check, whether there is at least one possible
		// beginning. Only if this is the case, we need to calculate the values for potential systems ....
		if (count == 0 || myentvect->GetCount() > 0 )
		{
		}
		else
		{
			// then we can just continue without anything ....
			mSystemSlices->GetNext(pos);
			continue;
		}
		
		GRSystemSlice * begslice = mSystemSlices->GetAt(pos);	// this is the beginning slice of the potential line.
		// this gets the SpaceForceFunction of the beginning-elemnts 
		// that would be needed, if the break would really occur at this location.
		if (count > 0 )
		{
            const GRBeginSpaceForceFunction2 * begsff = begslice->mBeginForceFunc;
			if (begsff)
			{
				// this initialises the spring constant together with the minimum extent ....
				curconstant = begsff->getOptConstant();
				curxmin = begsff->getXminOpt();
				// this adds the beginning slice using the bounding-retangle of the staffs ...
				sliceheight.AddSystemSlice(begslice,1);
				//float alterheight = sliceheight.getHeight();
			}
		}

		// this variable is set so that we can abort the next loop once a precessor has been found.
//		bool predecessor_found = false;
//		float predecessor_value = 0;

		while (tmppos) {
			tmpcount++;
			GRSystemSlice * slc = mSystemSlices->GetNext(tmppos);	
			if (slc)
			{
                float optconst = 0;

                if (slc->mPossibleBreakState)
				    optconst = slc->mPossibleBreakState->sff->getOptConstant();

				if (curconstant == -1)
					curconstant = optconst;
				else {
					// we just add the constant ...
					float calt = curconstant;
					float cneu = 1.0f / ( (1.0f /calt) + (1.0f /optconst) );
					curconstant = cneu;
				}

                if (slc->mPossibleBreakState)
				    curxmin += slc->mPossibleBreakState->sff->getXminOpt(); // we also just add the curxmin-value ....

				// we have to deal with the height ...
				sliceheight.AddSystemSlice(slc);
				const float slcheight = slc->mBoundingBox.Height();
								
				if (slcheight > curheight)
					curheight = slcheight;
				//float alterheight = sliceheight.getHeight(); // no top
			}

			// then we check, if we are in the range for the system ....
			if (curconstant > 0)
			{
				// I have to take care of the very first line ....
				// If a systemformat is set, I want the break to occur differently ... but only for the very first line ....
				float wishext = mGrPage->getInnerWidth();
				if (count == 0 && mCurSysFormat)
				{
                    const TagParameterFloat *tpf = mCurSysFormat->getDX();
                    float dx = 0;

                    if (tpf)
					    dx = tpf->getValue();
					
                    wishext = wishext - dx ;
				}
				float reqforce = (wishext - curxmin) * curconstant;
				// if reqforce is in the range of optforce then we have found an optimum break ....

				//int dobreak = 0;
				float val = reqforce - settings.force;
				if ((val >0 && val <  1.3f * settings.force)
					|| (val < 0 && val > -0.5f * settings.force))
				{
//					predecessor_found = true;
//					predecessor_value = val;

					if (val < 0)
					{
						val = -val;
						val = pow( val, 1.2f );
					}
					// we are in the range of 750 above the optimum force ...
					// then we have found a break .... then the value is valid ....

					// now we need to check this value against all the possible
					// ones on the current entryvect of count ....
					if (myentvect->empty())
					{
						// Then we are at the very beginning ....
						entryvect * tovect = mybreakvect->Get(tmpcount);

						// find the correct entry ...
						//float altercurheight = sliceheight.getHeight();
						float myheight = sliceheight.getHeight() +
							//curheight + 
							inBeginHeight;

						int slot = -1;
						int ispagebreak = 0;
						if (myheight > pageheight)
						{
							ispagebreak = 1;
							myheight = sliceheight.getHeight(1);
									// curheight;
							if (myheight > pageheight)
								// then, the cursystem will not fit on a single page ....
								// ...  
								myheight = pageheight;
						}
						
						slot = (int)( floor((float) myheight / pageheight * (numpageareas-1) + 0.5)); 
						assert(slot < numpageareas);

						Entry * myent = tovect->Get(slot);
						if (myent)
						{
							// then, the entry is already there, we compare below ....
						}
						else
						{
							myent = new Entry;
							myent->penalty = (float)1e9;
							myent->precessorcount = -1;
							tovect->Set(slot,myent);
						}

						if (val < myent->penalty)
						{
							myent->penalty = val;
							assert(count == 0);
							myent->precessorcount = count;
							myent->precessorslot  = 0; 
							myent->curheight = myheight;
							myent->pagebreaktoprev = ispagebreak;
						}
#ifdef _DEBUG
						{
							edge * edg = new edge;
							edg->source = tmpcount * numpageareas + slot;
							edg->target = count * numpageareas;
							edg->penalty = val;
							edges.AddTail(edg);
						}
#endif
					}
					else
					{
						// then we are in the middle of the calculation ....
						// there is at least one previous entry in the myentvect (which is the slice at count)  .....
						entryvect * tovect = mybreakvect->Get(tmpcount);

						int k;
						const int kmini = myentvect->GetMinimum();
						const int kmaxi = myentvect->GetMaximum();

						for( k = kmini; k <= kmaxi; ++k )
						{
							Entry * ent = myentvect->Get(k);
							if (ent)
							{
								// this is one of the previous entries ....
								
								// find the correct entry ...
								// This is the "standard" distance ... I have to get the correct distance ... but I am not sure of the changes ....
								// 1.5*LSPACE is the default distance .... it might be enhanced with springs
								// that stretch the systems on the page ...
							//	float alterheight = sliceheight.getHeight();
								float myheight =  sliceheight.getHeight() +
									//curheight + 
									ent->curheight + settings.systemsDistance;
								int slot = -1;
								int ispagebreak = 0;
								if (myheight > pageheight)
								{
									ispagebreak = 1;
									myheight = sliceheight.getHeight(1); 
										 // curheight;
									if (myheight > pageheight)
										// then, the cursystem will not fit on a single page ....
										// ...  
										myheight = pageheight;
								}
								
								slot = (int)(floor(((float) myheight) / pageheight * (numpageareas-1) + 0.5));
								assert(slot < numpageareas );								
								// slot = slot % numpagebreaks;
								Entry * myent = tovect->Get(slot);
								if (myent)
								{
								}
								else
								{
									myent = new Entry;
									myent->penalty = (float)1e9;
									myent->precessorcount = -1;
									tovect->Set(slot,myent);
								}
								if (val + ent->penalty < myent->penalty)
								{
									myent->penalty = val + ent->penalty;
									myent->precessorcount = count;
									myent->precessorslot  = k;
									myent->curheight = myheight;
									myent->pagebreaktoprev = ispagebreak;
								}
							}
						}
					}
				}
				else if (val < 0)
				{
						break;
				}
			}
		}
		inBeginHeight = 0;
		mSystemSlices->GetNext(pos);
	}

	// myoutfile.close();

#ifdef BREAKVECT
//#ifdef _DEBUG
	// we output the breakvect ....
	// into a graph-file (graphlet-format)
	ofstream myoutfile("breakvect.gml");
	myoutfile << "graph [" << endl;
	myoutfile << "directed 1 " << endl;
	myoutfile << "node [ " << endl;
	myoutfile << " id 0 " << endl;
	myoutfile << " label \"0/0/0\"" << endl;
	myoutfile << " ] " << endl;
	int cnt;
	for (cnt=mybreakvect->GetMinimum();cnt<=mybreakvect->GetMaximum();cnt++)
	{
		entryvect * tmpentryvect = mybreakvect->Get(cnt);
		if (tmpentryvect && tmpentryvect->GetCount()>0)
		{

			// myoutfile << "Slice : " << cnt << endl;
			int cnt2;
			for (cnt2 = tmpentryvect->GetMinimum();
			               cnt2<=tmpentryvect->GetMaximum();cnt2++)
			{
				Entry *ent = tmpentryvect->Get(cnt2);
				if (ent)
				{
					int thisid = cnt * numpageareas + cnt2;
					myoutfile << "node [" << endl;
					myoutfile << "id " << thisid  << endl;
					myoutfile << "label \"" << cnt;
					if (numpageareas>1)
						myoutfile << "/" << cnt2;
					myoutfile << "/" << ent->penalty << "\"" << endl; // Should be  "\" ?
					myoutfile << "graphics [ " << endl;
					myoutfile << "center [ " << endl;
					myoutfile.setf(ios::showpoint);
					myoutfile << " x " << (580.0/numpageareas * cnt2) - 280.0  << endl;
					myoutfile << " y " << (600.0/numslices * cnt) - 300.0  << endl;
					myoutfile << " z 0.0 " << endl;
					myoutfile << " ] " << endl;
					myoutfile << " ] " << endl;
					myoutfile << " vgj [ " << endl;
					myoutfile << " labelPosition \"in\"" << endl;
					myoutfile << " shape \"Rectangle\"" << endl;
					myoutfile << " ] " << endl;
					myoutfile << "] " << endl;

#undef ALL_EDGES
#ifndef ALL_EDGES
					if (ent->precessorcount > 0)
					{
						int targetid = ent->precessorcount * numpageareas + 
							ent->precessorslot;
						assert(thisid > targetid);
						myoutfile << "edge [ " << endl;
						myoutfile << " source " << thisid << endl;
						myoutfile << "target " << targetid << endl;
						myoutfile << "label \"";
						float pen = ent->penalty;
						if (targetid != 0)
						{
							entryvect * tmpentryvect2 = mybreakvect->Get(ent->precessorcount);
							if (tmpentryvect2)
							{
								Entry * tmpentry2 = tmpentryvect2->Get(ent->precessorslot);
								if (tmpentry2)
									pen -= tmpentry2->penalty;
							}
							
						}
						myoutfile << pen << "\"" << endl;
						myoutfile << " ]" << endl;
					} 
#endif
				}
			}
		}
	}

#ifdef ALL_EDGES
	GuidoPos mypos = edges.GetHeadPosition();
	while (mypos)
	{
		edge *edg = edges.GetNext(mypos);
		if (edg)
		{
						myoutfile << "edge [ " << endl;
						myoutfile << " source " << edg->target << endl;
						myoutfile << "target " << edg->source << endl;
						myoutfile << " label \"" << edg->penalty << "\"" << endl;
						myoutfile << " ]" << endl;

		}
	}

#endif // ALL_EDGES
	myoutfile << " ] " << endl;
	myoutfile.close();
	edges.RemoveAll();
#endif // BREAKVECT

	// now we are done and can retrieve the breaksequence ....
	breaklist * mybreaklist = new breaklist(1); // owns elements ...

	int number = numslices;
	entryvect * myentvect = mybreakvect->Get(number);
	Entry * ent = nullptr;
	if (inPageOrSystemBreak == 1)
	{
		ent = myentvect->Get(0);
	}
	else
	{
		// look for the highest entryvect (the one, that fills most of the page ....)
		// we do this by looking at the top 20% of the page-area and take the one with the best penalty ....
		float kminpagepenalty = (float)1e9;
		int kminpage = -1;
		int kminpagenumlines = -1;
		int searchbegin = int ( (float) numpageareas * 0.6f ) ;
		int mycnt;
		for (mycnt = searchbegin;mycnt <= numpageareas ;mycnt++)
		{
			Entry * tmpent = myentvect->Get(mycnt);
			if (tmpent)
			{
				int tmpentnumlines = tmpent->getPageNumlines(*mybreakvect);
				if (tmpent->penalty < kminpagepenalty
					|| tmpentnumlines > kminpagenumlines)
				{
					kminpage = mycnt;
					kminpagepenalty = tmpent->penalty;
					kminpagenumlines = tmpentnumlines;
				}
			}
		}
		
		// This looks for the overall minimum ....
		int k;
		int kmin = -1;
		float penmin = (float)1e9;
		
		const int entMin = myentvect->GetMinimum();
		const int entMax = myentvect->GetMaximum();
		for( k=entMin; k <= entMax; ++k )
		{
			Entry * tmpent = myentvect->Get(k);
			if (tmpent)
			{
				if (tmpent->penalty < penmin)
				{
					kmin = k;
					penmin = tmpent->penalty;
				}
			}
		}
		
		int lineoptimum = 0;
		int pageoptimum = 1;
		
		if (lineoptimum)
			ent = myentvect->Get(kmin);
		else if (pageoptimum)
		{
			ent = myentvect->Get(kminpage);
		}
		
		if (ent == nullptr)
		{
			// we just take the minimum entry ....
			ent = myentvect->Get(kmin);
		}
	}

	while (ent)
	{
		if (ent->precessorcount >= 0)
		{
			breakentry * breakent = new breakentry;
			breakent->begslice = ent->precessorcount + 1;
			breakent->numslices = number - ent->precessorcount;
			breakent->followedbypagebreak = ent->pagebreaktoprev;

			mybreaklist->AddHead(breakent);
		}
		number = ent->precessorcount;
		myentvect = mybreakvect->Get(ent->precessorcount);
		ent = myentvect->Get(ent->precessorslot);
	}

	float retval = 0;
	pos = mybreaklist->GetHeadPosition();
	GRSystemSlice * beginslice = 0;
	const GRSystemSlice * lastslice = 0;
traceslice(cout << ">>>> GRStaffManager::FindOptimumBreaks  =>  start pos loop" << endl);
	while (pos)
	{
		breakentry * breakent = mybreaklist->GetNext(pos);
		// we should break here .....

		if (breakent->followedbypagebreak && mGrSystem)
		{
			GRPage * newpage = new GRPage( mGrMusic, this, mGrSystem->getRelativeEndTimePosition(), settings, mGrPage );
			mGrPage->finishPage();
			mGrPage = newpage;
			mGrMusic->addPage( mGrPage );
//			mGrSystem->setGRPage( mGrPage ); // "Last system points to new page" bug!?
		}

		TYPE_TIMEPOSITION tp; 
		if (lastslice)
		{
			// then we have a preceeding last slice .... this must be used to construct the begin-slice ....
			// the mSpringVector must be supplied, so that the new elements get "real" springs ....
traceslice(cout << "GRStaffManager::FindOptimumBreaks  =>  CreateBeginSlice" << endl);
			beginslice = CreateBeginSlice(lastslice);
			tp = lastslice->getRelativeTimePosition();
			// this handles the open tags of a previous slice ...
			ResumeOpenTags(lastslice,beginslice);
		}
		else
		{
			tp = mSystemSlices->GetHead()->getRelativeTimePosition();
		}

		mGrSystem = new GRSystem(this, mGrPage, tp, &mSystemSlices, breakent->numslices, beginslice, &mSpringVector, mCurSysFormat,
								 settings.force, settings.spring, settings.proportionalRenderingForceMultiplicator,
								pos == nullptr && inPageOrSystemBreak == 0);
		
		if( ! mCurAccoladeTag.empty() )	
		{
			for(std::vector<ARAccolade *>::const_iterator it = mCurAccoladeTag.begin(); it < mCurAccoladeTag.end(); it++)
			{
				mGrSystem->notifyAccoladeTag( *it );	
			}
		}

		mCurSysFormat = nullptr;
		beginslice = nullptr;
		lastslice = mGrSystem->lastSlice();
traceslice(cout << "GRStaffManager::FindOptimumBreaks  =>  get lastslice: " << lastslice << endl);

		// we can only add the system on the page, if it would still fit ... 
		// maybe this will change whem optimum pagebreaks are implemented ?
		mGrPage->addSystem(mGrSystem, &usedsystemdistance);
		retval = mGrSystem->getPosition().y + mGrSystem->getBoundingBox().bottom;
	}
traceslice(cout << ">>>> GRStaffManager::FindOptimumBreaks  =>  end pos loop" << endl);

	delete mybreaklist;

	// there is something left to break. this only happends, if a forced system break
	// must occur .... this just takes whatever is left and pushes it into the current line ....
	if (mSystemSlices->size() > 0)
	{
		// we take everything that is left .... 
		TYPE_TIMEPOSITION tp ( mSystemSlices->GetHead()->getRelativeTimePosition());

		mGrSystem = new GRSystem(this, mGrPage, tp, &mSystemSlices, mSystemSlices->GetCount(),
					nullptr, &mSpringVector, mCurSysFormat,
					settings.force, settings.spring, settings.proportionalRenderingForceMultiplicator,
					// is last system
					inPageOrSystemBreak == 0);
		
		if( ! mCurAccoladeTag.empty() )	
		{
			for(std::vector<ARAccolade *>::const_iterator it = mCurAccoladeTag.begin(); it < mCurAccoladeTag.end(); it++)
			{
				mGrSystem->notifyAccoladeTag( *it );	
			}
		}
		mCurSysFormat = nullptr;
			
		if (!mGrPage->addSystem(mGrSystem, &usedsystemdistance))
		{
			assert(false);
			// build a new page and put the system in the new page
			GRPage * newpage = new GRPage(mGrMusic, this, mGrSystem->getRelativeTimePosition(), settings, mGrPage);
			mGrPage->finishPage();
			mGrPage = newpage;
			mGrMusic->addPage(mGrPage);
			mGrSystem->setGRPage(mGrPage);
			float tmpf = -1.0f;
			mGrPage->addSystem(mGrSystem, &tmpf);

		}
		retval = mGrSystem->getPosition().y + mGrSystem->getBoundingBox().bottom;
	}

	if (mGrPage)
		mGrPage->finishPage( true );// this is definitely the last page

	delete mybreakvect;
	return retval;
}

const GRBeginSpaceForceFunction2 * GRStaffManager::getCurrentBegSFF() const
{
    if (beg_sff_list)
        return beg_sff_list->GetTail();
    return 0;
}

/** \brief Called by staves, when they need
to add the start- and end-glue to the Springs.
a springid of 0 is the beginning, a springid
of -1 means the last springid available.
*/
void GRStaffManager::addElementToSpring(GRNotationElement * grne, int springid)
{
    int dorecalc = 0;
    assert(springid == 0 || springid == -1);
    if (springid == 0)
    {
        dorecalc = 1;
        springid = mSpringVector->GetMinimum();
    }
    else
        springid = mSpringVector->GetMaximum();
    GRSpring *spr = mSpringVector->Get(springid);

    spr->addElement(grne, nullptr);
    grne->tellSpringID(spr->getID());

    if (dorecalc)
        spr->recalcConstant();
}

/** \brief Stretches the springs with IDs in the range between start and end.

The rods, that (partially) cover over these springs are used for the initial-stretch.
If a spring in this range has an initial stretch it is reset.
*/
float GRStaffManager::InitialSpringStretch(int start, int end,
    IRodList * tmpsimplerods,
    IRodList * tmpcomplexrods,
    ISpringVector * tmpsprvect,
    ofstream * pspringlog)
{
    assert(tmpsimplerods);
    assert(tmpcomplexrods);
    assert(tmpsprvect);

    // First, stretch the springs with the simple-Rods ...
    GuidoPos pos = tmpsimplerods->GetTailPosition();
    while (pos)
    {
        GRRod *rod = tmpsimplerods->GetPrev(pos);
        if (rod->getSpr1() >= start && rod->getSpr2() <= end)
        {
            GRSpring *spr = tmpsprvect->Get(rod->getSpr1());
            if (spr) spr->setlength(rod->getLength());
        }
        else if (rod->getSpr1() < start)
            break;
    }

    // Then we need to do the interplay of strechting the rods covering more than one spring ...
    IRodList * rodlst = new IRodList(0);
    pos = nullptr;
    if (tmpcomplexrods)
        pos = tmpcomplexrods->GetTailPosition();

    while (pos)
    {
        GRRod * rod = tmpcomplexrods->GetPrev(pos);
        if ((rod->getSpr1() >= start && rod->getSpr1()<end)
            || (rod->getSpr2() > start && rod->getSpr2() <= end)
            || (rod->getSpr1() <= start && rod->getSpr2() >= end))
            rodlst->AddHead(rod);
    }


    // now we have the rods that are in the area and conver more than one spring ...
    // as long as there are Elements in the rodlist ...
    // fl owns the entries.
    KF_IPointerList<GRForceRodEntry> forceList(1);

    do
    {
        // calculate the force neccessary to stretch springs to the desired length ...
        pos = rodlst->GetHeadPosition();
        while (pos)
        {
            GRRod *rod = rodlst->GetAt(pos);
            float force = rod->calcforce(tmpsprvect);
            if (force > 0)
            {
                GRForceRodEntry * fre = new GRForceRodEntry;
                fre->rod = rod;
                fre->force = force;
                forceList.AddTail(fre);
                forceList.sort(GRForceRodEntry::comp);
            }
            int ishead = 0;
            if (pos == rodlst->GetHeadPosition())
                ishead = 1;
            int istail = 0;
            if (pos == rodlst->GetTailPosition())
                istail = 1;
            GuidoPos savepos = pos;
            if (!ishead && !istail)
                rodlst->GetNext(pos);
            rodlst->RemoveElementAt(savepos);
            if (ishead)
                pos = rodlst->GetHeadPosition();
            else if (istail)
                pos = nullptr;
        }

        // the force list must be sorted by force and also by space-rod and spr1 ...
        // (a space rod with a smaller start spring must have a priority over the ones with 
        // bigger springs; after the space-rods, the other rods are sorted by force ...
        // the maximum is at the end ...
        GRForceRodEntry * fre = forceList.RemoveTail();
        if (!fre || fre->force == 0.0)
        {
            delete fre;
            break;
        }

        // This is a very weird hack ... have to look into this
        // 846_f2.gmn
        if (fre->rod->getSpr1() >= start && fre->rod->getSpr2() <= end)
            fre->rod->stretchsprings(fre->force, tmpsprvect);

        // now, we "know", which springs are streteched ....
        GuidoPos tmppos = forceList.GetHeadPosition();
        int changed = 0;
        while (tmppos)
        {
            GRForceRodEntry *tmpfre = forceList.GetAt(tmppos);
            GRRod *rod = tmpfre->rod;
            if (rod->resetForce(*fre->rod))
            {
                tmpfre->force = rod->calcforce(tmpsprvect);
                if (tmpfre->force == 0.0)
                {
                    int ishead = 0;
                    if (tmppos == forceList.GetHeadPosition())
                        ishead = 1;
                    int istail = 0;
                    if (tmppos == forceList.GetTailPosition())
                        istail = 1;
                    GuidoPos savepos = tmppos;
                    if (!ishead && !istail)
                        forceList.GetNext(tmppos);
                    forceList.RemoveElementAt(savepos);
                    if (ishead)
                        tmppos = forceList.GetHeadPosition();
                    else if (istail)
                        tmppos = forceList.GetTailPosition();
                }
                else
                {
                    forceList.GetNext(tmppos);
                    changed = 1;
                }

            }
            else
                forceList.GetNext(tmppos);
        }
        if (changed)
            forceList.sort(GRForceRodEntry::comp);

        delete fre;
    } while (!forceList.IsEmpty());
    delete rodlst;

    // now we are done (the springs are stretched) hopefully ...
    return 0;
}

/** \brief  AddGRSyncElement is called by the GRVoiceManager-class to add those elements, that
need horizontal synchronization.

It uses a temporary spring-id and a data-structure, to keep track of equivalent tags.
When a slice is finished, FinishSyncSlice is called, which handles spring-ID-distribution and
spring-stretching.
The routine works as follows:
If the notationelement is a tag or an event with duration 0, than the type is looked up in a hash-table.
The hash-table has entries of the following type: (type,(grnotationelement,staff,voiceID),mSpringID):

If there is no element of this type, the temporary springid is incremented, and the type (and
element) is added to the hash-table. The entry voice-id of the voice-array is set to the new springid.

If there is an element of this type, it is checked, whether voice-array[voiceID] is
greater than the mSpringID of the entry of the hash-table. In this case, the mSpringID is
incremented and the hashtable-entry is replaced by a new entry. voice-array[voice-id] is set to the
new spring-id.
(There is a special case here: if the current springIDs of ALL voices in the old hash-table-entry are
smaller or equal to the mSpringID of the old hash-table-entry, than the old mSpringID
is REMOVED, that is, all the notationelements are moved to the new hash-table-entry. Each respective
voice-arr[voice-id] entry is set to the new mSpringID).
If the voice-array[voice-id] is smaller than the temporary mSpringID, the notation-element is added to the
same hash-table-entry. The mSpringID of voice-array[voice-id] is set to the value in the hash-table-entry.
*/
int GRStaffManager::AddGRSyncElement(GRNotationElement * grel, GRStaff * grstaff, int voiceID,
    GRVoice * vce, GRNotationElement * sameel)
{
    if (!grel) return -1;

    if (grel->getNeedsSpring() == -1)
    {
        // then, the element has been added with a sharelocation or a sharestem-tag in mind.
        // associated the element with an already assigned spring ...
        // In this case, voiceID is really a mSpringID!!!!
        // then there has been no spring assigned yet. we have to find the realspringid
        GRSpring * spr = 0;
        if (voiceID == -1)
        {
            // then we use sameel ....
            assert(sameel != 0);
            const type_info &ti = typeid(*sameel);

            // convert it, so it can be looked up in the hash-table
            NVstring tmp(ti.name());
            HashEntry tmphash;

            GuidoPos syncpos = syncHash.Lookup(tmp, tmphash);
            if (syncpos != nullptr)
            {
                SubHash * sh = new SubHash();
                sh->grel = grel;
                sh->grstaff = grstaff;
                sh->voiceID = voiceID;
                sh->voice = vce;
                tmphash.data->AddTail(sh);
            }
            else {
                // we have a problem ....
            }
        }
        else
        {
            spr = mSpringVector->Get(voiceID);
        }

        if (spr)
        {
            spr->addElement(grel, vce);
            grel->tellSpringID(spr->getID());
        }
        grel->setNeedsSpring(0);
        // spr->checkLocalCollisions();
        return -1;
    }
    if (!grel->getNeedsSpring()) return -1;

    // assert(grel->getDuration() == DURATION_0);

    GREvent * theEv = GREvent::cast(grel);
    if (theEv && grel->getDuration() > DURATION_0)
    {
        // here we deal with the events ...
        GREvent * grev = theEv;
        VoiceEvent * ve = new VoiceEvent;
        ve->ev = grev;
        ve->vce = vce;
        evlist.AddTail(ve);
    }
    else
    {
        // and here, we deal with tags ...
        const type_info & ti = typeid(*grel);

        // convert it, so it can be looked up in the hash-table
        NVstring tmp(ti.name());
        HashEntry tmphash;

        GuidoPos syncpos = syncHash.Lookup(tmp, tmphash);
        if (syncpos != nullptr)
        {
            // then, the entry is already there ...			
            // now, we have to check, wether the voice has a greater or equal current spring-id then the entry ...			
            if (voiceSpringArr.Get(voiceID) >= tmphash.mSpringID)
            {
                // now we need to check, wether ALL voices have smaller/equal current voiceIDs as
                // the mSpringID of the hash-entry.
                // in this case, the spring-ID is set to the bigger number ....
                bool found = false;
                // check, wether we are in the same voice ...				
                GuidoPos tmppos = tmphash.data->GetHeadPosition();
                while (tmppos)
                {
                    SubHash *sh = tmphash.data->GetNext(tmppos);
                    if (voiceSpringArr.Get(sh->voiceID)>tmphash.mSpringID || sh->voiceID == voiceID)
                    {
                        found = true;
                        break;
                    }
                }
                if (found)
                {
                    // there must be a new entry
                    // we remove the key, so that the entry is no longer recognized but will be found later...
                    syncHash.Set(syncpos, "", tmphash);

                    // owns elements
                    SubHashList *shl = new SubHashList(1);
                    SubHash *sh = new SubHash();
                    sh->grel = grel;
                    sh->grstaff = grstaff;
                    sh->voiceID = voiceID;
                    sh->voice = vce;
                    shl->AddTail(sh);

                    tmphash.data = shl;
                    tmphash.mSpringID = ++mTempSpringID;
                    voiceSpringArr.Set(voiceID, mTempSpringID);
                    syncHash.Set(tmp, tmphash);
                }
                else
                {
                    // we can replace the springIDs. remember to set the voidSpringArr-IDs
                    tmphash.mSpringID = ++mTempSpringID;
                    SubHash * sh = new SubHash();
                    sh->grel = grel;
                    sh->grstaff = grstaff;
                    sh->voiceID = voiceID;
                    sh->voice = vce;
                    tmphash.data->AddTail(sh);

                    // this sets the tmphash (because of changed mSpringID)
                    syncHash.Set(syncpos, tmp, tmphash);
                    GuidoPos tmppos = tmphash.data->GetHeadPosition();
                    while (tmppos)
                    {
                        SubHash * sh = tmphash.data->GetNext(tmppos);
                        voiceSpringArr.Set(sh->voiceID, mTempSpringID);
                    }
                }
            }
            else
            {
                // is smaller; just add as an entry ...
                SubHash * sh = new SubHash();
                sh->grel = grel;
                sh->grstaff = grstaff;
                sh->voiceID = voiceID;
                sh->voice = vce;
                tmphash.data->AddTail(sh);
                voiceSpringArr.Set(voiceID, tmphash.mSpringID);
            }
        }
        else
        {
            // no entry yet
            ++mTempSpringID;

            // create a new SubHash
            SubHash * sh = new SubHash();
            sh->grel = grel;
            sh->grstaff = grstaff;
            sh->voiceID = voiceID;
            sh->voice = vce;

            // owns elements ...
            SubHashList * shl = new SubHashList(1);
            shl->AddTail(sh);

            tmphash.data = shl;
            tmphash.mSpringID = mTempSpringID;

            // this sets the SpringID of the voice
            voiceSpringArr.Set(voiceID, mTempSpringID);
            syncHash.Set(tmp, tmphash);
        }
    } // if duration_0
    return 0;
}

/** \brief Called by the VoiceManager to add a SystemTag to the score.

A System tag (like e.g. \\tempo) is valid for the whole system. Each time, a SyncSlice is finished,
the collected systemtags must be checked for contraditing entries.
The Systemtags must also be assigned to already present springs.
*/
int GRStaffManager::AddSystemTag(GRNotationElement * grel, GRStaff * grstaff, int voiceid)
{
    // first, we have to look, wether we already have an entry of the given type at the current time.
    // if so, it is ignored (only one System-Tag per SyncSlice).
    // if not present, we just add it.
    const type_info & ti = typeid(*grel);

    // convert it, so it can be looked up in the hash-table
    NVstring tmp(ti.name());
    GRNotationElement * mytag = 0;

    GuidoPos systempos = systemHash.Lookup(tmp, mytag);

    if (systempos != nullptr)
    {
        // there is an entry of this type already
        GuidoWarn("Another SystemTag of the same type is already used");
        delete grel;
    }
    else
        systemHash.Set(tmp, grel);
    return 0;
}

/** \brief Called by either the
GRVoice after being called in FinishSyncSlice
(grvoice->CreateRods()) or from the GRStaffs.

The procedure adds the given rod to the current ROD-List. They are being
saved for (initial) Spring-Stretching.
*/
int GRStaffManager::addRod(GRRod * rod, bool spaceactive, bool atHead, int voiceID)
{
    // maybe distinguish between One-Rods (rods that cover exactly one spring) ond more rods.
    // then, these Data-structures can more easily be sorted by Spring-IDs!
    if (spaceactive)
    {
        const int s1 = rod->getSpr1();
        const int s2 = rod->getSpr2();
        for (int i = s1; i < s2; ++i)
            mSpringVector->Get(i)->fIsfrozen = 1;
    }

    if (rod->spansOne())
    {
        if (atHead)
        {
            GRRod * firstrod = mSimpleRods->GetHead();
            if (firstrod && rod->operator<(*firstrod) == 1)
                mSimpleRods->AddSortedHead(rod, &rodpcomp);
            else
                mSimpleRods->AddHead(rod);
        }
        else
        {
            GRRod * lastrod = mSimpleRods->GetTail();
            if (lastrod && lastrod->operator<(*rod) == 1)
                mSimpleRods->AddSortedTail(rod, &rodpcomp);
            else
                mSimpleRods->AddTail(rod);
        }
    }
    else
    {
        if (atHead)
        {
            GRRod * firstrod = mComplexRods->GetHead();
            if (firstrod && rod->operator<(*firstrod) == 1)
                mComplexRods->AddSortedHead(rod, &rodpcomp);
            else
                mComplexRods->AddHead(rod);
        }
        else
        {
            GRRod * lastrod = mComplexRods->GetTail();
            if (lastrod && lastrod->operator<(*rod) == 1)
                mComplexRods->AddSortedTail(rod, &rodpcomp);
            else
                mComplexRods->AddTail(rod);
        }
    }
    return 0;
}

/** Take care of breaking at cnt (number of slices for the new system).
Check, which grtags are still open and handle these by
setting the end
*/
void GRStaffManager::TakeCareOfBreakAt(GRSystem * newsys)
{
    const GRSystemSlice* mylastslice = newsys->lastSlice();
    // now we retrieve the open grtags ....
    GRPossibleBreakState * pbs = mylastslice->mPossibleBreakState;
    if (pbs) {
        const int mini = pbs->vtsvect->GetMinimum();
        const int maxi = pbs->vtsvect->GetMaximum();
        for (int i = mini; i <= maxi; ++i) {
            // These are the GRVoiceTagsAndStaff
            GRPossibleBreakState::GRVoiceTagsAndStaff * vts = pbs->vtsvect->Get(i);
            if (vts && vts->grtags)
            {
                // this closes the currently open grtags ...
                // it assumes, that there isan EndGlue() ....
                // (but that is OK, we have one ...)
                // someone should tell the lastslice, that the staffs should have a valid endglue ....
                vts->EndAtBreak();
            }
        }
    }
}

/** \brief Called when there has been either a newSystem, newPage or a potential break.

The routine is responsible of creating the SFF and putting it together with the system-slice.
later. the line-breaking-routine will determine, which slices to add together to form a system.
old commentary:
this procedure is called, whenever a Potential breakpoint has been encountered.
In this case, the needed Force needs to be calculated.
A new SPF has to be started at each PBreak; also, the VoiceState hast to be remembered
(that is GRVoiceManager, including curvst and grtags ...)
*/
GRSpaceForceFunction2 * GRStaffManager::BuildSFF()
{
    // now we need to go through the voices and build the rods, so that we can initially stretch the springs ...
    // now we need to build the Rods (first the voice-rods, than the staff-rods (for collision-removal))

    // foreach voice voice->createRods()
    // foreach staff staff->createRods()

    GRSpaceForceFunction2 * sff = new GRSpaceForceFunction2(settings.force);
    GRSpacingMatrix *		spm = new GRSpacingMatrix();

    if (kIsGiesekingSpacing) {
        if (spm->getMSCMatrixRealSize() < mSpringID)
            spm->resizeMSCMatrix(mSpringID);
        if (spm->getMSCMatrixRealSize() < mLastSpringID)
            spm->resizeMSCMatrix(mLastSpringID);
    }

    // This is important, so that the springs have correct durations for the QP-Matrix...
    // and it is also important, to test, wether we need SPECIAL Spacing, or just regular Gourlay-Spacing ....

    // this just adjust the spring-duration to the correct value. This is important, because
    // we need this value for later determining the correct spring-constant.
    for (int myi = mLastSpringID; myi < mSpringID; ++myi) {
        GRSpring * spr1 = mSpringVector->Get(myi);
        GRSpring * spr2 = mSpringVector->Get(myi + 1);
        if (spr1 && spr2) {
            const TYPE_DURATION dur(spr2->getTimePosition() - spr1->getTimePosition());
            spr1->change_dur(dur);
        }
    }

    // for each voice, the rods are created.
    const int mini = mVoiceMgrList->GetMinimum();
    const int maxi = mVoiceMgrList->GetMaximum();
    for (int i = mini; i <= maxi; ++i)
    {
        int tmpstart = mLastSpringID;
        int tmpend = mSpringID;
        if (kIsGiesekingSpacing) {
            if (mVoiceMgrList->Get(i) && mVoiceMgrList->Get(i)->getGRVoice()) {
                mVoiceMgrList->Get(i)->getGRVoice()->updateMSCMatrix(sff, spm, this, mSpringVector, tmpstart, tmpend);
            }
        }
        // the voiceManager gets the voice and calls createNewRods to create rods ...
        if (mVoiceMgrList->Get(i) && mVoiceMgrList->Get(i)->getGRVoice())
            mVoiceMgrList->Get(i)->getGRVoice()->createNewRods(this, tmpstart, tmpend, settings.force);
    }

    // now we can ask the spacing-matrix to check on the neighbourhood ...
    // this deals with the equal spacing of neighboring notes.
    if (settings.neighborhoodSpacing)
        spm->CheckNeighbours(mSpringVector, settings.spring);

    // now we additionally build the staff-rods?! these are needed for collision-detection and prevention.
    // Idea: The staff is traversed. For each element in the staff, that shares the same mSpringID it is determined, if there is a collision.
    // If this is the case, the elements are MOVED (how?) and a rod is created for the spring .... somewhat similar to GRVoice->createNewRods ....
    // another idea is to traverse the springs and find elements that share the spring. Then assure, that I have at least enough space left and right.
    // Add collision prevention later ...
    createNewSystemRods(mLastSpringID, mSpringID);

    // actually, the strechting starts at mLastSpringID ...
    // now we do the stretching, starting from to springIDs returned by
    // createRods. (these springs loose their initial-Stretch-entry)
    // we need to check the constans one more time ...
    // the durations inbetween the springs are importnat, not at the springs themselves ....
    // this the old spacing implied by lippold haken
    if (!kIsGiesekingSpacing)
    {
        for (int i = mLastSpringID; i<mSpringID; i++) {
            GRSpring * spr1 = mSpringVector->Get(i);
            GRSpring * spr2 = mSpringVector->Get(i + 1);
            if (spr1 && spr2) {
                const TYPE_DURATION dur(spr2->getTimePosition() - spr1->getTimePosition());
                //cout << "GRStaffManager::BuildSFF change_dur B: " << dur << endl;
                spr1->change_dur(dur);
            }
        }
    }
    else {
        // this can be used to turn on and off lilypond spacing. 
        int lilypondspacing = 0;

        // this is the Gieseking model
        for (int i = mLastSpringID; i<mSpringID; i++) {
            double max = 0;
            GRSpring * spr1 = mSpringVector->Get(i);
            GRSpring * spr2 = mSpringVector->Get(i + 1);
            if (spr1 && spr2) {
                TYPE_DURATION dur(spr2->getTimePosition() - spr1->getTimePosition());
                // instead of just taking the durations, we build the maximum of all values that are part of the first spring ...
                if (dur <= DURATION_0)
                    continue;

                double tmpmatdur = spm->getMSCMatrix(i, 0);
                double tmpmatdur2 = spm->getMSCMatrix(i, 3);
                // this is the mean-value!
                double tmpmatdur3 = spm->getMSCMatrix(i, 4);
                if (settings.neighborhoodSpacing) {
                    max = ((double)dur) * tmpmatdur3;
                    if (max > 0) {
                        double sconst;
                        // we try the second possibility ... this should word?
                        double invert = 1.0 / tmpmatdur3;
                        sconst = (double)GRSpring::defconst((float)invert, settings.spring);
                        sconst *= invert / (double)dur;
                        spr1->change_const((float)sconst);
                        continue;
                    }
                }

                // then we have the gieseking-model (gourlay)
                {
                    max = ((double)dur) * tmpmatdur;
                    if (max>0 && !lilypondspacing) {
                        double sconst;
                        int durnumerator = (int)spm->getMSCMatrix(i, 1);
                        int durdenominator = (int)spm->getMSCMatrix(i, 2);
                        TYPE_DURATION eldur(durnumerator, durdenominator);

                        sconst = GRSpring::defconst(eldur, settings.spring);
                        sconst *= (double)eldur / (double)dur;
                        spr1->change_const((float)sconst);
                    }
                    else {
                        // then, this is lilypond-spacing ...
                        double sconst;
                        // we try the second possibility ...
                        max = ((double)dur) * tmpmatdur2;
                        if (max>0) {
                            // this sould word?
                            TYPE_DURATION myfrac(tmpmatdur2);
                            myfrac.invert();
                            sconst = GRSpring::defconst(myfrac, settings.spring);
                            sconst *= (double)myfrac / (double)dur;
                            spr1->change_const((float)sconst);
                        }
                    }
                }
            }
        }
    }

    // then we can delete the spacing-matrix ...
    delete spm;

    // this determines, wether the last spring of the current slice is frozen (which means, it is a spring for a bar-line).
    /*int isfrozen =*/ CheckForBarSpring(mSpringID - 1);
    // this is the function, that actually stretches the strings according to the rods.
    InitialSpringStretch(mLastSpringID, mSpringID, mSimpleRods, mComplexRods, mSpringVector);

    // Last spring (previous end-spring) needs to be restreched ... only
    // The new springs are really added ... now we can add the springs to the SpaceForce function
    for (int i = mLastSpringID; i < mSpringID; ++i) {
        // this fills the space-force-function
        sff->addSpring(mSpringVector->Get(i));
    }

    // now we can experiment with the spf ...
    return sff;
}

/** \brief Called when a clef or key changed
the beginning sff saves the SpaceForceFunction for a
potential beginning at the current state. This
saves the extent of a clef and a potential key.

Called whenever a clef or key changes in one of the voices.
This is a "signal" to the staff-manager that the beginning of a new
line could look different.
In this case, a new sff is created to reflect this. Each
systemslice gets a pointer to the current beginning sff.
this can then be used when finding optimum breaks.
*/
void GRStaffManager::UpdateBeginningSFF(int staffnum)
{

    // I have to iterate through the staves and find the current clefs and keys. 
    // I then have to create the SFF for that ....
    if (!mStaffStateVect)
        mStaffStateVect = new IStaffStateVector(0);

    bool needsupdate = false;
    int i;
    int mini = mMyStaffs->GetMinimum();
    int maxi = mMyStaffs->GetMaximum();

    for (i = mini; i <= maxi; i++)
    {
        GRStaff * staff = mMyStaffs->Get(i);
        if (staff)
        {
            mStaffStateVect->Set(i, &staff->getGRStaffState());

            if (i == staffnum)
            {
                GuidoPos pos = staff->mCompElements.GetTailPosition();
                // ONLY The last element is looked at!!!!
                if (pos)
                {
                    GRNotationElement * el = staff->mCompElements.GetPrev(pos);
                    GRClef * tmpclef = 0;
                    GRKey * tmpkey = 0;
                    if ((tmpclef = dynamic_cast<GRClef *>(el)) != 0)
                    {

                        const NVRect & tmprect = tmpclef->getBoundingBox();
                        float tmpposy = tmpclef->getPosition().y;

                        float tmptop = staffTopVector.Get(i);
                        float tmpbottom = staffBottomVector.Get(i);

                        if (tmprect.top + tmpposy < tmptop)
                        {
                            staffTopVector.Set(i, tmprect.top + tmpposy);
                        }
                        if (tmprect.bottom + tmpposy > tmpbottom)
                        {
                            staffBottomVector.Set(i, tmprect.bottom + tmpposy);
                        }

                        if (mMaxClef)
                        {
                            const NVRect & maxrect = mMaxClef->getBoundingBox();
                            if (tmprect.Width() > maxrect.Width())
                            {
                                mMaxClef = tmpclef;
                                needsupdate = true;
                            }
                        }
                        else
                        {
                            mMaxClef = tmpclef;
                            needsupdate = true;
                        }
                    }
                    if ((tmpkey = dynamic_cast<GRKey *>(el)) != 0)
                    {
                        const NVRect & tmprect = tmpkey->getBoundingBox();
                        float tmpposy = tmpkey->getPosition().y;

                        float tmptop = staffTopVector.Get(i);
                        float tmpbottom = staffBottomVector.Get(i);
                        if (tmprect.top + tmpposy < tmptop)
                        {
                            staffTopVector.Set(i, tmprect.top + tmpposy);
                        }
                        if (tmprect.bottom + tmpposy > tmpbottom)
                        {
                            staffBottomVector.Set(i, tmprect.bottom + tmpposy);
                        }

                        if (mMaxKey)
                        {
                            const NVRect & maxrect = mMaxKey->getBoundingBox();
                            if (tmprect.Width() > maxrect.Width())
                            {
                                mMaxKey = tmpkey;
                                needsupdate = true;
                            }
                        }
                        else
                        {
                            mMaxKey = tmpkey;
                            needsupdate = true;
                        }
                    }
                }
            }
        }
    }

    // now i use maxclef and mMaxKey to create the current sff .....
    if (needsupdate && mMaxClef && mMaxKey)
    {
        GRBeginSpaceForceFunction2 * bsff = new GRBeginSpaceForceFunction2(settings.force);
        // then I need a new sff ....
        // we need to save these bounding boxes (the heights as well ...)
        const NVRect & maxrectclef = mMaxClef->getBoundingBox();
        const float lengthclef = maxrectclef.Width();
        const NVRect & maxrectkey = mMaxKey->getBoundingBox();
        const float lengthkey = maxrectkey.Width();

        GRSpring * spr;
        spr = new GRSpring(DURATION_0, DURATION_0, settings.spring, settings.proportionalRenderingForceMultiplicator);
        spr->set_const(SCONST_GLUESTART);
        spr->setlength(lengthclef * 0.5f);
        beg_spr_list->AddTail(spr);
        bsff->addSpring(spr);

        spr = new GRSpring(DURATION_0, DURATION_0, settings.spring, settings.proportionalRenderingForceMultiplicator);
        spr->set_const(mMaxClef->getSConst());
        spr->setlength(lengthclef * 0.5f + lengthkey * 0.5f);
        beg_spr_list->AddTail(spr);
        bsff->addSpring(spr);

        spr = new GRSpring(DURATION_0, DURATION_0, settings.spring, settings.proportionalRenderingForceMultiplicator);
        spr->set_const(mMaxKey->getSConst());
        spr->setlength(lengthkey * 0.5f);
        beg_spr_list->AddTail(spr);
        bsff->addSpring(spr);

        spr = new GRSpring(DURATION_0, DURATION_0, settings.spring, settings.proportionalRenderingForceMultiplicator);
        spr->set_const(SCONST_GLUENOSTART);
        beg_spr_list->AddTail(spr);
        bsff->addSpring(spr);

#ifdef _DEBUG
        float extent = bsff->getExtent(bsff->getOptForce());
#endif

        mini = staffTopVector.GetMinimum();
        maxi = staffTopVector.GetMaximum();
        for (int cnt = mini; cnt <= maxi; ++cnt)
        {
            float tmptop = staffTopVector.Get(cnt);
            float tmpbottom = staffBottomVector.Get(cnt);
            if (tmptop < 0 || tmpbottom > 0)
            {
                bsff->setHeight(cnt, tmptop, tmpbottom);
            }
        }
        beg_sff_list->AddTail(bsff);
    }
}

/** \brief Creates the begin-slice for a given last slice of a previous line.
*/
GRSystemSlice * GRStaffManager::CreateBeginSlice(const GRSystemSlice * lastslice)
{
    // now I have to create the begin - slice given the last slice of the previous line
    // (which includes the staff-states for my current line ....
    // I start with basically nothing ... only the state information.
    // I need to synchronize everything .... This is somewhat connected
    // To CreateBeginElements ......

    // now, we travers the previously saved staves to create new staves with the respective
    // state (that is clef, key) added at the beginning
    GRPossibleBreakState * pbs = lastslice->mPossibleBreakState;
    int mini = pbs->ssvect->GetMinimum();
    int maxi = pbs->ssvect->GetMaximum();
    GRSystemSlice * beginslice = new GRSystemSlice(this, pbs->tp);
    for (int i = mini; i <= maxi; ++i)
    {
        GRPossibleBreakState::GRStaffAndState * sas = pbs->ssvect->Get(i);
        if (sas)
        {
            // The Staff-numbers are equal to the staff-vector at the breaktime.			
            GRStaff * newstaff = new GRStaff(beginslice, settings.proportionalRenderingForceMultiplicator);
            beginslice->addStaff(newstaff, i);

            // We apply potential staff size defined with GuidoSetStaffSize API call
            applyStaffSize(newstaff, i);

            // add the staffstate stuff ... the call to BeginStaff is done later, when we have
            // determined the number of springs that are needed by the New-Elements!
            newstaff->CreateBeginElements(this, sas->staffstate, i);
        }
    }

    // this sets the mSpringID to a number suitable for adding all needed Graphical Element 
    // (it takes into account the newly added elements (mTempSpringID-1) and the one StartGlue-Spring (1).
    const int mycount = syncHash.GetCount();

    mSpringID = lastslice->mEndSpringID + 1 - mycount - 1;
    beginslice->mStartSpringID = mSpringID;

    // this is the spring for the start-glue
    GRSpring * spr = new GRSpring(pbs->tp, DURATION_0, settings.spring, settings.proportionalRenderingForceMultiplicator);
    spr->setID(mSpringID);
    spr->change_const(50);
    mSpringVector->Set(mSpringID++, spr);

    // now, the sync-slice is finished, the springs are created and the elements get their springIDs
    FinishSyncSlice(pbs->tp);
    // assert, that we have not counted wrong.
    assert(mSpringID == pbs->springID);

    beginslice->mEndSpringID = mSpringID - 1;

    // at this point we remove the rods that might still exist from previous sff-creation process ....
    GuidoPos pos = mSimpleRods->GetHeadPosition();
    while (pos)
    {
        GuidoPos oldpos = pos;
        GRRod * rod = mSimpleRods->GetNext(pos);
        if (rod && rod->getSpr1() >= beginslice->mStartSpringID && rod->getSpr2() <= beginslice->mEndSpringID + 1)
            mSimpleRods->RemoveElementAt(oldpos);
    }

    mini = beginslice->mStaffs->GetMinimum();
    maxi = beginslice->mStaffs->GetMaximum();
    for (int i = mini; i <= maxi; ++i)
    {
        GRStaff * staff = beginslice->mStaffs->Get(i);
        if (staff)
        {
            // call beginStaff so that startglue is added, this calls  addElementToSpring for the startglue.
            // not yet needed ....?
            staff->BeginStaff(this);

            // now, we need to build the rods for the newly added elements for the staves. (these are staff-rods)
            int start = mSpringVector->GetMinimum();
            int end = mSpringID;
            staff->createNewRods(this, start, end, settings.force);

            // remember to set lastrod of staff to previous lastrod a little bit unsure of this ...
            // staff->lastrod = mMyStaffs->Get(i)->lastrod;
        }
    }

    // now we need to stretch the initialsprings including the one at the remainder note, that all the springs
    // have just been created, so they do not need to be reset to length 0.0;
    InitialSpringStretch(mSpringVector->GetMinimum(), mSpringID, mSimpleRods, mComplexRods, mSpringVector);

    GRSpaceForceFunction2 * newForceFunc = new GRSpaceForceFunction2(settings.force);
    beginslice->mForceFunction = newForceFunc;

    // now, we have to add the start springs (those, that were just created) to the curspf ...
    mini = mSpringVector->GetMinimum();
    for (int i = mini; i < mSpringID; ++i)
    {
        newForceFunc->addSpring(mSpringVector->Get(i));
    }
    return beginslice;
}

/** \brief Creates rods beginning with
the spring startid and ending with spring endid.
the springvector is traversed and rods are
inserted so that minimum distances are preserved.
*/
void GRStaffManager::createNewSystemRods(int startid, int endid)
{
    firstrod = nullptr;
    lastrod = nullptr;
    int i;
    KF_Vector<int> dvect(0);
    for (i = startid; i<endid; i++)
    {
        //int dright = 0;
        //int dleft  = 0;
        GRSpring * spr = mSpringVector->Get(i);
        spr->checkLocalCollisions();
        GRSpring * spr2 = nullptr;
        if (i<endid - 1)
            spr2 = mSpringVector->Get(i + 1);

        if (spr && spr2)
        {
            float distance = spr->GetMaxDistance(spr2);
            if (distance > 0)
            {
                GRRod * rod = new GRRod(distance, i, i + 1, settings.force);
                if (!firstrod)
                    firstrod = rod;
                lastrod = rod;
                addRod(rod, 0);
            }
        }
    }
}

int GRStaffManager::CheckForBarSpring(int sprid)
{
    if (mSpringVector)
    {
        GRSpring * spr = mSpringVector->Get(sprid);
        if (spr)
        {
            if (dynamic_cast<const GRBar *>(spr->getFirstGRO())
                || dynamic_cast<const GRRepeatEnd *>(spr->getFirstGRO()))
                //			|| dynamic cast<const GREmpty *>(spr->getFirstGRO()))
            {
                //				spr->isfrozen = 1;
                //				return 1;
                return 0;
            }
        }
    }
    return 0;
}

/** \brief The comparisson-routine for GRForceRodEntries.
*/
int GRForceRodEntry::comp(const GRForceRodEntry * e1, const GRForceRodEntry * e2)
{
    if (e1->force == 0.0 && e2->force>0)		return -1;
    if (e2->force == 0.0 && e1->force>0)		return 1;
    if (e1->force == 0.0 && e2->force == 0.0)	return 0;

    if (e1->rod->getIsSpaceRod() && !e2->rod->getIsSpaceRod())
        return 1;

    if (e2->rod->getIsSpaceRod() && !e1->rod->getIsSpaceRod())
        return -1;

    if (e1->rod->getIsSpaceRod())
    {
        if (e1->rod->getSpr1() < e2->rod->getSpr1()) return 1;
        if (e1->rod->getSpr1() == e2->rod->getSpr1()) return 0;

        return -1;
    }
    if (e1->force<e2->force) 		return -1;
    if (e1->force == e2->force)		return 0;
    return 1;
}

/** \brief FinishSyncSlice is called when a synchronization cycle is finished.

The routine clears the hashtable-entries and distributes the elements to the staves and
assigning REAL spring-ids.
(Afterwards, the rods can be created; no more ordering of springs is neccessary)
The routine works as follows:
Iterate through the hash-table (ordered by mSpringID).
For each entry, set the mSpringID and add the element to the staff. (This might
cause WARNINGS to be issued by the staff, as there might be two or more elements added with the
samed Spring-ID but conflicting meanings (as in clefs or keys)
*/
int GRStaffManager::FinishSyncSlice(const TYPE_TIMEPOSITION & tp)
{
    // first, we sort the tmphash for the springIDs...
    int startspringID = mSpringID;
    // syncHash is the Hash of Elements ...
    if (syncHash.GetCount() > 0)
    {
        syncHash.sortValues();
        HashEntry tmphash;
        GuidoPos pos = syncHash.GetHeadPosition();
        int conflict = 0;

        while (pos)
        {
            if (syncHash.GetNext(pos, tmphash))
            {
                // Now, we construct the Spring to put it in the SpaceForceFunction.
                GRSpring * spr = new GRSpring(tp, DURATION_0, settings.spring, settings.proportionalRenderingForceMultiplicator);
                spr->setID(mSpringID);
                // now we need to check whether there are elements in the SAME staff that would be added to the same spring.
                // If there are auto-Tags, then these are ignored. If not, only the first one is really used, the others are REMOVED!
                GuidoPos tmppos = tmphash.data->GetHeadPosition();
                while (tmppos)
                {

                    // we need to compare each of the entries to each other.
                    SubHash *sh = tmphash.data->GetNext(tmppos);
                    GRTag * tag = dynamic_cast<GRTag *>(sh->grel);

                    if (!tag)				continue;
                    if (tag->getError())	continue;

                    GuidoPos tmppos2 = tmppos;
                    while (tmppos2)
                    {
                        SubHash * sh2 = tmphash.data->GetNext(tmppos2);
                        if (sh->grstaff == sh2->grstaff)
                        {
                            GRStaffState &  staffstate = sh->grstaff->getGRStaffState();
                            GRTag * tag2 = dynamic_cast<GRTag *>(sh2->grel);
                            if (!tag2)	continue;

                            if (tag->getIsAuto() && !tag2->getIsAuto())
                            {
                                // check consistency between staff and clef baseline
                                GRClef * clef = dynamic_cast<GRClef*>(tag2);
                                if (clef && (staffstate.baseline != clef->getBaseLine()))
                                    sh->grstaff->setClefParameters(clef);
                                tag->setError(1);
                                if (*tag != *tag2)
                                    conflict = 1;
                                break;
                            }
                            else
                            {
                                if (tag2->getIsAuto() && !tag->getIsAuto())
                                {
                                    // check consistency between staff and clef baseline
                                    GRClef * clef = dynamic_cast<GRClef*>(tag);
                                    if (clef && (staffstate.baseline != clef->getBaseLine()))
                                        sh->grstaff->setClefParameters(clef);
                                    tag2->setError(1);
                                    if (*tag != *tag2)
                                        conflict = 1;
                                    continue;
                                }
                            }

                            // now, none of the two is auto-tag disable the second tag
                            GRClef * clef = dynamic_cast<GRClef*>(tag);
                            // check consistency between staff and clef baseline
                            if (clef && (staffstate.baseline != clef->getBaseLine()))
                                sh->grstaff->setClefParameters(clef);
                            tag2->setError(1);
                            if (*tag != *tag2)
                                conflict = 1;
                        }
                    }
                }

                tmppos = tmphash.data->GetHeadPosition();
                while (tmppos)
                {
                    SubHash *sh = tmphash.data->GetNext(tmppos);
                    // now, we check, wether adds the elements to the spring.
                    spr->addElement(sh->grel, sh->voice);
                    sh->grel->tellSpringID(mSpringID);
                }

                // this is done, so that the correct spring-constants are used (for example for Startglues ...)
                spr->recalcConstant();
                // this releases the memory ...
                delete tmphash.data;
                mSpringVector->Set(mSpringID, spr);
                // spr->checkLocalCollisions();
                mSpringID++;
            }
        }

        // now we check, wether we had a conflict
        if (conflict)
        {
            // then, we need to deal with the conflict, that is, the staves have to "reparse" the
            // last section and determine the staff-state with the correct elements.
            // In some cases, the vertical position of elements has to be recalculated 
            // (for example for \key).
            const int mini = mMyStaffs->GetMinimum();
            const int maxi = mMyStaffs->GetMaximum();
            for (int i = mini; i <= maxi; ++i)
            {
                GRStaff * staff = mMyStaffs->Get(i);
                if (staff)
                {
                    GuidoPos pos = staffposvect.Get(i);
                    staff->UpdateStaffState(pos);
                }
            }
        }
    }
    else // syncHash.GetCount() >0
    {
        // syncHash.GetCount() == 0
        GuidoPos pos = evlist.GetHeadPosition();
        if (pos)
        {
            GRSpring * spr = new GRSpring(tp, DURATION_0, settings.spring, settings.proportionalRenderingForceMultiplicator);
            spr->setID(mSpringID);
            TYPE_DURATION dur(Frac_Max);
            while (pos)
            {
                VoiceEvent * ve = evlist.GetNext(pos);
                GREvent * grev = ve->ev;
                ARMusicalEvent * arev = /*dynamic*/static_cast<ARMusicalEvent *>(grev->getAbstractRepresentation());
                if (arev->getDuration() < dur)
                    dur = arev->getDuration();
                spr->addElement(grev, ve->vce);
                grev->tellSpringID(mSpringID);
            }
            spr->change_dur(dur);
            mSpringVector->Set(mSpringID, spr);
            // spr->checkLocalCollisions();
            ++mSpringID;
        }
    }

    // now, we introduce the System- and Pagetags.
    if (systemHash.GetCount() > 0)
    {
        // depending on the type, we need to associate with different springs.
        // right now, we only have the tempo-tag that needs to be put, where the meter
        // tag is (or at the first following event, if there is no meter-tag)
        GRNotationElement * mytag;
        GuidoPos systempos = systemHash.GetHeadPosition();
        while (systempos)
        {
            systemHash.GetNext(systempos, mytag);
            // now we have the tag in mytag ...
            if (typeid(*mytag) == typeid(GRTempo))
            {
                // find the meter-spring ... else put it onto the last spring that  was added ....
                // (this is not optimum -> a possible solution: PreParse: if meter and tempo
                // are there handle like here, otherwise handle tempo-tag as a position tag ...
                bool found = false;
                for (int j = startspringID; j < mSpringID; j++)
                {
                    GRSpring * tmpspr = mSpringVector->Get(j);
                    const type_info & ti = typeid(GRMeter);
                    // typeid(GRTempo);
                    if (tmpspr->hasType(ti))
                    {
                        found = true;
                        tmpspr->addElement(mytag, nullptr);
                        break;
                    }
                }

                if (!found)
                {
                    mSpringVector->Get(mSpringID - 1)->addElement(mytag, nullptr);
                    // added to last spring ...
                }
                mGrSystemSlice->AddTail(mytag);
            }
            else
            {
                // ignored ...
                delete mytag;
            }
        }
        systemHash.DeleteAll();
    }

    if (pageHash.GetCount() > 0)
    {
    }

    syncHash.DeleteAll();
    voiceSpringArr.RemoveAll();
    evlist.RemoveAll();
    mTempSpringID = 1; // now reset the mTempSpringID

    // now, we remember the last position of the staffs ....
    const int theMin = mMyStaffs->GetMinimum();
    const int theMax = mMyStaffs->GetMaximum();

    for (int i = theMin; i <= theMax; ++i)
    {
        GRStaff * staff = mMyStaffs->Get(i);
        if (staff)
            staffposvect.Set(i, staff->mCompElements.GetTailPosition());
    }
    return 0;
}

// ----------------------------------------------------------------------------
float GRStaffManager::systemBreak(int newlineMode, float beginheight)
{
    beginheight = FindOptimumBreaks(newlineMode, beginheight);

    // we need to call the "break-algorithm" with the systemslice-list and  just force 
    // the correct break at the given position....
    // this means, we are dealing with a line or page-break at this point.

    // Right now I just delete the rods ...
    delete mSimpleRods;					// TODO: optimise
    mSimpleRods = new IRodList(1);
    delete mComplexRods;				// TODO: optimise
    mComplexRods = new IRodList(1);
    // the springs should be part of the whatever
    delete mSpringVector;

    // We need a new spring-vector .... we just start completely anew ....
    mSpringVector = new ISpringVector(1);
    // put one single (beginning) spring in the springvector, with id 0.
    GRSpring * spr = new GRSpring(TYPE_TIMEPOSITION(), DURATION_0, settings.spring, settings.proportionalRenderingForceMultiplicator);
    spr->setID(0);
    spr->change_const(50);
    mSpringVector->Set(0, spr);

    mTempSpringID = 1;
    mSpringID = 1;
    mLastSpringID = 0;
    return beginheight;
}

#ifndef GRStaffManager_H
#define GRStaffManager_H

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

#include "kf_ilist.h"	// required (should be replaced by STL containers)
#include "kf_vect.h"	// idem
#include "kr_hash.h"	// idem

template <class T> class KF_IVector;

#include "defines.h"	// For TYPE_TIMEPOSITION

#include "GRBeginSpaceForceFunction2.h"
#include "GRSpring.h"
#include "GRRod.h"

class ARMusicalTag;
class ARMusic;
class ARMusicalVoice;
class ARPageFormat;
class ARBarFormat;
class ARSystemFormat;
class ARAuto;
class ARAccolade;

class GRClef;
class GREvent;
class GRKey;
class GRPage;
class GRSystem;
class GRSystemSlice;
class GRMusic;
class GRPossibleBreakState;
class GRVoice;
class GRStaff;
class GRStaffState;
class GRVoiceManager;
class GRGlobalStem;

typedef KF_IVector<GRStaff> VStaff;
typedef KF_IVector<GRVoiceManager> VoiceManagerList;
typedef KF_Vector<int> VoiceSpringIDArray;

/** \brief This class combines the elements that are saved within a hash-entry for a 
	single graphical element.
*/

class SubHash
{
public:
    SubHash() : grel(nullptr), grstaff(nullptr), voice(nullptr), voiceID(-1) { }

	GRNotationElement * grel;
	GRStaff * grstaff;
	GRVoice * voice;
	int voiceID;
};

typedef KF_IPointerList<SubHash> SubHashList;

class HashEntry
{
public:
    HashEntry(int tmp) : data(nullptr)
	{
		assert(false);
		mSpringID = tmp;
	}

    HashEntry() : data(nullptr)
	{
		mSpringID = -1;
	}

	SubHashList * data;
	int mSpringID;
	
	int operator>(const HashEntry &h) const
	{
        return mSpringID > h.mSpringID ? 1 : 0;
	}
};

typedef KR_HashTable<NVstring,HashEntry> SyncHash;
typedef KR_HashTable<NVstring,GRNotationElement *> SystemHash;
typedef SystemHash PageHash;

class VoiceEvent
{
public:
	GREvent *ev;
	GRVoice *vce;
};

typedef KF_IPointerList<GRRod> IRodList;
typedef KF_IVector<GRSpring> ISpringVector;
// typedef KF_IPointerList<GREvent> GREventList;
typedef KF_IPointerList<GRPossibleBreakState> GRPBList;
typedef KF_Vector<GuidoPos> PositionVector;
typedef KF_IVector<GRStaffState> IStaffStateVector;
typedef KF_IPointerList<VoiceEvent> vce_ev_list;
typedef KF_IPointerList<GRSystemSlice> sysslicelist;

typedef KF_IPointerList<GRBeginSpaceForceFunction2> bsfflist;
typedef KF_IPointerList<GRSpring> sprlist;
typedef KF_Vector<float> FloatVector;

class GRForceRodEntry
{
	// this class represents the entries in a list for determining, which springs
	// are stretched by InitialSpringStretch.
	// The Entries are later sorted by space-rods (and start-springIDs)
	// and further by force ...
	public:
		GRForceRodEntry() : force(0.0), rod(nullptr) { }
	
		// the routine to compare the  entries against each other ..
		// returns:
		// -1: e1<e2
		//  0: e1 == e2
		//  1: e1 > e2
		static int comp( const GRForceRodEntry * e1, const GRForceRodEntry * e2 );
		float force;
		GRRod * rod;
};


/** \brief  This class manages the staffs of a system. 

	The class fills a staff with
	objects/events (it converts the abstract representation into the graphical representation).
	The GRStaffManager will be called from the GRSystem.
	That means GRSystem gives the start and end time of the system and abstract representation and
	the GRStaffManager retrieves the objects and put them into the staffs. At the end the staffs 
	will be added to the system.
*/

class GRStaffManager
{
		friend class GRSystem;
		friend class GRPossibleBreakState;

	public:

        GRStaffManager(GRMusic * p_grmusic, const ARPageFormat * inPageFormat = nullptr, const GuidoLayoutSettings * settings = nullptr);
        virtual ~GRStaffManager();

        //-> called by GRMusic -> main entry point
        void createStaves();

        //-> called by GRSystemSlice
        GRMusic * getGRMusic() { return mGrMusic; }

        // While GRVoiceManager iterates thru voices and if the newSystem tag is set and system spacing
        // is specified than GRVoiceManager notify GRStaffManager about system distance. Information about
        // system distance is required for finding the optimal breaks.
        //-> called by GRVoiceManager::Iterate()
		void setSystemDistance(float distance, const GRVoiceManager & p_vcmgr);
        // While GRVoiceManager is first initialized or after new system or page break than GRVoiceManager reads
        // all tags that have to be read at the beginning - pageFormat, systemFormat, staff etc. It informs 
        // GRStaffManager which just bypasses this information to GRPage.
		void setPageFormat(const ARPageFormat * pform);
        // System format is needed to take care of the very first line breaking position since it can be different if 
        // user wants like that and it is also needed in creating new GRSystem while calculating optimum breaks.
        //-> called by GRVoiceManager::ReadBeginTags()
		void setSystemFormat(const ARSystemFormat * sysfrm);

        //! [02.08.2017]
        // This function is supposed to be called by GRPage::systemFinished
        // which would be called by GRSystem::FinishSystem but this is not the case
        // anymore and this function is not used at all. GRStaffManager is fully responsible
        // for adding pages into GRMusic after finding optimum page breaks. 
		////void NewPage(GRPage *newpage);

        // Called by the GRVoiceManager to add those tags that are placed directly on
        // the page (like e.g. \\title, \\composer etc.)
        //-> called by GRVoiceManager::Iterate()
        int AddPageTag(GRNotationElement * grel, GRStaff * grstaff, int voiceid);

        // GRVoiceManager::parseTag() creates GR from given tag and than calls this function to set the 
        // state of the staff (clef, meter, key) - no GR is created here.
        //-> called by GRVoiceManager::parseTag
        virtual bool	setStaffStateTag(ARMusicalTag * tag, int staffnum);
        virtual GRStaff *	getStaff(int staff);
        virtual void		prepareStaff(int staff);

        // Function for global shared stem for chords between different voices.
        GRGlobalStem *		getOtherGlobalStem(GRSystemSlice * psys, GRVoiceManager * curvcmgr, const NVstring & label);

        // Connects two or more lines of music that are played simultaneously in piano, keyboard, harp, or some 
        // pitched percussion music. Accolade is old name for that brace.
        //-> called by GRVoiceManager::ReadBeginTags
        void	notifyAccoladeTag(ARAccolade * inAccoladeTag);

        //! SPRING-ROD SPACING ALGORITHM
        // This routine is used to get the current beginning_sff.
        // This is used by the constructor of GRSystemSlice to save a pointer to the value, which is then used later
        // to calculate the actual optimum line breaks...
        // -> called by GRSystemSlice::GRSystemSlice function (read-only)
        const GRBeginSpaceForceFunction2 * getCurrentBegSFF() const;
        
        // GRStaffManager after finding optimum breaks creates beginning system slice which start new staff
        // and adds elements to spring also after creating new GRSystem one need to end current staff and call
        // this function again.
        //-> called by GRStaff::BeginStaff & GRStaff::EndStaff
		void addElementToSpring(GRNotationElement *grne, int springid);
		static float InitialSpringStretch(int start, int end, IRodList * simplerods, IRodList * complerods, ISpringVector *sprvect, std::ofstream *springlog = 0);
        
        // After GRStaffManager finds optimum breaks or GRVoiceManager does user manual tag break this 
        // function is called to support spring-rod spacing algorithm.
        //-> called by GRVoiceManager and GRStaff
        int AddGRSyncElement(GRNotationElement *grel, GRStaff *grstaff, int voiceid, GRVoice *vce, GRNotationElement * sameel = 0);
        // Adding system tags to hash table for springs algorithm.
        //-> called by GRVoiceManager::Iterate()
        int AddSystemTag(GRNotationElement * grel, GRStaff * grstaff, int voiceid);
		int addRod(GRRod *rod, bool spaceactive, bool atHead = false, int voiceID = -1);

        // This routine takes care of breaking the position tags of a new system.
        // The routine first retrieves the last slice of the (new, to be created) system and then manages the still open tags of the
        // system. It saves the open tags in a list, which will then be used to resume the tags in the next line.
        //-> called by GRSystem::GRSystem
        void TakeCareOfBreakAt(GRSystem * newsys);

	protected:

        GuidoLayoutSettings settings;

#ifdef SPRINGLOG
		ofstream springlog;
#endif 
        // Takes care of resuming the open tags from the previous system.
        // Will be reused for proportional spacing algorithm as well.
		void ResumeOpenTags (const GRSystemSlice * lastslice, GRSystemSlice * beginslice);

		// remember the system-distance (if it is explicitly set ....)
		float mSystemDistancePrev;
		float mSystemDistance;

        VStaff *		   mMyStaffs;
        PositionVector	   staffposvect;
        VoiceManagerList * mVoiceMgrList;
        GRSystemSlice * mGrSystemSlice;
        GRSystem *		mGrSystem;
        GRMusic *		mGrMusic;
        GRPage *		mGrPage;
        
        ARMusic *		mArMusic;

        const ARSystemFormat * mCurSysFormat;

        sysslicelist *	 mSystemSlices;

        IStaffStateVector * mStaffStateVect;

        std::vector<ARAccolade *> mCurAccoladeTag;
	
        //! SPRING-ROD SPACING & LINE BREAKING ALGORITHM
        GRSpaceForceFunction2 * BuildSFF();
        // This routine takes the current system-slice list and finds the optimum break for it.
        // It then also creates the systems with the respective system slices.
        float FindOptimumBreaks(int pageorsystembreak, float beginheight = 0);
        void UpdateBeginningSFF(int staffnum);
		GRSystemSlice* CreateBeginSlice (const GRSystemSlice * lastslice);
		void createNewSystemRods(int startid,int endid);
        int	CheckForBarSpring(int sprid);

        // These are pointers to the maximum width clef and the maximum width key. These are saved so that
        // the beginning sff can be updated easily .....
        GRClef * mMaxClef;
        GRKey  * mMaxKey;

		// this variable holds in a list the sffs for the beginning of a line given the current state information.
		// Whenever a clef or key changes, the sff is updated and added to the list. These values can then be used to evaluate
		// the potential lines. 
		bsfflist * beg_sff_list;
		sprlist  * beg_spr_list;

		FloatVector staffTopVector;
		FloatVector staffBottomVector;

		// these rods are build during BuildSPF and then saved by GRPossibleBreakPosition
		// to be merged later through MergeSPFs 
		GRRod * lastrod;
		GRRod * firstrod;

		SystemHash	systemHash;		// a hash for system-tags
		PageHash	pageHash;		// a hash-table for page-tags
		SyncHash	syncHash;		// this hash has all the synchronization work ...
		VoiceSpringIDArray voiceSpringArr;
		int mTempSpringID;			// The temporary SpringID for each Slice (especially for Tags)

		// The real SpringID ...
		// This is incremented for the whole System that this staff manager handles ...
		int mSpringID;
		// this remembers the mLastSpringID for the BuildSPF function.
		int mLastSpringID;

		// The mSimpleRods are the ones, that cover just a single spring
		IRodList * mSimpleRods;

		// mComplexRods cover more than one spring (they need to be handled differently during stretching).
		IRodList * mComplexRods;

		// This vector holds the springs that are handled by this staffManager
		// They are later passed to the system(slice) ...
		ISpringVector * mSpringVector;

		// The SpaceForceFunction
		// This is build a new, when a call to BuildSPF is done.
		// It is then added to the break-state if needed ...
		// At the end, the new spf is added to the curspf.

		// The EventList for events that are added for synchronization
		vce_ev_list evlist;

		const ARAuto *	mArAuto;
	
	private:
		// the methods below are used by createStaves
		// they have been added to structure the structure the method
		enum	{ kNormalState, kNewSystem, kNewPage, kPBreak };

		typedef struct {
            // The current time position
			TYPE_TIMEPOSITION timePos;	
            // The smallest time position for the next event in any of the voices
			TYPE_TIMEPOSITION mintp;		
			// This position is maintained for forward switching.
			// It remembers the smallest time position where there is a need for a filltagmode = true.
            // This value is also important for potential Breakpoint determination.
			TYPE_TIMEPOSITION minswitchtp;
            // this determines whether the filltagmode should be maintained, so that all tags can be read in
			bool	continueWithFillTagMode;
            // this is set if at least one voice has a newSystem/newPage
			int		newline;	
            // potential Break
			float	pbreakval;
		} TCreateStavesState;

		bool nextTimePosition (int nvoices, bool filltagMode, TCreateStavesState& state);
		int	 initVoices();
        void applyStaffSize(GRStaff *staff, int staffNum);

        //! SPRING-ROD SPACING & LINE BREAKING ALGORITHM
        int FinishSyncSlice(const TYPE_TIMEPOSITION &tp);
        float systemBreak(int newlineMode, float beginheight);
};

#endif

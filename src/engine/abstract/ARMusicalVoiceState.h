#ifndef ARMusicalVoiceState_H
#define ARMusicalVoiceState_H

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

#include <typeinfo>

#include "TagList.h"		// is a typedef
#include "defines.h"		// for TYPE_TIMEPOSITION
#include "GUIDOTypes.h"		// for GuidoPos
/*
class ARMusic;
class ARVoiceManager;
class GRVoiceManager;
*/

class ARMusicalTag;
class ARPositionTag;
class ARMeter;
class ARDisplayDuration;
class ARBeamState;
class ARAuto;
class ARGrace;
class ARChordTag;

template <class T> class KF_IPointerList;
typedef KF_IPointerList<ARPositionTag> PositionTagList;

/** \brief This class manage the state of a voice. Here additional information for 
	voices will be stored (i.e..: Stemstate (Stemsup, down, auto).
 */
class ARMusicalVoiceState
{
	friend class ARMusic;
	friend class ARMusicalVoice;
	friend class ARVoiceManager;
	friend class GRVoiceManager;
	friend class GRStaffManager;

	public:
				ARMusicalVoiceState();
				ARMusicalVoiceState(const ARMusicalVoiceState &vst);
				ARMusicalVoiceState & operator=(const ARMusicalVoiceState & vst);

		virtual ~ARMusicalVoiceState();

        ARMusicalTag * getCurStateTag(const std::type_info & ti);
		ARMusicalTag * RemoveCurStateTag(const std::type_info & ti);

		void DeleteAddedAndRemovedPTags();
		
		const TagList * getCurTags() const					{ return curtags; }
		const TagList * getCurStateTags() const				{ return curstatetags; }
		const PositionTagList * getCurPositionTags() const	{ return curpositiontags; }

		void AddStateTag(ARMusicalTag * ntag);
		void AddTag(ARMusicalTag * ntag);
		void RemoveTag(ARMusicalTag * ntag);
		void AddPositionTag(ARPositionTag * ntag, int addtoaddedlist = 1);
		void RemovePositionTag(ARPositionTag * ntag, int addtoremovedlist = 1);

		void DeleteAll();
	
		const PositionTagList * addedPTags() const		{ return addedpositiontags; }
		const PositionTagList * removedPTags() const	{ return removedpositiontags; }
		const PositionTagList * currentPTags() const	{ return curpositiontags; }
	
		const ARMeter *	curMeter() const				{ return curmeter; }

	protected:

        // time position within the voice
        TYPE_TIMEPOSITION curTimePosition;
        // item position within the voice
        GuidoPos vpos;

		TYPE_TIMEPOSITION lastBarLineTimePosition;
		GuidoPos lastBarLinePosition;

		// current position within the ARMusicalVoice::mPosTagList which
        // is created during parsing process (AR creation)
		GuidoPos ptagpos;

        // list of tags that determine the state of the voice (clef, key, meter ...)
        // (tags that have no range)
		TagList * curstatetags;

        // helper state tags for autobeaming algorithm
        ARMeter * curmeter;
        ARBeamState * curbeamstate;

        // list of musical voice tags (ARText, ARFermata ...)
        // (tags that have no range)
		TagList * curtags;

		// current display duration (if available)
		ARDisplayDuration * fCurdispdur;

		ARAuto * curautostate;

		ARGrace * curgracetag;

		ARChordTag * curchordtag;

		ARMusicalVoiceState * chordState;
		ARMusicalVoiceState * prevchordState;

	private:
		// these describe those position tags that have been Added in the last
		// operation on the Voice with the given state.
		PositionTagList * addedpositiontags;

		// this list holds all the position tags that have been removed in the last
		// operation on the Voice with the state.
		PositionTagList * removedpositiontags;

		// this list holds all the position tags that are currently active.
		PositionTagList * curpositiontags;
};

#endif



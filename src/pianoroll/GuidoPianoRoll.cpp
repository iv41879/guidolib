/*
 GUIDO Library
 Copyright (C) 2013 Grame
 
 This Source Code Form is subject to the terms of the Mozilla Public
 License, v. 2.0. If a copy of the MPL was not distributed with this
 file, You can obtain one at http://mozilla.org/MPL/2.0/.
 
 Grame Research Laboratory, 11, cours de Verdun Gensoul 69002 Lyon - France
 research@grame.fr
 
 */

#include "ARMusic.h"
#include "VGDevice.h"
#include "ARRest.h"
#include "ARNote.h"
#include "ARChordComma.h"
#include "ARNoteFormat.h"
#include "TagParameterString.h"

#ifdef MIDIEXPORT
    #include "midifile.h"
#endif

#include "GuidoPianoRoll.h"

using namespace std;

#define kDefaultWidth     1024                    // the default canvas width
#define kDefaultHeight    400                     // the default canvas height
#define kDefaultLowPitch  0                       // the default canvas width
#define kDefaultHighPitch 127                     // the default canvas height
#define kDefaultStartDate TYPE_TIMEPOSITION(0, 1) // the default start date
#define kMinDist          4                       // the minimum distance between lines of the grid

//--------------------------------------------------------------------------
GuidoPianoRoll::GuidoPianoRoll() :
    fWidth(kDefaultWidth), fHeight(kDefaultHeight),
    fLowPitch(kDefaultLowPitch), fHighPitch(kDefaultHighPitch),
    fARMusic(NULL), fDev(NULL),
    fIsEndDateSet(false)
{
    fNoteHeight = fHeight / pitchRange();

	if (!fNoteHeight)
        fNoteHeight = 1;
}

//--------------------------------------------------------------------------
GuidoPianoRoll::~GuidoPianoRoll() 
{
}

//--------------------------------------------------------------------------
void GuidoPianoRoll::setARMusic(ARMusic *arMusic) 
{
    fARMusic = arMusic;
}

//--------------------------------------------------------------------------
void GuidoPianoRoll::setMidiFile(const char *midiFileName)
{
    fMidiFileName = midiFileName;
}

//--------------------------------------------------------------------------
void GuidoPianoRoll::setCanvasDimensions(int width, int height)
{
    if (width == -1)
        fWidth = kDefaultWidth;
    else
        fWidth = width;

    if (height == -1)
        fHeight = kDefaultHeight;
    else
        fHeight = height;

    fNoteHeight = fHeight / pitchRange();

	if (!fNoteHeight)
        fNoteHeight = 1;
}

//--------------------------------------------------------------------------
void GuidoPianoRoll::setLimitDates(GuidoDate start, GuidoDate end)
{
    if (start.num == 0 && start.denom == 0)
        fStartDate = kDefaultStartDate;
    else
	    fStartDate = TYPE_TIMEPOSITION(start.num, start.denom);

    if (end.num == 0 && end.denom == 0)
        fIsEndDateSet = false;
    else {
        fIsEndDateSet = true;
	    fEndDate = TYPE_TIMEPOSITION(end.num, end.denom);
    }
}

//--------------------------------------------------------------------------
void GuidoPianoRoll::setPitchRange(int minPitch, int maxPitch)
{
    if (minPitch == -1)
        fLowPitch = kDefaultLowPitch;
    else
        fLowPitch = minPitch;

    if (maxPitch == -1)
        fHighPitch = kDefaultHighPitch;
    else
        fHighPitch = maxPitch;
}

//--------------------------------------------------------------------------
bool GuidoPianoRoll::ownsARMusic() {
    if (fARMusic)
        return true;
    else
        return false;
}

//--------------------------------------------------------------------------
bool GuidoPianoRoll::ownsMidi() {
    if (fMidiFileName)
        return true;
    else
        return false;
}

//--------------------------------------------------------------------------
void GuidoPianoRoll::getRenderingFromAR(VGDevice *dev)
{
    fDev = dev;

    fDev->NotifySize(fWidth, fHeight);
    fDev->BeginDraw();
    fDev->PushPenColor(VGColor(100, 100, 100));
    fDev->PushFillColor(VGColor(0, 0, 0));

    if (!fIsEndDateSet)
        fEndDate = fARMusic->getDuration();

    fDuration = double(fEndDate - fStartDate);

    DrawGrid();

    GuidoPos pos = fARMusic->GetHeadPosition();

    while(pos) {
        ARMusicalVoice *e = fARMusic->GetNext(pos);
        DrawVoice(e);
    }

    fDev->PopFillColor();
    fDev->PopPenColor();
    fDev->EndDraw();
}

//--------------------------------------------------------------------------
void GuidoPianoRoll::getRenderingFromMidi(VGDevice *dev)
{
    fDev = dev;

    fDev->NotifySize(fWidth, fHeight);
    fDev->BeginDraw();
    fDev->PushPenColor(VGColor(100, 100, 100));
    fDev->PushFillColor(VGColor(0, 0, 0));

    DrawFromMidi();

    fDev->PopFillColor();
    fDev->PopPenColor();
    fDev->EndDraw();
}

//--------------------------------------------------------------------------
void GuidoPianoRoll::DrawGrid() const
{
	fDev->PushPenWidth(0.3f);

	for (int i = fLowPitch; i < fHighPitch; i++) {
		int y = pitch2ypos(i);
		int step = i % 12;		// the note in chromatic step

		if (fNoteHeight < kMinDist) {
			switch (step) {
				case 0 :			// C notes are highlighted
					fDev->PushPenWidth((i == 60) ? 1.0f : 0.6f);
					fDev->Line(0, (float) y, (float) fWidth, (float) y);
					fDev->PopPenWidth();
					break;
				case 7:				// G
					fDev->Line(0, (float) y, (float) fWidth, (float) y);
					break;
			}
		}
		else {
			switch (step) {
				case 0 :			// C notes are highlighted
					fDev->PushPenWidth((i == 60) ? 1.0f : 0.6f);
					fDev->Line(0, (float) y, (float) fWidth, (float) y);
					fDev->PopPenWidth();
					break;
				case 2:				// D
				case 4:				// E
				case 5:				// F
				case 7:				// G
				case 9:				// A
				case 11:			// B
					fDev->Line(0, (float) y, (float) fWidth, (float) y);
					break;
			}
		}
	}

	fDev->PopPenWidth();
}

//--------------------------------------------------------------------------
void GuidoPianoRoll::DrawVoice(ARMusicalVoice* v)
{
	fColored = false;
    fChord   = false;
	ObjectList *ol  = (ObjectList *)v;
	GuidoPos    pos = ol->GetHeadPosition();

	while(pos)
	{
		ARMusicalObject  *e    = ol->GetNext(pos);
		TYPE_DURATION     dur  = e->getDuration();
		TYPE_TIMEPOSITION date = e->getRelativeTimePosition();

        if (fChord) {
            dur   = fChordDuration;
            date -= dur;
        }

		TYPE_TIMEPOSITION end = date + dur;

		if (date >= fStartDate) {
            if (date < fEndDate) {
                if (end > fEndDate)
                    dur = fEndDate - date;

                DrawMusicalObject(e, date, dur);
            }
		}
		else if (end > fStartDate) { // To make the note end appear
			date = fStartDate;	
			
            if (end > fEndDate)
                dur = fEndDate - date;
			else
                dur = end - date;
			
            DrawMusicalObject(e, date, dur);
		}
		if (dynamic_cast<ARRest *>(e))
			fChord = false;
		else if (dynamic_cast<ARChordComma *>(e))
			fChord = true;
		else if (dynamic_cast<ARNoteFormat *>(e))
            handleColor(dynamic_cast<ARNoteFormat *>(e));
	}

	if (fColored) {
		fDev->PopPenColor();
		fDev->PopFillColor();
	}
}

//--------------------------------------------------------------------------
void GuidoPianoRoll::DrawMusicalObject(ARMusicalObject *e, TYPE_TIMEPOSITION date, TYPE_DURATION dur)
{
	ARNote *note = dynamic_cast<ARNote *>(e);

	if (note) {
		int pitch = note->midiPitch();

		if (pitch < 0)
			fChordDuration = dur; // prepare for potential chord
        else if (pitch >= fLowPitch && pitch <= fHighPitch) {
			if (note->getName() != ARNoteName::empty)
				DrawNote(pitch, double(date), double(dur));

			fChord = false;
		}
	}
}

//--------------------------------------------------------------------------
void GuidoPianoRoll::DrawNote(int pitch, double date, double dur) const
{
	int x = date2xpos  (date);
	int y = pitch2ypos (pitch);
	DrawRect(x, y, dur);
}

//--------------------------------------------------------------------------
void GuidoPianoRoll::DrawRect(int x, int y, double dur) const
{
	int w        = duration2width(dur);
	int halfstep = stepheight() / 2;

	if (!halfstep)
        halfstep = 1;

	fDev->Rectangle((float) x, (float) (y - halfstep), (float) (x + (w ? w : 1)), (float) (y + halfstep));
}

//--------------------------------------------------------------------------
bool GuidoPianoRoll::handleColor(ARNoteFormat* noteFormat)
{
	if (noteFormat) {
		const TagParameterString *tps = noteFormat->getColor();
		unsigned char colref[4];
		
        if (tps && tps->getRGB(colref)) {
			fColor = VGColor(colref[0], colref[1], colref[2], colref[3]);
			fDev->PushFillColor(fColor);
			fDev->PushPenColor(fColor);
			fColored = true;
		}
		else if (fColored) {
			fColored = false;
			fDev->PopPenColor();
			fDev->PopFillColor();
		}

		return true;
	}

	return false;
}

//--------------------------------------------------------------------------
int	GuidoPianoRoll::pitch2ypos (int midipitch) const
{
	int p = midipitch - fLowPitch;

	return fHeight - int((fHeight * p) / pitchRange());
}

#ifdef MIDIEXPORT
//--------------------------------------------------------------------------
// MIDI Piano roll
//--------------------------------------------------------------------------
static MidiSeqPtr KeyOnOff2Note(MidiSeqPtr seq, MidiLight* midi)
{
	if (!seq)
        return 0;

	map<int, MidiEvPtr> pitchMap;
	MidiEvPtr ev = FirstEv(seq);
	MidiSeqPtr outseq = midi->NewSeq();

	while (ev) {
		switch (EvType(ev)) {
			case typeKeyOn:
				pitchMap[Pitch(ev)] = ev;
				break;
            case typeKeyOff: {
                    MidiEvPtr note = midi->CopyEv(pitchMap[Pitch(ev)]);
                    pitchMap[Pitch(ev)] = 0;

                    if (note) {
                        EvType(note) = typeNote;
                        Dur(note)    = Date(ev) - Date(note);
                        midi->AddSeq(outseq, note);
                    }
                    else
                        cerr << "KeyOnOff2Note: key off " << int(Pitch(ev)) << " without matching key on at date " << Date(ev) << endl;
                }

				break;
			default:
				midi->AddSeq(outseq, midi->CopyEv(ev));
		}

		ev = Link(ev);
	}

	midi->FreeSeq(seq);

	return outseq;
}

//--------------------------------------------------------------------------
void GuidoPianoRoll::DrawMidiSeq(MidiSeqPtr seq, int tpqn)
{
	MidiEvPtr ev = FirstEv(seq);
	int tpwn     = tpqn * 4;
	double start = double(fStartDate);
	double end   = double(fEndDate);

	while (ev) {
		if (EvType(ev) == typeNote) {
			double date = double(Date(ev)) / tpwn;
			double dur  = double(Dur(ev))  / tpwn * 1.25; // REM: I don't know why it works but it does !

			if (date >= start) {
                if (date < end) {
                    double remain = end - date;
                    DrawNote(Pitch(ev), date, (dur > remain ? remain : dur));
                }
			}
			else if ((date + dur) > start) {
				dur -= (start - date);
				double remain = end - start;
				DrawNote(Pitch(ev), start, (dur > remain ? remain : dur));
			}
		}

		ev = Link(ev);
	}
}

//--------------------------------------------------------------------------
void GuidoPianoRoll::DrawFromMidi()
{
	MIDIFile mf;

    mf.Open(fMidiFileName, MidiFileRead);

    int n    = mf.infos().ntrks; /* get the number of tracks */
    int tpqn = mf.infos().time;
    vector<MidiSeqPtr> vseq;
    long maxTime = 0;

    while (n--) {
        MidiSeqPtr seq = KeyOnOff2Note(mf.ReadTrack(), mf.midi()); /* read every track */

        if (seq) {
            vseq.push_back(seq);
            long t = Date(LastEv(seq));

            if (t > maxTime)
                maxTime = t;
        }
    }

    if (!fIsEndDateSet)
        fEndDate = TYPE_TIMEPOSITION (int(double(maxTime) / (tpqn * 4) * 256), 256);

    fDuration = double(fEndDate - fStartDate);

    for (int i = 0; i < vseq.size(); i++) {
        DrawMidiSeq(vseq[i], tpqn);
        mf.midi()->FreeSeq(vseq[i]);
    }

    mf.Close();
    DrawGrid();
}

#endif
#ifndef GRSystem_H
#define GRSystem_H

/*
  GUIDO Library
  Copyright (C) 2002  Holger Hoos, Juergen Kilian, Kai Renz
  Copyright (C) 2003, 2004 Grame

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.

  Grame Research Laboratory, 11, cours de Verdun Gensoul 69002 Lyon - France
  research@grame.fr

*/

template <class T> class KF_IVector;

#include "kf_ilist.h"	// required

#include "NEPointerList.h"
#include "ARBar.h"
#include "GRBar.h"
#include "GRRod.h"
#include "GRSystemSlice.h"
#include "GREvent.h"

class ARSystemFormat;
class ARMusic;
class ARAccolade;

class GRPage;
class GRStaff;
class GRSpring;
class GRAccolade;
class GRSpaceForceFunction2;

typedef KF_IVector<GRStaff> StaffVector;
typedef KF_IVector<GRSpring> ISpringVector;
typedef KF_IPointerList<GRRod> IRodList;
typedef KF_IPointerList<GRSystemSlice> SSliceList;

extern GRStaff * gCurStaff;

/** \brief Manages a number of staffs and has a given start and end timeposition.
	GRSystem is the grafical representation of a part of ARMusic.
 
 	GRSystem is derived from GREvent, because GRSystem is really a musical 
	event and directly represents a part of ARMusic.
*/
class GRSystem : public GREvent
{
public:
	using GRNotationElement::getStaffNumber;

	enum BARTYPE { SYSTEM, ACCOLADE, STAFF, MENSUR };
	
				 GRSystem(GRStaffManager * stfmgr, GRPage * inPage, const TYPE_TIMEPOSITION & relativeTimePositionOfSystem,
							SSliceList ** systemslices, int count, GRSystemSlice * beginslice, ISpringVector ** pvect,
							const ARSystemFormat * sysform, float force, float spring, float proportionnalRender,
							bool islastsystem = false);
	virtual 	~GRSystem();

	virtual	float		getSystemWidthCm();
	virtual GRPage *	getGRPage()                             { return mPage; }
	GRSystemSlice *     getFirstGRSystemSlice() const;
	float               getDistance() const                     { return mDistance; }
	int                 getStaffNumber(const GRStaff * staff ) const;
	int                 getNewLinePage() const                  { return mNewLinePage; }
	float               getCurPosX() const                      { return mCurPosX; }							
	ARMusic *           getARMusic() const;
	GRSpring *          getSpring( int id ) const;
	const GRSpring *    getGRSpring( int id ) const;
    const StaffVector * getStaves() const;
	const SSliceList *  getSlices() const						{ return &mSystemSlices; }

	void        setSpringParameter(float nconst);
	void        setSystemFormat( const ARSystemFormat * sysfrm );
	void        setDistance( float inDistance )					{ mDistanceSet = true; mDistance = inDistance; }
	void        setGRPage( GRPage * inNewPage )					{ mPage = inNewPage; }
	void        setMeterOfStaffs( int num, int denom );

	virtual void		OnDraw( VGDevice & hdc ) const;
	virtual void		GetMap( GuidoElementSelector sel, MapCollector& f, MapInfos& infos ) const;
	virtual void		GGSOutput() const;
	virtual void		setPosition( class NVPoint const & );
	virtual void		updateBoundingBox();
	virtual void		print(std::ostream& os) const;
	virtual void		accept (GRVisitor& visitor);

	void	addSystemTag( GRNotationElement * mytag );
	void 	addBar( GRBar * mybar, enum GRSystem::BARTYPE btype, GRStaff * inStaff );
	void 	addToSpring(GRNotationElement * el, int id );
	void	addStaff( GRStaff * newStaff, int num );

	bool 	IsDistanceSet() const								{ return mDistanceSet; }
	void	notifyAccoladeTag( ARAccolade * inAccoladeTag );

	void 	FinishSystem();
	void 	FinishSystem( ISpringVector * pvect, IRodList * prods1, IRodList * prods2,
						  GRSpaceForceFunction2 * psff, const TYPE_TIMEPOSITION & tp, int lastline = 0);
	void	checkCollisions (TCollisions& state, bool lyrics) const;
	float	getNotesDensity () const;
	const GRSystemSlice* lastSlice() const		{ return mSystemSlices.GetTail(); }

	TYPE_TIMEPOSITION mDebugSystemDate;
    static int sSystemID;

protected:
	void 		dospacing();
//	void		DrawAccolade(  VGDevice & hdc, const NVPoint & leftTop, const NVPoint & leftBottom, int id ) const;
	void 		DrawSystemSprings( VGDevice & hdc ) const;
	void 		DrawSystemForce( VGDevice & hdc ) const;	
	void		AddSystemSlice( GRSystemSlice * inSlice );
	void		InitForceFunction (GRStaffManager * staffmgr, SSliceList ** psystemslices, int count);
	void		AdjustForceFunction (GRSliceHeight &sliceheight, int &startspring, int &endspring, float optForce);
	GRStaff *	ComputeBoundingBox(GRSliceHeight &sliceheight);
	
	GRBar::TRanges	barRange2ypos (const ARBar::TRanges& r) const;
								
	float 	mSystemforce; 		// The force used to stretch the system
	float 	mMarginLeft;		 // Margin to the Left (=indent)
	float 	mDistance;
	bool 	mDistanceSet;
	bool 	mIsLastLine;		// was int

	// this is returned by GRStaffManager.
	ISpringVector * mSpringVector;
	
	
	// we have two rodlists, one simple one with simple rods (spanning exactly one spring)
	// and another one with complex rods (spanning more than one spring).
	// These both are returned by the StaffManager:
	IRodList * simplerods;
	IRodList * complexrods;	
	GRSpaceForceFunction2 * mSpaceForceFunc;

	SSliceList 		mSystemSlices;
	
	float 			mOldPosX;
	int 			mNewLinePage;
	float 			mCurPosX;
	NEPointerList 	mZeroSpaceList;
	NEPointerList 	mSpaceList;

	// GRAccoladeList	mAccolades; // TODO
	std::vector<GRAccolade *>	mAccolade;		// TEMP

private:
	const GRStaff*	getStaff (int index) const;
	void	checkCollisions (TCollisions& state, std::vector<const GRNotationElement*>& elts) const;
	float	checkCollision (const GRNotationElement* e1, const GRNotationElement* e2) const;
	void	DrawAccolade( VGDevice & hdc, const GRSystemSlice * slice, const GRStaff * staff) const;
	void	HandleStaffOnOff (GuidoPos pos, const GRSystemSlice * slice) const;
	void	patchTempoIssue ();

	GRPage*			mPage;
	StaffVector*	mStaffs;
	NVPoint mNextStaffPosition;
};

#endif



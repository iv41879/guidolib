#ifndef ARBembel_H
#define ARBembel_H

/*
  GUIDO Library
  Copyright (C) 2002  Holger Hoos, Juergen Kilian, Kai Renz
  Copyright (C) 2002-2017 Grame

  This Source Code Form is subject to the terms of the Mozilla Public
  License, v. 2.0. If a copy of the MPL was not distributed with this
  file, You can obtain one at http://mozilla.org/MPL/2.0/.

  Grame Research Laboratory, 11, cours de Verdun Gensoul 69002 Lyon - France
  research@grame.fr

*/

#include "ARMTParameter.h"

/** \brief not yet documented
*/
class ARBembel : public ARMTParameter
{
	public:
					 ARBembel();
		virtual 	~ARBembel() {}

		virtual const char*	getParamsStr() const	{ return kARBembelParams; };
		virtual const char*	getTagName() const		{ return "ARBembel"; };
		virtual std::string getGMNName() const		{ return "\\bembel"; };

		int getInverse() const;

	protected:
	//	int inverse;
};

#endif

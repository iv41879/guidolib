#ifndef ARComposer_H
#define ARComposer_H

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

#include "ARText.h"

/** \brief not yet documented
*/
class ARComposer : public ARText
{
	public:
				 ARComposer();
	  virtual	~ARComposer();

		const char* getName() const;
		const char* getPageFormat() const;


		virtual void print(std::ostream& os) const;
		virtual void setTagParameterList(TagParameterList & tpl);
		virtual std::ostream & operator<<(std::ostream & os) const;

	protected:
			static ListOfTPLs ltpls;

	private:
	  TagParameterString * name;
	  TagParameterString * pageformat;

};


#endif

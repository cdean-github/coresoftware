#ifndef __PHGenFit_Measurement__
#define __PHGenFit_Measurement__

#include "GenFit/AbsMeasurement.h"

namespace PHGenFit {

class Measurement
{
public:
	//!ctor
	Measurement() : _measurement(NULL) {};

	//!dtor
	~Measurement();

	//!
	genfit::AbsMeasurement* getMeasurement() {return _measurement;}

protected:
	genfit::AbsMeasurement* _measurement;

	};
} //End of PHGenFit namespace

#endif //__PHGenFit_Measurement__

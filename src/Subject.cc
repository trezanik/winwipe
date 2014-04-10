
/**
 * @file	Subject.cc
 * @author	James Warren
 * @copyright	James Warren, 2013-2014
 * @license	Zlib (see license.txt or http://opensource.org/licenses/Zlib)
 */


#include "build.h"		// always first

#include "Subject.h"		// prototypes
#include "Observer.h"



void
Subject::Attach(
	Observer& observer
)
{
	_observers.insert(&observer);
}



void
Subject::Detach(
	Observer& observer
)
{
	_observers.erase(&observer);
}



void
Subject::Notify(
	Operation* operation
) const
{
#if MSVC_BEFORE_VS11
	// compiler doesn't support C++11? Here you go:

	for ( std::set<Observer*>::const_iterator iter = _observers.begin();
		iter != _observers.end(); iter++ )
	{
		(*iter)->Update(operation);
	}
#else
	for ( Observer* observer : _observers )
		observer->Update(operation);
#endif
}

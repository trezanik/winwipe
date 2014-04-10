#ifndef SUBJECT_H_INCLUDED
#define SUBJECT_H_INCLUDED

/**
 * @file	Subject.h
 * @author	James Warren
 * @brief	The subject matter of the Observer design pattern
*/


// required inclusions
#include "build.h"
#include <set>

// forward declarations
class Observer;
class Operation;



/**
 *
 *
 * @class Subject
 */
class Subject
{
private:
	NO_CLASS_ASSIGNMENT(Subject);
	NO_CLASS_COPY(Subject);

	std::set<Observer*>	_observers;

protected:

	Subject()
	{
	}

	void
	Notify(
		Operation* operation
	) const;

public:
	virtual ~Subject()
	{
	}


	/**
	 * Adds the observer to the list of classes that receive notifications
	 * from this Subject.
	 *
	 * @param observer A class inheriting Observer
	 */
	void 
	Attach(
		Observer& observer
	);


	/**
	 * Removes the observer from the list of notified classes.
	 *
	 * @param observer A class inheriting Observer
	 */
	void 
	Detach(
		Observer& observer
	);
};



#endif	// SUBJECT_H_INCLUDED

#ifndef OBSERVER_H_INCLUDED
#define OBSERVER_H_INCLUDED

/**
 * @file	Observer.h
 * @author	James Warren
 * @brief	Design pattern base class
 */


// required inclusions
#include "build.h"

// forward declarations
class Operation;



/**
 * 
 *
 * @class Observer
 */
class Observer
{
private:
	//NO_CLASS_ASSIGNMENT(Observer);	// abstract, can't assign
	NO_CLASS_COPY(Observer);

protected:
	Observer()
	{
	}

public:
	virtual ~Observer()
	{
	}


	virtual void
	Update(
		Operation* operation
	) = 0;
};



#endif	// OBSERVER_H_INCLUDED

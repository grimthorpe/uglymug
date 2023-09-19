#ifndef _EXPRESSION_H
#define _EXPRESSION_H

#include "copyright.h"
#include "db.h"
/*
 * Expression stuff
 *
 * This is the post-parsed expression tree structure(s) for Adrian's new
 * language. The idea is simple; since each command will have to be parsed
 * at some point, why not do that when it is first used (or even as part
 * of the game startup) and then keep the expression trees in memory instead
 * of inventing a new VM system.
 *
 * The down side is that the code has to be parsed each time the game comes
 * up (shouldn't be all that often), and it probably uses more storage space.
 */

#include "mudstring.h"

class ExpressionContext;

class Expression
{
public:
	enum State
	{
		Abort,
		Continue,
		Return,
		End
	};

	virtual ~Expression();

	State	Eval(ExpressionContext* pContext);

	// Evaluate the expression and return it as a string
	virtual String	Value() = 0;
	virtual bool	Success() = 0;

	// Evaluate the expression and return a bool
	virtual bool	BoolValue()	{ return Value(); }
	// Evaluate the expression and return an int
	virtual int	IntValue()	{ return atoi(Value().c_str()); }
	// Evaluate the expression and return a dbref
	virtual dbref	ObjectValue()	{ return NOTHING; }

	virtual bool	IsConst() const	{ return false; }
};

#endif

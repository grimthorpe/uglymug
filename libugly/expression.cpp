#include "copyright.h"
#include "expression.h"

class ConstExpression : public Expression
{
	virtual bool	IsConst() const	{ return true; }
};

class Int : public ConstExpression
{
private:
	int	m_int;
public:
	Int(int val) : m_int(val)
	{
	}

	State Eval(ExpressionContext*)
	{
		return End;
	}

	String Value()
	{
		char tmp[20];
		sprintf(tmp, "%i", IntValue());
		return tmp;
	}
	bool Success()
	{
		return IntValue() != 0;
	}
	int IntValue()
	{
		return m_int;
	}
};

class UnaryExpression : public Expression
{
	Expression*	m_pArg;
protected:
	UnaryExpression(Expression* pArg) : m_pArg(pArg)
	{
	}
	~UnaryExpression()
	{
		delete m_pArg;
	}
public:
	virtual bool	IsConst() const	{ return m_pArg->IsConst(); }
};

class BinaryExpression : public Expression
{
	Expression*	m_pLeft;
	Expression*	m_pRight;
protected:
	BinaryExpression(Expression* pLeft, Expression* pRight)
	{
		m_pLeft = pLeft;
		m_pRight = pRight;
	}
	~BinaryExpression()
	{
		delete m_pLeft;
		delete m_pRight;
	}
public:
	virtual bool	IsConst() const { return m_pLeft->IsConst() && m_pRight->IsConst(); }
};

class StatementBlock : public Expression
{
	std::list<Expression*>	m_expressions;

public:
	StatementBlock();
	~StatementBlock();

};

class IfStatement : public UnaryExpression
{
};

class ReturnStatement : public UnaryExpression
{
};


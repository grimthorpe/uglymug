/*
 * stack.h: Dead simple template for Stacks, more-or-less pinched
 *	from Stroustrup.
 *
 *	Peter Crowther, 11/1/94.
 */

#ifndef	_STACK_H
#define	_STACK_H

#include "db.h"
#pragma interface

extern void panic(const char *);

template <class T>
class	Array_stack
{
    private:
			int	size;
			int	where;
			T	*ary;
    public:
			Array_stack	(int s)		{ ary = new T [size = s]; where = 0; }
			~Array_stack	()		{ delete [] ary; }
		bool	push		(T elem)	{ if (where >= size) return false; ary [where++] = elem; return true; }
		T	pop		()		{ return ary [--where]; }
		void	drop		()		{ --where; }
	const	bool	is_empty	()	const	{ return where == 0; }
	const	int	depth		()	const	{ return where; }
	const	T	&top		()	const	{ if (where <= 0) panic ("Top with no elements"); return ary [where - 1]; }
	const	T	&operator[]	(int i)	const	{ if (i >= where) return *(T*)0; return ary [i]; }
};

template <class T> class Link_stack;
template <class T> class Link_iterator;

template <class T>
class	Link_element
{
		friend class Link_stack <T>;
		friend class Link_iterator <T>;
		T			object;
		Link_element <T>	*prev;
		Link_element <T>	*next;
					Link_element	(const T& obj)	: object (obj), prev (0), next (0)	{}
		void			link_between	(Link_element <T> *p, Link_element <T> *n)	{ prev = p; next = n; if (prev) prev->next = this; if (next) next->prev = this; }
		Link_element <T>	*unlink		()	{ Link_element <T> *temp = next; if (prev) prev->next = next; if (next) next->prev = prev; prev = 0; next = 0; return temp; }
};


template <class T>
class	Link_iterator
{
    private:
		const	Link_stack <T>	*stack;
			Link_element<T>	*current_elem;
    public:
					Link_iterator	(const Link_stack <T> &s)	: stack (&s), current_elem (s.list)	{}
		void			reset		()		{ current_elem = stack->list; }
		void			step		()		{ if (current_elem) current_elem = current_elem->next; }
		T			&current	()	const	{ return current_elem ? current_elem->object : *(T*)0; }
		char			finished	()	const	{ return !current_elem; }
		T			&next		()		{ step (); return current (); }
};


template <class T>
class	Link_stack
{
    friend class Link_iterator <T>;
    private:
		int			depth;
		Link_element <T>	*list;
    public:
					Link_stack	()		{ list = 0; depth = 0; }
					~Link_stack	()		{ while (list) pop (); }
	const	bool			is_empty	()	const	{ return !list; }
		void			push		(const T &what)	{ Link_element <T> *temp = new Link_element <T> (what); temp->link_between (0, list); list = temp; depth++; }
		T			pop		()		{ if (is_empty ()) panic ("Popped off end of Link_stack"); T value (list->object); Link_element <T> *temp = list; list = list->unlink (); delete temp; depth--; return value; }
		void			drop		()		{ if (is_empty ()) panic ("Dropped off end of Link_stack"); Link_element <T> *temp = list; list = list->unlink (); delete temp; depth--; }
		T			&top		()	const	{ return list ? list->object : *(T*)0; }
	const	int			get_depth	()	const	{ return depth; }
		T			remove		(const T &what)	{
										Link_element <T> *temp;
										for (temp = list; temp; temp = temp->next)
											if (temp->object == what)
											{
												if (temp == list)
													list = temp->next;
												temp->unlink ();
												T t2 (temp->object);
												delete temp;
												depth--;
												return t2;
											}
										panic ("Removing an item not on the list");
										return *(T*) 0;
									}
		Link_iterator <T>	iterator	()	const	{ return Link_iterator <T> (*this); }
};

#endif	/* _STACK_H */

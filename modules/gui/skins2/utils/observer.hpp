/*****************************************************************************
 * observer.hpp
 *****************************************************************************
 * Copyright (C) 2003 the VideoLAN team
 * $Id$
 *
 * Authors: Cyril Deguet     <asmax@via.ecp.fr>
 *          Olivier Teulière <ipkiss@via.ecp.fr>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111, USA.
 *****************************************************************************/

#ifndef OBSERVER_HPP
#define OBSERVER_HPP

#include <set>

// Forward declaration
template <class S, class ARG> class Observer;


/// Template for subjects in the Observer design pattern
template <class S, class ARG> class Subject
{
    public:
        virtual ~Subject() {}

        /// Remove all observers; should only be used for debugging purposes
        virtual void clearObservers()
        {
            m_observers.clear();
        }

        /// Add an observer to this subject
        /// Note: adding twice the same observer is not harmful
        virtual void addObserver( Observer<S, ARG>* pObserver )
        {
            m_observers.insert( pObserver );
        }

        /// Remove an observer from this subject
        /// Note: removing twice the same observer is not harmful
        virtual void delObserver( Observer<S, ARG>* pObserver )
        {
            m_observers.erase( pObserver );
        }

        /// Notify the observers when the status has changed
        virtual void notify( ARG arg )
        {
            // This stupid gcc 3.2 needs "typename"
            typename set<Observer<S, ARG>*>::const_iterator iter;
            for( iter = m_observers.begin(); iter != m_observers.end();
                 iter++ )
            {
                if( *iter == NULL )
                {
                    fprintf( stderr, "iter NULL !\n" );
                    return;
                }
                (*iter)->onUpdate( *this , arg );
            }
        }

        /// Notify without any argument
        virtual void notify() { notify( NULL ); }

    protected:
        Subject() {}

    private:
        /// Set of observers for this subject
        set<Observer<S, ARG>*> m_observers;
};


/// Template for observers in the Observer design pattern
template <class S, class ARG> class Observer
{
    public:
        virtual ~Observer() {}

        /// Method called when the subject is modified
        virtual void onUpdate( Subject<S,ARG> &rSubject , ARG arg) = 0;

    protected:
        Observer() {}
};


#endif

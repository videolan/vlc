/*
 * MovingAverage.cpp
 *****************************************************************************
 * Copyright (C) 2015 - 2016 VideoLAN Authors
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation; either version 2.1 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this program; if not, write to the Free Software Foundation,
 * Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 *****************************************************************************/
#ifndef MOVINGAVERAGE_HPP
#define MOVINGAVERAGE_HPP

#include <list>
#include <algorithm>

namespace adaptive
{
    template <class T>
    class MovingAverageSum
    {
        public:
            MovingAverageSum<T>(T i): sum(0), prev(i) { }
            void operator()(T n) {
                sum += (n > prev) ? n - prev : prev - n;
                prev = n;
            }
            T sum;
        private:
            T prev;
    };

    template <class T>
    class MovingAverage
    {
        public:
            MovingAverage(unsigned = 10);
            T push(T);

        private:
            std::list<T> values;
            T previous;
            unsigned maxobs;
            T avg;
    };

    template <class T>
    MovingAverage<T>::MovingAverage(unsigned nbobs) : avg(0)
    {
        if(nbobs < 1)
            throw new std::exception();
        this->maxobs = nbobs;
        previous = 0;
    }

    template <class T>
    T MovingAverage<T>::push(T v)
    {
        if(values.size() >= maxobs)
        {
            previous = values.front();
            values.pop_front();
        }
        values.push_back(v);

        /* compute for deltamax */
        T omin = *std::min_element(values.begin(), values.end());
        T omax = *std::max_element(values.begin(), values.end());
        MovingAverageSum<T> diffsums = std::for_each(values.begin(), values.end(),
                                                     MovingAverageSum<T>(previous));
        /* Vertical Horizontal Filter / Moving Average
         *
         * stability during observation window alters the alpha parameter
         * and then defines how fast we adapt */
        const T deltamax = omax - omin;
        double alpha = (diffsums.sum) ? 0.33 * ((double)deltamax / diffsums.sum) : 0.5;
        avg = alpha * avg + (1.0 - alpha) * (*values.rbegin());
        return avg;
    }
}

#endif // MOVINGAVERAGE_HPP

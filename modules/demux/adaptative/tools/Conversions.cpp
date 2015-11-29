/*
 * Conversions.cpp
 *****************************************************************************
 * Copyright © 2015 - VideoLAN and VLC Authors
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
#include "Conversions.hpp"

#include <vlc_charset.h>
#include <sstream>

/*
  Decodes a duration as defined by ISO 8601
  http://en.wikipedia.org/wiki/ISO_8601#Durations
  @param str A null-terminated string to convert
  @return: The duration in seconds. -1 if an error occurred.

  Exemple input string: "PT0H9M56.46S"
 */
static time_t str_duration( const char *psz_duration )
{
    bool        timeDesignatorReached = false;
    time_t      res = 0;
    char*       end_ptr;

    if ( psz_duration == NULL )
        return -1;
    if ( ( *(psz_duration++) ) != 'P' )
        return -1;
    do
    {
        double number = us_strtod( psz_duration, &end_ptr );
        double      mul = 0;
        if ( psz_duration != end_ptr )
            psz_duration = end_ptr;
        switch( *psz_duration )
        {
            case 'M':
            {
                //M can mean month or minutes, if the 'T' flag has been reached.
                //We don't handle months though.
                if ( timeDesignatorReached == true )
                    mul = 60.0;
                break ;
            }
            case 'Y':
            case 'W':
                break ; //Don't handle this duration.
            case 'D':
                mul = 86400.0;
                break ;
            case 'T':
                timeDesignatorReached = true;
                break ;
            case 'H':
                mul = 3600.0;
                break ;
            case 'S':
                mul = 1.0;
                break ;
            default:
                break ;
        }
        res += (time_t)(mul * number);
        if ( *psz_duration )
            psz_duration++;
    } while ( *psz_duration );
    return res;
}

IsoTime::IsoTime(const std::string &str)
{
    time = str_duration(str.c_str());
}

IsoTime::operator time_t () const
{
    return time;
}

/* i_year: year - 1900  i_month: 0-11  i_mday: 1-31 i_hour: 0-23 i_minute: 0-59 i_second: 0-59 */
static int64_t vlc_timegm( int i_year, int i_month, int i_mday, int i_hour, int i_minute, int i_second )
{
    static const int pn_day[12+1] = { 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 304, 334 };
    int64_t i_day;

    if( i_year < 70 ||
        i_month < 0 || i_month > 11 || i_mday < 1 || i_mday > 31 ||
        i_hour < 0 || i_hour > 23 || i_minute < 0 || i_minute > 59 || i_second < 0 || i_second > 59 )
        return -1;

    /* Count the number of days */
    i_day = (int64_t)365 * (i_year-70) + pn_day[i_month] + i_mday - 1;
#define LEAP(y) ( ((y)%4) == 0 && (((y)%100) != 0 || ((y)%400) == 0) ? 1 : 0)
    for( int i = 70; i < i_year; i++ )
        i_day += LEAP(1900+i);
    if( i_month > 1 )
        i_day += LEAP(1900+i_year);
#undef LEAP
    /**/
    return ((24*i_day + i_hour)*60 + i_minute)*60 + i_second;
}

UTCTime::UTCTime(const std::string &str)
{
    enum { YEAR = 0, MON, DAY, HOUR, MIN, SEC, MSEC, TZ };
    int values[8] = {0};
    std::istringstream in(str);

    try
    {
        /* Date */
        for(int i=YEAR;i<=DAY && !in.eof();i++)
        {
            if(i!=YEAR)
                in.ignore(1);
            in >> values[i];
        }
        /* Time */
        if (!in.eof() && in.peek() == 'T')
        {
            for(int i=HOUR;i<=SEC && !in.eof();i++)
            {
                in.ignore(1);
                in >> values[i];
            }
        }
        if(!in.eof() && in.peek() == '.')
        {
            in.ignore(1);
            in >> values[MSEC];
        }
        /* Timezone */
        if(!in.eof() && in.peek() == 'Z')
        {
            in.ignore(1);
        }
        else if (!in.eof() && (in.peek() == '+' || in.peek() == '-'))
        {
            int i, tz = (in.peek() == '+') ? -60 : +60;
            in.ignore(1);
            if(!in.eof())
            {
                in >> i;
                tz *= i;
                in.ignore(1);
                if(!in.eof())
                {
                    in >> i;
                    tz += i;
                }
                values[TZ] = tz;
            }
        }

        t = vlc_timegm( values[YEAR] - 1900, values[MON] - 1, values[DAY],
                        values[HOUR], values[MIN], values[SEC] );
        t += values[TZ] * 60;
        t *= CLOCK_FREQ;
        t += values[MSEC] * 1000;
    } catch(int) {
        t = 0;
    }
}

time_t UTCTime::time() const
{
    return t / CLOCK_FREQ;
}

mtime_t UTCTime::mtime() const
{
    return t;
}

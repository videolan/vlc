#!/usr/bin/perl
# Copyright © 2011 Rafaël Carré <funman at videolanorg>
#
#  This program is free software; you can redistribute it and/or modify
#  it under the terms of the GNU General Public License as published by
#  the Free Software Foundation; either version 2 of the License, or
#  (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#  GNU General Public License for more details.
#
#  You should have received a copy of the GNU General Public License
#  along with this program; if not, write to the Free Software
#  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
#

use warnings;

if ($#ARGV != 0) {
    die "Need exactly one argument";
}

open F, "+<$ARGV[0]"
    or die "Can't open `$ARGV[0]'";
binmode F;

seek F, 0x3c, 0;
my $offset = get_le(4);
seek F, $offset, 0;

if (get_le(4) != 0x00004550) { # IMAGE_NT_SIGNATURE
    die "Not a NT executable";
}

seek F, 20 + 70, 1;

my $flags = get_le(2);
seek F, -2, 1;

$flags |= 0x40;   # Dynamic Base
$flags |= 0x100;  # NX Compat
$flags |= 0x400;  # NO SEH
#$flags |= 0x1000; # App Container

printf F "%c%c", $flags & 0xff,($flags >> 8) & 0xff;

close F;

sub get_le {
    my $bytes;
    read F, $bytes, $_[0];
    if (length $bytes ne $_[0]) {
        die "Couldn't read";
    }

    my $ret = 0;
    my @array = split //, $bytes;
    for (my $shift = 0, my $i = 0; $i < $_[0]; $i++, $shift += 8) {
        $ret += (ord $array[$i]) << $shift;
    }
    return $ret;
}

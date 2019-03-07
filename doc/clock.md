# Clock Architecture

## Introduction

This document presents the new clock archictecture for **VLC**, starting from
VLC 4.0.

The clock is the element that manages the synchronisation of all [ES][ES],
notably audio and video (+subtitles) synchronization.

While it can seem simple, at first glance, this part is not trivial, because
one must take care of numerous clocks in parallel, and they can be out-of-sync:
for example, your audio clock and your system clock are not necessary in-sync.
This is the same issue between your streamer's clock and your player's clock.
And most clocks drift.

[PCR]: #f  "Program Clock Reference"
[PTS]: #f  "Presentation TimeStamp"
[DTS]: #f  "Decoding TimeStamp"
[ES]: #f "Elementary streams, aka Tracks"

### Old clock system

The old clock from VLC was mainly an "*input clock*", based on the [input
PCR][PCR] (from the file), inherited from the time where VLC was mostly a
**MPEG-2/TS** player on the network. This is the correct clock for streaming,
and notably when your input format carries a [PCR][] or something similar.

This old clock is quite nice, but has quite a few shortcomings, notably it
requires resampling of your audio output, even for local files or simple audio
files.

It also rebases all the timings on the main input [PCR][], and it loses all the
original [PTS][] (because it was adding the current computer date). This is
notably an issue for transcoding (where it loses the original timestamps), for
pausing (we need to keep rewriting the timestamps) and for frame-accuracy
(because you know accurately only the input timings).

It also depends too much on having a valid input, which are very rare,
unfortunately. And it does not work well with very large delays.

Finally, the UI seekbar advances only when the [PCR][] is updated, which makes
big jumps in the seekbar, and is not smooth for the end-user. This notably
happens with large-audio samples and is related to the file format.

## New clock system

The idea of the new clock system is to have multiple pluggable clocks, one of
which being the master clock, that could be selected depending on the
situation.

For example, you could have an **audio master clock** *(local files)*, an
**input PCR master clock** *(streaming)*, a **video master clock** *(V-Sync)*
or a future **external clock** *(SDI, netsync...)*.  In the *audio master
clock* mode, VLC would not resample the audio anymore.

As previously, there is one clock per input-program.  This **main clock** is
therefore mostly at the es_out level and manages mostly the [PTS][] of all the
Elementary Streams.

### Different clocks: main, slave and master

Every output *(audio, video, stream)* has a clock, managed in the core. One of
those clock is master, the other are slaves.

The main clock is the part managing the selection of the clocks and it will
derivate the main timings from the system clock *(the monotonic clock)* and
will provide those timings to the rest of VLC, including outputs, modules and
interfaces.

It is currently an affine function based on the system clock, where the affine
coefficients are the moving average of the coefficient computed from the master
clock (In the future, it could be a different function).

The main clock holds a reference to all the output clocks, whether they are the
master or one of the slaves. Please refer to the ***src/clock/clock.c*** for
details about those structures

The master clock de facto defines the slope of the affine function.

The main clock will rebase the timestamps according to the master clock.  The
slaves ask the main clock, what is the system time corresponding to their
[PTS][].

If you want to see it differently, the master clock is a setter and the slave
clocks are the getters.

### Core outputs

The audio will be the master clock, in the nominal case.

## Delays

One important feature is the delaying/hastening of [elementary streams][ES]
with regards to other ES, also known as "Track Synchronization".

It's very hard to hasten ES, because most hardware decoders will not like that,
and because often your decoder is already fully loaded (taking a lot of CPU).

Instead, we delay all the other [ES][] that are not in advance, by (sort of)
**pausing** them. That means not displaying any new image for video outputs, or
playing silence for audio outputs.

However, if we are in the case where the master output is the one that is in
advance, pausing this output will break the main clock, and it will
artificially drift.

The main clock needs therefore to be reset when you find the synchro again, aka
when the output is "un-paused".

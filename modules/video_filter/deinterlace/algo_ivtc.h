/*****************************************************************************
 * algo_ivtc.h : IVTC (inverse telecine) algorithm for the VLC deinterlacer
 *****************************************************************************
 * Copyright (C) 2010-2011 VLC authors and VideoLAN
 *
 * Author: Juha Jeronen <juha.jeronen@jyu.fi>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation; either version 2.1 of the License, or
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

#ifndef VLC_DEINTERLACE_ALGO_IVTC_H
#define VLC_DEINTERLACE_ALGO_IVTC_H 1

/* Forward declarations */
struct filter_t;
struct picture_t;

/*****************************************************************************
 * Data structures
 *****************************************************************************/

#define IVTC_NUM_FIELD_PAIRS 7
#define IVTC_DETECTION_HISTORY_SIZE 3
#define IVTC_LATEST (IVTC_DETECTION_HISTORY_SIZE-1)
/**
 * Algorithm-specific state for IVTC.
 * @see RenderIVTC()
 */
typedef struct
{
    int i_mode; /**< Detecting, hard TC, or soft TC. @see ivtc_mode */
    int i_old_mode; /**< @see IVTCSoftTelecineDetect() */

    int i_cadence_pos; /**< Cadence counter, 0..4. Runs when locked on. */
    int i_tfd; /**< TFF or BFF telecine. Detected from the video. */

    /** Raw low-level detector output.
     *
     *  @see IVTCLowLevelDetect()
     */
    int pi_scores[IVTC_NUM_FIELD_PAIRS]; /**< Interlace scores. */
    int pi_motion[IVTC_DETECTION_HISTORY_SIZE]; /**< 8x8 blocks with motion. */
    int pi_top_rep[IVTC_DETECTION_HISTORY_SIZE]; /**< Hard top field repeat. */
    int pi_bot_rep[IVTC_DETECTION_HISTORY_SIZE]; /**< Hard bot field repeat. */

    /** Interlace scores of outgoing frames, used for judging IVTC output
     *  (detecting cadence breaks).
     *
     *  @see IVTCOutputOrDropFrame()
     */
    int pi_final_scores[IVTC_DETECTION_HISTORY_SIZE];

    /** Cadence position detection history (in ivtc_cadence_pos format).
     *  Contains the detected cadence position and a corresponding
     *  reliability flag for each algorithm.
     *
     *  s = scores, interlace scores based algorithm, original to this filter.
     *  v = vektor, hard field repeat based algorithm, inspired by
     *              the TVTime/Xine IVTC filter by Billy Biggs (Vektor).
     *
     *  Each algorithm may also keep internal, opaque data.
     *
     *  @see ivtc_cadence_pos
     *  @see IVTCCadenceDetectAlgoScores()
     *  @see IVTCCadenceDetectAlgoVektor()
     */
    int  pi_s_cadence_pos[IVTC_DETECTION_HISTORY_SIZE];
    bool pb_s_reliable[IVTC_DETECTION_HISTORY_SIZE];
    int  pi_v_raw[IVTC_DETECTION_HISTORY_SIZE]; /**< "vektor" algo internal */
    int  pi_v_cadence_pos[IVTC_DETECTION_HISTORY_SIZE];
    bool pb_v_reliable[IVTC_DETECTION_HISTORY_SIZE];

    /** Final result, chosen by IVTCCadenceDetectFinalize() from the results
     *  given by the different detection algorithms.
     *
     *  @see IVTCCadenceDetectFinalize()
     */
    int pi_cadence_pos_history[IVTC_DETECTION_HISTORY_SIZE];

    /**
     *  Set by cadence analyzer. Whether the sequence of last
     *  IVTC_DETECTION_HISTORY_SIZE detected positions, stored in
     *  pi_cadence_pos_history, looks like a valid telecine.
     *
     *  @see IVTCCadenceAnalyze()
     */
    bool b_sequence_valid;

    /**
     *  Set by cadence analyzer. True if detected position = "dea".
     *  The three entries of this are used for detecting three progressive
     *  stencil positions in a row, i.e. five progressive frames in a row;
     *  this triggers exit from hard IVTC.
     *
     *  @see IVTCCadenceAnalyze()
     */
    bool pb_all_progressives[IVTC_DETECTION_HISTORY_SIZE];
} ivtc_sys_t;

/*****************************************************************************
 * Functions
 *****************************************************************************/

/**
 * Deinterlace filter. Performs inverse telecine.
 *
 * Also known as "film mode" or "3:2 reverse pulldown" in some equipment.
 *
 * This filter attempts to reconstruct the original film frames from an
 * NTSC telecined signal. It is intended for 24fps progressive material
 * that was telecined to NTSC 60i. For example, most NTSC anime DVDs
 * are like this.
 *
 * There is no input frame parameter, because the input frames
 * are taken from the history buffer.
 *
 * See the file comment for a detailed description of the algorithm.
 *
 * @param p_filter The filter instance. Must be non-NULL.
 * @param[out] p_dst Output frame. Must be allocated by caller.
 * @return VLC error code (int).
 * @retval VLC_SUCCESS A film frame was reconstructed to p_dst.
 * @retval VLC_EGENERIC Frame dropped as part of normal IVTC operation.
 * @see Deinterlace()
 * @see ComposeFrame()
 * @see CalculateInterlaceScore()
 * @see EstimateNumBlocksWithMotion()
 */
int RenderIVTC( filter_t *p_filter, picture_t *p_dst, picture_t *p_pic );

/**
 * Clears the inverse telecine subsystem state.
 *
 * Used during initialization and uninitialization
 * (called from Open() and Flush()).
 *
 * @param p_filter The filter instance.
 * @see RenderIVTC()
 * @see Open()
 * @see Flush()
 */
void IVTCClearState( filter_t *p_filter );

/*****************************************************************************
 * Extra documentation
 *****************************************************************************/

/**
 * \file
 * IVTC (inverse telecine) algorithm for the VLC deinterlacer.
 * Also known as "film mode" or "3:2 reverse pulldown" in some equipment.
 *
 * Summary:
 *
 * This is a "live IVTC" filter, which attempts to do in realtime what
 * Transcode's ivtc->decimate->32detect chain does offline. Additionally,
 * it removes soft telecine. It is an original design, based on some ideas
 * from Transcode, some from TVTime/Xine, and some original.
 *
 * If the input material is pure NTSC telecined film, inverse telecine
 * will (ideally) exactly recover the original progressive film frames.
 * The output will run at 4/5 of the original framerate with no loss of
 * information. Interlacing artifacts are removed, and motion becomes
 * as smooth as it was on the original film. For soft-telecined material,
 * on the other hand, the progressive frames alredy exist, so only the
 * timings are changed such that the output becomes smooth 24fps (or would,
 * if the output device had an infinite framerate).
 *
 * Put in simple terms, this filter is targeted for NTSC movies and
 * especially anime. Virtually all 1990s and early 2000s anime is
 * hard-telecined. Because the source material is like that,
 * IVTC is needed for also virtually all official R1 (US) anime DVDs.
 *
 * Note that some anime from the turn of the century (e.g. Silent Mobius
 * and Sol Bianca) is a hybrid of telecined film and true interlaced
 * computer-generated effects and camera pans. In this case, applying IVTC
 * will effectively attempt to reconstruct the frames based on the film
 * component, but even if this is successful, the framerate reduction will
 * cause the computer-generated effects to stutter. This is mathematically
 * unavoidable. Instead of IVTC, a framerate doubling deinterlacer is
 * recommended for such material. Try "Phosphor", "Bob", or "Linear".
 *
 * Fortunately, 30fps true progressive anime is on the rise (e.g. ARIA,
 * Black Lagoon, Galaxy Angel, Ghost in the Shell: Solid State Society,
 * Mai Otome, Last Exile, and Rocket Girls). This type requires no
 * deinterlacer at all.
 *
 * Another recent trend is using 24fps computer-generated effects and
 * telecining them along with the cels (e.g. Kiddy Grade, Str.A.In. and
 * The Third: The Girl with the Blue Eye). For this group, IVTC is the
 * correct way to deinterlace, and works properly.
 *
 * Soft telecined anime, while rare, also exists. Stellvia of the Universe
 * and Angel Links are examples of this. Stellvia constantly alternates
 * between soft and hard telecine - pure CGI sequences are soft-telecined,
 * while sequences incorporating cel animation are hard-telecined.
 * This makes it very hard for the cadence detector to lock on,
 * and indeed Stellvia gives some trouble for the filter.
 *
 * To finish the list of different material types, Azumanga Daioh deserves
 * a special mention. The OP and ED sequences are both 30fps progressive,
 * while the episodes themselves are hard-telecined. This filter should
 * mostly work correctly with such material, too. (The beginning of the OP
 * shows some artifacts, but otherwise both the OP and ED are indeed
 * rendered progressive. The technical reason is that the filter has been
 * designed to aggressively reconstruct film frames, which helps in many
 * cases with hard-telecined material. In very rare cases, this approach may
 * go wrong, regardless of whether the input is telecined or progressive.)
 *
 * Finally, note also that IVTC is the only correct way to deinterlace NTSC
 * telecined material. Simply applying an interpolating deinterlacing filter
 * (with no framerate doubling) is harmful for two reasons. First, even if
 * the filter does not damage already progressive frames, it will lose half
 * of the available vertical resolution of those frames that are judged
 * interlaced. Some algorithms combining data from multiple frames may be
 * able to counter this to an extent, effectively performing something akin
 * to the frame reconstruction part of IVTC. A more serious problem is that
 * any motion will stutter, because (even in the ideal case) one out of
 * every four film frames will be shown twice, while the other three will
 * be shown only once. Duplicate removal and framerate reduction - which are
 * part of IVTC - are also needed to properly play back telecined material
 * on progressive displays at a non-doubled framerate.
 *
 * So, try this filter on your NTSC anime DVDs. It just might help.
 *
 *
 * Technical details:
 *
 *
 * First, NTSC hard telecine in a nutshell:
 *
 * Film is commonly captured at 24 fps. The framerate must be raised from
 * 24 fps to 59.94 fields per second, This starts by pretending that the
 * original framerate is 23.976 fps. When authoring, the audio can be
 * slowed down by 0.1% to match. Now 59.94 = 5/4 * (2*23.976), which gives
 * a nice ratio made out of small integers.
 *
 * Thus, each group of four film frames must become five frames in the NTSC
 * video stream. One cannot simply repeat one frame of every four, because
 * this would result in jerky motion. To slightly soften the jerkiness,
 * the extra frame is split into two extra fields, inserted at different
 * times. The content of the extra fields is (in classical telecine)
 * duplicated as-is from existing fields.
 *
 * The field duplication technique is called "3:2 pulldown". The pattern
 * is called the cadence. The output from 3:2 pulldown looks like this
 * (if the telecine is TFF, top field first):
 *
 * a  b  c  d  e     Telecined frame (actual frames stored on DVD)
 * T1 T1 T2 T3 T4    *T*op field content
 * B1 B2 B3 B3 B4    *B*ottom field content
 *
 * Numbers 1-4 denote the original film frames. E.g. T1 = top field of
 * original film frame 1. The field Tb, and one of either Bc or Bd, are
 * the extra fields inserted in the telecine. With exact duplication, it
 * of course doesn't matter whether Bc or Bd is the extra field, but
 * with "full field blended" material (see below) this will affect how to
 * correctly extract film frame 3.
 *
 * See the following web pages for illustrations and discussion:
 * http://neuron2.net/LVG/telecining1.html
 * http://arbor.ee.ntu.edu.tw/~jackeikuo/dvd2avi/ivtc/
 *
 * Note that film frame 2 has been stored "half and half" into two telecined
 * frames (b and c). Note also that telecine produces a sequence of
 * 3 progressive frames (d, e and a) followed by 2 interlaced frames
 * (b and c).
 *
 * The output may also look like this (BFF telecine, bottom field first):
 *
 * a' b' c' d' e'
 * T1 T2 T3 T3 T4
 * B1 B1 B2 B3 B4
 *
 * Now field Bb', and one of either Tc' or Td', are the extra fields.
 * Again, film frame 2 is stored "half and half" (into b' and c').
 *
 * Whether the pattern is like abcde or a'b'c'd'e', depends on the telecine
 * field dominance (TFF or BFF). This must match the video field dominance,
 * but is conceptually different. Importantly, there is no temporal
 * difference between those fields that came from the same film frame.
 * Also, see the section on soft telecine below.
 *
 * In a hard telecine, the TFD and VFD must match for field renderers
 * (e.g. traditional DVD player + CRT TV) to work correctly; this should be
 * fairly obvious by considering the above telecine patterns and how a
 * field renderer displays the material (one field at a time, dominant
 * field first).
 *
 * The VFD may, *correctly*, flip mid-stream, if soft field repeats
 * (repeat_pict) have been used. They are commonly used in soft telecine
 * (see below), but also occasional lone field repeats exist in some streams,
 * e.g., Sol Bianca.
 *
 * See e.g.
 * http://www.cambridgeimaging.co.uk/downloads/Telecine%20field%20dominance.pdf
 * for discussion. The document discusses mostly PAL, but includes some notes
 * on NTSC, too.
 *
 * The reason for the words "classical telecine" above, when field
 * duplication was first mentioned, is that there exists a
 * "full field blended" version, where the added fields are not exact
 * duplicates, but are blends of the original film frames. This is rare
 * in NTSC, but some material like this reportedly exists. See
 * http://www.animemusicvideos.org/guides/avtech/videogetb2a.html
 * In these cases, the additional fields are a (probably 50%) blend of the
 * frames between which they have been inserted. Which one of the two
 * possibilites is the extra field then becomes important.
 * This filter does NOT support "full field blended" material.
 *
 * To summarize, the 3:2 pulldown sequence produces a group of ten fields
 * out of every four film frames. Only eight of these fields are unique.
 * To remove the telecine, the duplicate fields must be removed, and the
 * original progressive frames restored. Additionally, the presentation
 * timestamps (PTS) must be adjusted, and one frame out of five (containing
 * no new information) dropped. The duration of each frame in the output
 * becomes 5/4 of that in the input, i.e. 25% longer.
 *
 * Theoretically, this whole mess could be avoided by soft telecining, if the
 * original material is pure 24fps progressive. By using the stream flags
 * correctly, the original progressive frames can be stored on the DVD.
 * In such cases, the DVD player will apply "soft" 3:2 pulldown. See the
 * following section.
 *
 * Also, the mess with cadence detection for hard telecine (see below) could
 * be avoided by using the progressive frame flag and a five-frame future
 * buffer, but no one ever sets the flag correctly for hard-telecined
 * streams. All frames are marked as interlaced, regardless of their cadence
 * position. This is evil, but sort-of-understandable, given that video
 * editors often come with "progressive" and "interlaced" editing modes,
 * but no separate "telecined" mode that could correctly handle this
 * information.
 *
 * In practice, most material with its origins in Asia (including virtually
 * all official US (R1) anime DVDs) is hard-telecined. Combined with the
 * turn-of-the-century practice of rendering true interlaced effects
 * on top of the hard-telecined stream, we have what can only be described
 * as a monstrosity. Fortunately, recent material is much more consistent,
 * even though still almost always hard-telecined.
 *
 * Finally, note that telecined video is often edited directly in interlaced
 * form, disregarding safe cut positions as pertains to the telecine sequence
 * (there are only two: between "d" and "e", or between "e" and the
 * next "a"). Thus, the telecine sequence will in practice jump erratically
 * at cuts [**]. An aggressive detection strategy is needed to cope with
 * this.
 *
 * [**] http://users.softlab.ece.ntua.gr/~ttsiod/ivtc.html
 *
 *
 * Note about chroma formats: 4:2:0 is very common at least on anime DVDs.
 * In the interlaced frames in a hard telecine, the chroma alternates
 * every chroma line, even if the chroma format is 4:2:0! This means that
 * if the interlaced picture is viewed as-is, the luma alternates every line,
 * while the chroma alternates only every two lines of the picture.
 *
 * That is, an interlaced frame in a 4:2:0 telecine looks like this
 * (numbers indicate which film frame the data comes from):
 *
 * luma  stored 4:2:0 chroma  displayed chroma
 * 1111  1111                 1111
 * 2222                       1111
 * 1111  2222                 2222
 * 2222                       2222
 * ...   ...                  ...
 *
 * The deinterlace filter sees the stored 4:2:0 chroma. The "displayed chroma"
 * is only generated later in the filter chain (probably when YUV is converted
 * to the display format, if the display does not accept YUV 4:2:0 directly).
 *
 *
 * Next, how NTSC soft telecine works:
 *
 * a  b  c  d     Frame index (actual frames stored on DVD)
 * T1 T2 T3 T4    *T*op field content
 * B1 B2 B3 B4    *B*ottom field content
 *
 * Here the progressive frames are stored as-is. The catch is in the stream
 * flags. For hard telecine, which was explained above, we have
 * VFD = constant and nb_fields = 2, just like in a true progressive or
 * true interlaced stream. Soft telecine, on the other hand, looks like this:
 *
 * a  b  c  d
 * 3  2  3  2     nb_fields
 * T  B  B  T     *Video* field dominance (for TFF telecine)
 * B  T  T  B     *Video* field dominance (for BFF telecine)
 *
 * Now the video field dominance flipflops every two frames!
 *
 * Note that nb_fields = 3 means the frame duration will be 1.5x that of a
 * normal frame. Often, soft-telecined frames are correctly flagged as
 * progressive.
 *
 * Here the telecining is expected to be done by the player, utilizing the
 * soft field repeat (repeat_pict) feature. This is indeed what a field
 * renderer (traditional interlaced equipment, or a framerate doubler)
 * should do with such a stream.
 *
 * In the IVTC filter, our job is to even out the frame durations, but
 * disregard video field dominance and just pass the progressive pictures
 * through as-is.
 *
 * Fortunately, for soft telecine to work at all, the stream flags must be
 * set correctly. Thus this type can be detected reliably by reading
 * nb_fields from three consecutive frames:
 *
 * Let P = previous, C = current, N = next. If the frame to be rendered is C,
 * there are only three relevant nb_fields flag patterns for the three-frame
 * stencil concerning soft telecine:
 *
 * P C N   What is happening:
 * 2 3 2   Entering soft telecine at frame C, or running inside it already.
 * 3 2 3   Running inside soft telecine.
 * 3 2 2   Exiting soft telecine at frame C. C is the last frame that should
 *         be handled as soft-telecined. (If we do timing adjustments to the
 *         "3"s only, we can already exit soft telecine mode when we see
 *         this pattern.)
 *
 * Note that the same stream may alternate between soft and hard telecine,
 * but these cannot occur at the same time. The start and end of the
 * soft-telecined parts can be read off the stream flags, and the rest of
 * the stream can be handed to the hard IVTC part of the filter for analysis.
 *
 * Finally, note also that a stream may also request a lone field repeat
 * (a sudden "3" surrounded by "2"s). Fortunately, these can be handled as
 * a two-frame soft telecine, as they match the first and third
 * flag patterns above.
 *
 * Combinations with several "3"s in a row are not valid for soft or hard
 * telecine, so if they occur, the frames can be passed through as-is.
 *
 *
 * Cadence detection for hard telecine:
 *
 * Consider viewing the TFF and BFF hard telecine sequences through a
 * three-frame stencil. Again, let P = previous, C = current, N = next.
 * A brief analysis leads to the following cadence tables.
 *
 * PCN                 = stencil position (Previous Current Next),
 * Dups.               = duplicate fields,
 * Best field pairs... = combinations of fields which correctly reproduce
 *                       the original progressive frames,
 * *                   = see timestamp considerations below for why
 *                       this particular arrangement.
 *
 * For TFF:
 *
 * PCN   Dups.     Best field pairs for progressive (correct, theoretical)
 * abc   TP = TC   TPBP = frame 1, TCBP = frame 1, TNBC = frame 2
 * bcd   BC = BN   TCBP = frame 2, TNBC = frame 3, TNBN = frame 3
 * cde   BP = BC   TCBP = frame 3, TCBC = frame 3, TNBN = frame 4
 * dea   none      TPBP = frame 3, TCBC = frame 4, TNBN = frame 1
 * eab   TC = TN   TPBP = frame 4, TCBC = frame 1, TNBC = frame 1
 *
 * (table cont'd)
 * PCN   Progressive output*
 * abc   frame 2 = TNBC (compose TN+BC)
 * bcd   frame 3 = TNBN (copy N)
 * cde   frame 4 = TNBN (copy N)
 * dea   (drop)
 * eab   frame 1 = TCBC (copy C), or TNBC (compose TN+BC)
 *
 * On the rows "dea" and "eab", frame 1 refers to a frame from the next
 * group of 4. "Compose TN+BC" means to construct a frame using the
 * top field of N, and the bottom field of C. See ComposeFrame().
 *
 * For BFF, swap all B and T, and rearrange the symbol pairs to again
 * read "TxBx". We have:
 *
 * PCN   Dups.     Best field pairs for progressive (correct, theoretical)
 * abc   BP = BC   TPBP = frame 1, TPBC = frame 1, TCBN = frame 2
 * bcd   TC = TN   TPBC = frame 2, TCBN = frame 3, TNBN = frame 3
 * cde   TP = TC   TPBC = frame 3, TCBC = frame 3, TNBN = frame 4
 * dea   none      TPBP = frame 3, TCBC = frame 4, TNBN = frame 1
 * eab   BC = BN   TPBP = frame 4, TCBC = frame 1, TCBN = frame 1
 *
 * (table cont'd)
 * PCN   Progressive output*
 * abc   frame 2 = TCBN (compose TC+BN)
 * bcd   frame 3 = TNBN (copy N)
 * cde   frame 4 = TNBN (copy N)
 * dea   (drop)
 * eab   frame 1 = TCBC (copy C), or TCBN (compose TC+BN)
 *
 * From these cadence tables we can extract two strategies for
 * cadence detection. We use both.
 *
 * Strategy 1: duplicated fields ("vektor").
 *
 * Consider that each stencil position has a unique duplicate field
 * condition. In one unique position, "dea", there is no match; in all
 * other positions, exactly one. By conservatively filtering the
 * possibilities based on detected hard field repeats (identical fields
 * in successive input frames), it is possible to gradually lock on
 * to the cadence. This kind of strategy is used by the classic IVTC filter
 * in TVTime/Xine by Billy Biggs (Vektor), hence the name.
 *
 * "Conservative" here means that we do not rule anything out, but start at
 * each stencil position by suggesting the position "dea", and then only add
 * to the list of possibilities based on field repeats that are detected at
 * the present stencil position. This estimate is then filtered by ANDing
 * against a shifted (time-advanced) version of the estimate from the
 * previous stencil position. Once the detected position becomes unique,
 * the filter locks on. If the new detection is inconsistent with the
 * previous one, the detector resets itself and starts from scratch.
 *
 * The strategy is very reliable, as it only requires running (fuzzy)
 * duplicate field detection against the input. It is very good at staying
 * locked on once it acquires the cadence, and it does so correctly very
 * often. These are indeed characteristics that can be observed in the
 * behaviour of the TVTime/Xine filter.
 *
 * Note especially that 8fps/12fps animation, common in anime, will cause
 * spurious hard-repeated fields. The conservative nature of the method
 * makes it very good at dealing with this - any spurious repeats will only
 * slow down the lock-on, not completely confuse it. It should also be good
 * at detecting the presence of a telecine, as neither true interlaced nor
 * true progressive material should contain any hard field repeats.
 * (This, however, has not been tested yet.)
 *
 * The disadvantages are that at times the method may lock on slowly,
 * because the detection must be filtered against the history until
 * a unique solution is found. Resets, if they happen, will also
 * slow down the lock-on.
 *
 * The hard duplicate detection required by this strategy can be made
 * data-adaptive in several ways. TVTime uses a running average of motion
 * scores for its history buffer. We utilize a different, original approach.
 * It is rare, if not nonexistent, that only one field changes between
 * two valid frames. Thus, if one field changes "much more" than the other
 * in fieldwise motion detection, the less changed one is probably a
 * duplicate. Importantly, this works with telecined input, too - the field
 * that changes "much" may be part of another film frame, while the "less"
 * changed one is actually a duplicate from the previous film frame.
 * If both fields change "about as much", then no hard field repeat
 * is detected.
 *
 *
 * Strategy 2: progressive/interlaced field combinations ("scores").
 *
 * We can also form a second strategy, which is not as reliable in practice,
 * but which locks on faster when it does. This is original to this filter.
 *
 * Consider all possible field pairs from two successive frames: TCBC, TCBN,
 * TNBC, TNBN. After one frame, these become TPBP, TPBC, TCBP, TCBC.
 * These eight pairs (seven unique, disregarding the duplicate TCBC)
 * are the exhaustive list of possible field pairs from two successive
 * frames in the three-frame PCN stencil.
 *
 * The above tables list triplets of field pair combinations for each cadence
 * position, which should produce progressive frames. All the given triplets
 * are unique in each table alone, although the one at "dea" is
 * indistinguishable from the case of pure progressive material. It is also
 * the only one which is not unique across both tables.
 *
 * Thus, all sequences of two neighboring triplets are unique across both
 * tables. (For "neighboring", each table is considered to wrap around from
 * "eab" back to "abc", i.e. from the last row back to the first row.)
 * Furthermore, each sequence of three neighboring triplets is redundantly
 * unique (i.e. is unique, and reduces the chance of false positives).
 * (In practice, though, we already know which table to consider, from the fact
 * that TFD and VFD must match. Checking only the relevant table makes the
 * strategy slightly more robust.)
 *
 * The important idea is: *all other* field pair combinations should produce
 * frames that look interlaced. This includes those combinations present in
 * the "wrong" (i.e. not current position) rows of the table (insofar as
 * those combinations are not also present in the "correct" row; by the
 * uniqueness property, *every* "wrong" row will always contain at least one
 * combination that differs from those in the "correct" row).
 *
 * We generate the artificial frames TCBC, TCBN, TNBC and TNBN (virtually;
 * no data is actually moved). Two of these are just the frames C and N,
 * which already exist; the two others correspond to composing the given
 * field pairs. We then compute the interlace score for each of these frames.
 * The interlace scores of what are now TPBP, TPBC and TCBP, also needed,
 * were computed by this same mechanism during the previous input frame.
 * These can be slided in history and reused.
 *
 * We then check, using the computed interlace scores, and taking into
 * account the video field dominance information, which field combination
 * triplet given in the appropriate table produces the smallest sum of
 * interlace scores. Unless we are at PCN = "dea" (which could also be pure
 * progressive!), this immediately gives us the most likely current cadence
 * position. Combined with a two-step history, the sequence of three most
 * likely positions found this way always allows us to make a more or less
 * reliable detection. (That is, when a reliable detection is possible; if the
 * video has no motion at all, every detection will report the position "dea".
 * In anime, still shots are common. Thus we must augment this with a
 * full-frame motion detection that switches the detector off if no motion
 * was detected.)
 *
 * The detection seems to need four full-frame interlace analyses per frame.
 * Actually, three are enough, because the previous N is the new C, so we can
 * slide the already computed result. Also during initialization, we only
 * need to compute TNBN on the first frame; this has become TPBP when the
 * third frame is reached. Similarly, we compute TNBN, TNBC and TCBN during
 * the second frame (just before the filter starts), and these get slided
 * into TCBC, TCBP and TPBC when the third frame is reached. At that point,
 * initialization is complete.
 *
 * Because we only compare interlace scores against each other, no threshold
 * is needed in the cadence detector. Thus it, trivially, adapts to the
 * material automatically.
 *
 * The weakness of this approach is that any comb metric detects incorrectly
 * every now and then. Especially slow vertical camera pans often get treated
 * wrong, because the messed-up field combination looks less interlaced
 * according to the comb metric (especially in anime) than the correct one
 * (which contains, correctly, one-pixel thick cartoon outlines, parts of
 * which often perfectly horizontal).
 *
 * The advantage is that this strategy catches horizontal camera pans
 * immediately and reliably, while the other strategy may still be trying
 * to lock on.
 *
 *
 * Frame reconstruction:
 *
 * We utilize a hybrid approach. If a valid cadence is locked on, we use the
 * operation table to decide what to do. This handles those cases correctly,
 * which would be difficult for the interlace detector alone (e.g. vertical
 * camera pans). Note that the operations that must be performed for IVTC
 * include timestamp mangling and frame dropping, which can only be done
 * reliably on a valid cadence.
 *
 * When the cadence fails (we detect this from a sudden upward jump in the
 * interlace scores of the constructed frames), we reset the "vektor"
 * detector strategy and fall back to an emergency frame composer, where we
 * use ideas from Transcode's IVTC.
 *
 * In this emergency mode, we simply output the least interlaced frame out of
 * the combinations TNBN, TNBC and TCBN (where only one of the last two is
 * tested, based on the stream TFF/BFF information). In this mode, we do not 
 * touch the timestamps, and just pass all five frames from each group right
 * through. This introduces some stutter, but in practice it is often not
 * noticeable. This is because the kind of material that is likely to trip up
 * the cadence detector usually includes irregular 8fps/12fps motion. With
 * true 24fps motion, the cadence quickly locks on, and stays locked on.
 *
 * Once the cadence locks on again, we resume normal operation based on
 * the operation table.
 *
 *
 * Timestamp mangling:
 *
 * To make five into four we need to extend frame durations by 25%.
 * Consider the following diagram (times given in 90kHz ticks, rounded to
 * integers; this is just for illustration, and for comparison with the
 * "scratch paper" comments in pulldown.c of TVTime/Xine):
 *
 * NTSC input (29.97 fps)
 * a       b       c       d        e        a (from next group) ...
 * 0    3003    6006    9009    12012    15015
 * 0      3754      7508       11261     15015
 * 1         2         3           4         1 (from next group) ...
 * Film output (23.976 fps)
 *
 * Three of the film frames have length 3754, and one has 3753
 * (it is 1/90000 sec shorter). This rounding was chosen so that the lengths
 * of the group of four sum to the original 15015.
 *
 * From the diagram we get these deltas for presentation timestamp adjustment
 * (in 90 kHz ticks, for illustration):
 * (1-a)   (2-b)  (3-c)   (4-d)   (skip)   (1-a) ...
 *     0   +751   +1502   +2252   (skip)       0 ...
 *
 * In fractions of (p_next->date - p_cur->date), regardless of actual
 * time unit, the deltas are:
 * (1-a)   (2-b)  (3-c)   (4-d)   (skip)   (1-a) ...
 *     0   +0.25  +0.50   +0.75   (skip)       0 ...
 *
 * This is what we actually use. In our implementation, the values are stored
 * multiplied by 4, as integers.
 *
 * The "current" frame should be displayed at [original time + delta].
 * E.g., when "current" = b (i.e. PCN = abc), start displaying film frame 2
 * at time [original time of b + 751 ticks]. So, when we catch the cadence,
 * we will start mangling the timestamps according to the cadence position
 * of the "current" frame, using the deltas given above. This will cause
 * a one-time jerk, most noticeable if the cadence happens to catch at
 * position "d". (Alternatively, upon lock-on, we could wait until we are
 * at "a" before switching on IVTC, but this makes the maximal delay
 * [max. detection + max. wait] = 3 + 4 = 7 input frames, which comes to
 * 7/30 ~ 0.23 seconds instead of the 3/30 = 0.10 seconds from purely
 * the detection. The one-time jerk is simpler to implement and gives the
 * faster lock-on.)
 *
 * It is clear that "e" is a safe choice for the dropped frame. This can be
 * seen from the timings and the cadence tables. First, consider the timings.
 * If we have only one future frame, "e" is the only one whose PTS, comparing
 * to the film frames, allows dropping it safely. To see this, consider which
 * film frame needs to be rendered as each new input frame arrives. Secondly,
 * consider the cadence tables. It is ok to drop "e", because the same
 * film frame "1" is available also at the next PCN position "eab".
 * (As a side note, it is interesting that Vektor's filter drops "b".
 * See the TVTime sources.)
 *
 * When the filter falls out of film mode, the timestamps of the incoming
 * frames are left untouched. Thus, the output from this filter has a
 * variable framerate: 4/5 of the input framerate when IVTC is active
 * (whether hard or soft), and the same framerate as input when it is not
 * (or when in emergency mode).
 *
 *
 * For other open-source IVTC codes, which may be a useful source for ideas,
 * see the following:
 *
 * The classic filter by Billy Biggs (Vektor). Written in 2001-2003 for
 * TVTime, and adapted into Xine later. In xine-lib 1.1.19, it is at
 * src/post/deinterlace/pulldown.*. Also needed are tvtime.*, and speedy.*.
 *
 * Transcode's ivtc->decimate->32detect chain by Thanassis Tsiodras.
 * Written in 2002, added in Transcode 0.6.12. This probably has something
 * to do with the same chain in MPlayer, considering that MPlayer acquired
 * an IVTC filter around the same time. In Transcode 1.1.5, the IVTC part is
 * at filter/filter_ivtc.c. Transcode 1.1.5 sources can be downloaded from
 * http://developer.berlios.de/project/showfiles.php?group_id=10094
 */

#endif

# LibVLC 3 to 4 Migration Guide

This guide documents the API-breaking changes introduced in LibVLC 4.0 and
helps applications written against LibVLC 3 port to LibVLC 4. It covers the
removed, renamed and reworked APIs, new functionality, and the behavioral
differences to be aware of. A migration checklist is provided at the end.

---

## 1. Biggest structural changes

Three changes pervade the whole API and are likely to touch every non-trivial
application:

1. **The `libvlc_event_manager_t` / `libvlc_event_attach()` event system is
   gone.** Every LibVLC object (media player, media discoverer, renderer
   discoverer, media list player, parser, media) now takes an explicit
   versioned callback struct at creation time.

2. **`libvlc_time_t` is now microseconds everywhere.** LibVLC 3 mixed
   milliseconds and microseconds in the public API. In 4.0 a single typed
   unit `libvlc_time_t` (`int64_t`) is used throughout the API for time units. 

3. **New `libvlc_parser_t` API:** Parsing and thumbnail generation is moved from
   `libvlc_media_t` to its own separate API. One parser object owns its own thread pool
   and can serve many concurrent parse and thumbnail requests. See §6 for more details.

---

## 2. Removed APIs

### 2.1 Global event system

Everything in `<vlc/libvlc_events.h>` is gone. The header has been deleted.

Removed symbols:

- `libvlc_event_manager_t`
- `libvlc_event_t` / `libvlc_event_type_t`
- `libvlc_callback_t`
- `libvlc_event_attach()`
- `libvlc_event_detach()`
- `libvlc_media_event_manager()`
- `libvlc_media_list_event_manager()`
- `libvlc_media_player_event_manager()` (and equivalents on every other
  object)
- All enum values (`libvlc_MediaPlayerPlaying`, `libvlc_MediaParsedChanged`,
  `libvlc_MediaListItemAdded`, `libvlc_MediaListEndReached`,
  `libvlc_MediaPlayerMediaChanged`, etc.)

**Migration:** use the object-specific callback struct (`*_cbs`) passed at
creation. See §4 for per-object mappings.

### 2.2 Media parsing / thumbnailing on `libvlc_media_t`

Removed from `<vlc/libvlc_media.h>`:

- `libvlc_media_parse_request()`
- `libvlc_media_parse_stop()`
- `libvlc_media_thumbnail_request_t`
- `libvlc_media_thumbnail_request_by_time()`
- `libvlc_media_thumbnail_request_by_pos()`
- `libvlc_media_thumbnail_request_destroy()`

The old `libvlc_media_parse_flag_t` enum values `libvlc_media_parse_local`,
`libvlc_media_parse_network` and `libvlc_media_parse_forced` are gone; the
new flag set is redefined at [`libvlc_parser.h`](../../include/vlc/libvlc_parser.h).

**Migration:** use the new `libvlc_parser_t` object in
`<vlc/libvlc_parser.h>` (§6). It handles both parsing and thumbnailing, can
manage multiple concurrent requests, and exposes a single cancel/timeout
surface.

### 2.3 Other removals

- `libvlc_Buffering` state value (in `libvlc_state_t`): use the media
  player's `on_buffering_changed` callback.
- `libvlc_media_get_parsed_status()` and the `libvlc_media_parsed_status_t`
  enum: use the new `libvlc_media_is_parsed()`, which returns a `bool`.
  The detailed outcome of a parse request is now reported only through
  `libvlc_parser_cbs.on_parsed` as a `libvlc_parser_status_t` (§6).
- `libvlc_media_discoverer_media_list()`: Use the `libvlc_media_discoverer_cbs`
  callbacks instead.
- `libvlc_media_list_player_set_media_player()`: the player must be the one
  supplied at construction time and cannot be swapped anymore.

---

## 3. Renamed APIs

| LibVLC 3 | LibVLC 4 |
|---|---|
| `libvlc_media_player_stop()` | `libvlc_media_player_stop_async()` |
| `libvlc_media_discoverer_release()` | `libvlc_media_discoverer_destroy()` |
| `libvlc_renderer_discoverer_release()` | `libvlc_renderer_discoverer_destroy()` |
| `libvlc_media_track_hold()` | `libvlc_media_track_retain()` |
| `libvlc_renderer_item_hold()` | `libvlc_renderer_item_retain()` |

The `release -> destroy` rename marks objects that are *not* reference-counted,
they have a single owner and one call frees them. The `hold -> retain`
rename aligns with the rest of the library (`libvlc_media_retain`,
`libvlc_picture_retain`, …).

### 3.1 Anonymous unions are now named `u`

Every public struct that used to embed an *anonymous* union now wraps it in a
named member `u`. The members themselves keep their names; only an extra `u.`
appears on the access path. This removes the reliance on the (non-standard in
C++, compiler-extension) anonymous-union behaviour and makes the discriminated
unions explicit.

The main motivation is auto-generated bindings. Tools that generate bindings
for other languages from the C headers cannot reliably name or address the
fields of an anonymous union, since those fields have no enclosing member to
qualify them. Giving the union an explicit name `u` gives every field a stable,
addressable path, so the bindings can be generated mechanically.

Affected structs and their fields:

| Struct (header) | Fields now under `.u` |
|---|---|
| `libvlc_media_track_t` (`libvlc_media_track.h`) | `audio`, `video`, `subtitle` |
| `libvlc_video_setup_device_info_t` (`libvlc_media_player.h`) | `d3d11`, `d3d9` |
| `libvlc_video_output_cfg_t` (`libvlc_media_player.h`) | `dxgi_format`, `d3d9_format`, `opengl_format`, `p_surface`, `anw` |

LibVLC 3:
```c
switch (track->i_type) {
case libvlc_track_video:
    w = track->video->i_width;   /* anonymous union */
    break;
case libvlc_track_audio:
    rate = track->audio->i_rate;
    break;
}
```

LibVLC 4:
```c
switch (track->i_type) {
case libvlc_track_video:
    w = track->u.video->i_width; /* named union member 'u' */
    break;
case libvlc_track_audio:
    rate = track->u.audio->i_rate;
    break;
}
```

The same `.u.` prefix applies inside the D3D11/D3D9/OpenGL video-callback
setup, e.g. `out->d3d11.device_context` becomes `out->u.d3d11.device_context`
and `out->dxgi_format` becomes `out->u.dxgi_format`.

---

## 4. Callback structs: the new event model

Every object that used to rely on an event manager now takes a versioned
`*_cbs` struct and an opaque pointer at construction. Every struct starts
with a `uint32_t version` field. Set it to the latest version number
available in the headers you compiled against — currently `0` for every
struct. Each field is documented with the version it became available in
(for example, "available since version 0"). The version you set tells LibVLC
exactly which fields you populated, so LibVLC can safely call every callback
you set. Setting a version lower than the fields you actually filled in
causes LibVLC to ignore those fields, silently dropping callbacks you
expected to fire. When you rebuild against newer headers that add fields and
you want to use them, raise this number to the new latest version.

**Forward ABI compatibility:** Future LibVLC releases may append new fields
to a `*_cbs` struct and raise the latest version number. Because structs are always passed
by pointer and fields are strictly append-only, the byte offsets of all
existing fields stay fixed. An old application's baked-in version number
tells LibVLC exactly how far into the struct it may read; everything beyond
that boundary is treated as NULL. A newer `libvlc.so` therefore runs old,
unmodified binaries correctly.

```c
static const struct libvlc_media_player_cbs cbs = {
    .version = 0,
    .on_state_changed    = my_on_state,
    .on_position_changed = my_on_position,
    /* remaining fields may be NULL if the callback is marked Optional */
};
libvlc_media_player_t *mp =
    libvlc_media_player_new(inst, &cbs, my_opaque);
```

Unused callbacks may be left NULL or zero-initialized when the documentation
marks them *Optional*. Callbacks marked *Mandatory* must be set.

**Lifetime and const-correctness:** LibVLC stores the pointer you pass; it
does not copy the struct. The callback object must therefore:

- outlive the LibVLC object it was registered with (the player,
  discoverer, etc.) and
- not be modified after it has been submitted.

Declare every `*_cbs` struct as `static const` when all of its fields are
compile-time constants.

### 4.1 Media player — `libvlc_media_player_cbs`

Defined in `<vlc/libvlc_media_player.h>`. Replaces every
`libvlc_MediaPlayer*` event. Full list:

| Callback | Replaces (v3 event) |
|---|---|
| `on_media_changed` | `libvlc_MediaPlayerMediaChanged` |
| `on_media_stopping` | `libvlc_MediaPlayerStopping` (now receives `libvlc_stopping_reason_t`) |
| `on_state_changed` | `libvlc_MediaPlayer{Opening,Playing,Paused,Stopped,…}` |
| `on_buffering_changed` | `libvlc_MediaPlayerBuffering` |
| `on_capabilities_changed` | `libvlc_MediaPlayerSeekableChanged`, `PausableChanged` |
| `on_position_changed` | `libvlc_MediaPlayerPositionChanged` / `TimeChanged` (receives both time in us and position) |
| `on_length_changed` | `libvlc_MediaPlayerLengthChanged` |
| `on_track_list_changed`, `on_track_selection_changed` | track-related events |
| `on_program_list_changed`, `on_program_selection_changed` | program-related events |
| `on_titles_changed`, `on_title_selection_changed` | title events |
| `on_chapter_selection_changed` | now delivers `libvlc_title_description_t *` and `libvlc_chapter_description_t *` directly |
| `on_recording_changed` | `libvlc_MediaPlayerRecordChanged` |
| `on_screenshot_taken` | `libvlc_MediaPlayerSnapshotTaken` |
| `on_media_parsed` | `libvlc_MediaParsedChanged` (delivered through the player) |
| `on_media_meta_changed` | `libvlc_MediaMetaChanged` |
| `on_media_subitems_changed` | `libvlc_MediaSubItemAdded` / `SubItemTreeAdded` |
| `on_media_attachments_added` | **new** — delivers `libvlc_picture_list_t *` attachments |
| `on_vout_changed` | `libvlc_MediaPlayerVout` |
| `on_cork_changed` | `libvlc_MediaPlayerCorked` / `Uncorked` |
| `on_audio_volume_changed`, `on_audio_mute_changed`, `on_audio_device_changed` | matching audio events |

New enums that appear in the callbacks:

- `libvlc_capability_t` — bitfield of `seek`, `pause`, `change_rate`, `rewind`.
- `libvlc_list_action_t` — `added`, `removed`, `updated`.
- `libvlc_stopping_reason_t` — `error`, `eos`, `user`.

### 4.2 Media player time watcher — `libvlc_media_player_watch_time_cbs`

For a simple "the time/position changed" notification, `on_position_changed`
(§4.1) is enough. When you need an **accurate**, output-synced playback time -
to drive a seek bar and a clock label - use `libvlc_media_player_watch_time()`
instead. It takes a callback struct:

```c
int libvlc_media_player_watch_time(libvlc_media_player_t *mp,
                                   libvlc_time_t min_period_us,
                                   const struct libvlc_media_player_watch_time_cbs *cbs,
                                   void *cbs_opaque);
```

with members `on_update` (mandatory), `on_paused`, `on_seek`. Call
`libvlc_media_player_unwatch_time()` to stop. Only one watcher can be
registered at a time.

**Why it exists.** In LibVLC 3 a time display was driven by the
`libvlc_MediaPlayerTimeChanged` / `PositionChanged` events, which reported the
*input* (demux) time. That time was only refreshed about every 250ms and
lagged the actual audio/video output by the output buffer size (300ms to 2s),
so a clock could be visibly wrong and out of sync with what was heard or seen.
The watcher instead reports points coming from the *output* itself - each
displayed video frame or written audio sample.

**How it is used.** Each `on_update` delivers a
`libvlc_media_player_time_point_t` carrying a `position`, a `rate`, a `ts_us`,
a `length_us` and a `libvlc_clock()`-based `system_date_us`. Because each point
embeds a system date and a rate, it is a self-contained value: `on_update` only
has to copy it to the UI thread once - that copy is the single point of
synchronization. From then on the UI mainloop can re-interpolate that *same*
point whenever it repaints. Interpolation reads only the copied point, takes no
lock and is a handful of arithmetic operations, so it is essentially free to
call as often as needed. Source updates can be sparse and irregular
(5ms, 1s, 10s…); interpolation fills the gaps:

- **Interpolate the time/position to "now"** with
  `libvlc_media_player_time_point_interpolate()`, driving a smooth seek bar at
  any frequency from the UI mainloop. `min_period_us` throttles how often
  `on_update` fires - it does not limit how often you may interpolate:

  ```c
  libvlc_time_t now = libvlc_clock(), ts_us;
  double pos;
  if (libvlc_media_player_time_point_interpolate(&point, now, &ts_us, &pos) == 0)
      ui_set_time(ts_us, pos);
  ```

- **Compute the system date of the next interval** (e.g. the next whole second)
  with `libvlc_media_player_time_point_get_next_date()`, to refresh a time
  label exactly on the second instead of polling for it.

`on_paused` tells the watcher to stop its interpolation timer; `on_seek`
reports the target while a seek is in flight (and `NULL` once it settles), so
the UI can snap to it instead of interpolating across the jump.

See [`player.c`](./player.c) for a complete example. This libvlc API wraps the
core `vlc_player_AddTimer()` (with `vlc_player_timer_point_Interpolate()` and
`vlc_player_timer_point_GetNextIntervalDate()`), used the same way by the Qt
interface in `modules/gui/qt/player/player_controller.cpp`.

### 4.3 Media discoverer — `libvlc_media_discoverer_cbs`

```c
libvlc_media_discoverer_t *
libvlc_media_discoverer_new(libvlc_instance_t *inst, const char *name,
                            const struct libvlc_media_discoverer_cbs *cbs,
                            void *cbs_opaque);
```

Callbacks: `on_media_added(opaque, parent, media)`,
`on_media_removed(opaque, media)`. The `parent` argument replaces the
removed `libvlc_media_discoverer_media_list()` function for parent tracking.

### 4.4 Renderer discoverer — `libvlc_renderer_discoverer_cbs`

```c
libvlc_renderer_discoverer_t *
libvlc_renderer_discoverer_new(libvlc_instance_t *inst, const char *name,
                               const struct libvlc_renderer_discoverer_cbs *cbs,
                               void *cbs_opaque);
```

Callbacks: `on_item_added` (mandatory), `on_item_removed` (optional).

### 4.5 Media — `libvlc_media_open_cbs`

`libvlc_media_new_callbacks()` no longer takes four function pointers – it
takes a struct:

LibVLC 3:
```c
libvlc_media_t *m = libvlc_media_new_callbacks(open_cb, read_cb, seek_cb,
                                               close_cb, opaque);
```

LibVLC 4:
```c
static const struct libvlc_media_open_cbs cbs = {
    .version = 0,
    .open    = my_open,   /* optional */
    .read    = my_read,   /* mandatory */
    .seek    = my_seek,   /* optional */
    .close   = my_close,  /* optional */
};
libvlc_media_t *m = libvlc_media_new_callbacks(&cbs, opaque);
```

### 4.6 Dialogs — `libvlc_dialog_cbs`

`libvlc_dialog_cbs` gained a `uint32_t version` field (initialise with
the latest version number, currently `0`). The error-display callback is no longer
part of the struct; register it separately with
`libvlc_dialog_set_error_callback()`.

### 4.7 Media list player — uses `libvlc_media_player_cbs`

`libvlc_media_list_player_new()` now takes the media-player callback struct
so that state / media changes can be observed without any list-player
events:

```c
libvlc_media_list_player_t *
libvlc_media_list_player_new(libvlc_instance_t *inst,
                             const struct libvlc_media_player_cbs *cbs,
                             void *cbs_opaque);
```

`libvlc_MediaListPlayerNextItemSet` is replaced by `on_media_changed`, and
`libvlc_MediaListPlayerPlayed` / `libvlc_MediaListPlayerStopped` by
`on_state_changed`.

### 4.8 Parser — `libvlc_parser_cbs` / `libvlc_thumbnailer_cbs`

See §6.

---

## 5. Microsecond unit

Functions that in LibVLC 3 used milliseconds now accept/return microseconds
through `libvlc_time_t`. A single typed time unit has been used throughout
the API. If you serialize or exchange times with other components (databases,
schedulers), review every boundary.

---

## 6. The new `libvlc_parser_t` API

Header: `<vlc/libvlc_parser.h>`

One parser object owns its own thread pool and can serve many concurrent
parse and thumbnail requests. Each queued request returns an opaque task
handle.

### 6.1 Lifetime

```c
struct libvlc_parser_cfg cfg = {
    .version                 = 0,
    .max_parser_threads      = 0,   /* 0 = default (1) */
    .max_thumbnailer_threads = 0,
    .timeout                 = 0,   /* in us, 0 = no timeout */
};
libvlc_parser_t *p = libvlc_parser_new(inst, &cfg);
/* ... */
libvlc_parser_destroy(p);
```

**NOTE:** `libvlc_parser_destroy` cancels all pending and running tasks, reports
it via their corresponding on_parsed/on_ended callback and blocks until all worker
threads are joined.

### 6.2 Parse a media

```c
/* cbs must outlive the returned task handle (i.e., until on_parsed is
   called on that particular task handle) */
static const struct libvlc_parser_cbs cbs = {
    .version              = 0,
    .on_parsed            = my_on_parsed,            /* mandatory */
    .on_attachments_added = my_on_attachments_added, /* optional */
};
libvlc_parser_request_t req = {
    .version     = 0,
    .media       = media,
    .parse_flags = libvlc_media_parse | libvlc_media_fetch_local,
};
libvlc_parser_task *task =
    libvlc_parser_queue(p, &req, &cbs, opaque);
/* later, when done: */
libvlc_parser_task_release(task);
```

`libvlc_parser_t` now allows re-queueing of an already parsed
`libvlc_media_t`. In LibVLC 3, a media that was already parsed was rejected.
In that case, the media will be parsed again because its metadata might have changed.

### 6.3 Generate a thumbnail

```c
/* tcbs must outlive the returned task handle (i.e., until on_ended is
   called on that particular task handle) */
static const struct libvlc_thumbnailer_cbs tcbs = {
    .version  = 0,
    .on_ended = my_on_thumbnail_ended, /* mandatory */
};
libvlc_thumbnailer_request_t treq = {
    .version = 0,
    .media   = media,
    .width   = 320,
    .height  = 0,      /* derived from aspect ratio */
    .crop    = false,
    .type    = libvlc_picture_Argb,
    .seek    = {
        .type  = libvlc_thumbnailer_seek_time,
        .value = { .time = 10 * 1000 * 1000, /* 10s, in us */ },
        .speed = libvlc_media_thumbnail_seek_fast,
    },
    .hw_dec  = false,
};
libvlc_parser_task *task =
    libvlc_parser_queue_thumbnailing(p, &treq, &tcbs, opaque);
/* later, when done: */
libvlc_parser_task_release(task);
```

### 6.4 Cancelling / introspection

- `libvlc_parser_cancel_request(p, task)` – cancel a specific request, or
  `NULL` to cancel everything. Returns the number of requests cancelled.
  Cancelled parse tasks fire `on_parsed` with
  `libvlc_parser_status_cancelled`. Cancelled thumbnail generation
  fire `on_ended` with `picture` as NULL.
- `libvlc_parser_task_get_media(task)` – borrowed pointer; do not release.
- `libvlc_parser_task_release(task)` – mandatory, releases the task handle
  (safe to call from inside `on_parsed` / `on_ended`). It does not
  cancel an in-flight request.
- `libvlc_parser_destroy(p)` – cancels all pending and running tasks, reports it
  via their corresponding on_parsed/on_ended callback and blocks until all
  worker threads are joined.

---

## 7. Migration checklist

Work through these in order; each step compiles more of your tree:

1. **Replace the event system.**
   - Remove every `libvlc_event_attach()` / `detach()` call and the
     associated dispatcher functions.
   - For each object, build a `*_cbs` struct with `version` set to the
     latest version number (currently `0`) and translate your handlers
     using the table in §4.1.
   - Delete any `libvlc_*_event_manager()` lookup.

2. **Update object construction.**
   - `libvlc_media_player_new(inst)` ->
     `libvlc_media_player_new(inst, &cbs, opaque)`.
   - Same for `libvlc_media_player_new_from_media()`,
     `libvlc_media_list_player_new()`,
     `libvlc_media_discoverer_new()`,
     `libvlc_renderer_discoverer_new()`.
   - `libvlc_media_new_callbacks(open, read, seek, close, op)` ->
     `libvlc_media_new_callbacks(&cbs_struct, op)`.

3. **Rename destructors / retainers.**
   - `libvlc_media_discoverer_release` -> `_destroy`.
   - `libvlc_renderer_discoverer_release` -> `_destroy`.
   - `libvlc_media_track_hold` -> `_retain`.
   - `libvlc_renderer_item_hold` -> `_retain`.

4. **Switch times to microseconds.** Audit every call site involving time unit.
   Convert `(ms)` literals to `(ms * 1000)`, rename locals, update database
   schemas if needed.

5. **Port parsing / thumbnailing.** Replace
   `libvlc_media_parse_request()` / `libvlc_media_thumbnail_request_by_*()`
   calls with a shared `libvlc_parser_t` per libvlc instance; use
   `libvlc_parser_queue()` / `libvlc_parser_queue_thumbnailing()` and remove
   `libvlc_media_parse_stop()`.

6. **Update the dialog struct.** Add `.version = 0` (the latest
   version). Move your error handler to
   `libvlc_dialog_set_error_callback()`.

7. **Review async control flow.** Anywhere your code assumed
   `libvlc_media_player_stop()` / `set_pause()` / `set_media()` returned
   after the state change, introduce a wait on `on_state_changed` /
   `on_media_changed` / `on_media_stopping`.

8. **Drop removed enum values.**
   - `libvlc_Buffering`
   - `libvlc_media_parsed_status_t` enum (replace
     `libvlc_media_get_parsed_status()` with `libvlc_media_is_parsed()`).
   - Old `libvlc_media_parse_flag_t` flags (`parse_local`, `parse_network`,
     `parse_forced`).

9. **Add the `u.` prefix on named unions.** For every anonymous-union access
   listed in §3.1, insert `u.`: `track->video` -> `track->u.video`,
   `out->dxgi_format` -> `out->u.dxgi_format`, etc. The compiler flags each one.

10. **Recompile with `-Werror`.** The majority of remaining breakage will be
    trivial adaptations.

---

## 8. Reference: sample code

Ported examples are available under `doc/libvlc/` and exercise the new
callback-based APIs:

- [`player.c`](./player.c) – media player + time watcher
- [`parser.c`](./parser.c) – parser queue
- [`thumbnailer.c`](./thumbnailer.c) – thumbnailing through the parser
- [`media_discoverer.c`](./media_discoverer.c) – discoverer callbacks
- [`renderer_discoverer.c`](./renderer_discoverer.c) – renderer callbacks

Follow-up reading: the in-header doxygen documents every callback field and its
version field.

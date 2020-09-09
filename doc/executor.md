# Executor

## Overview

The executor API allows to submit _runnables_ to be executed from background
threads.

The runnable instances are allocated and freed by the caller. It is the
responsibility of the caller to guarantee that the runnable is valid until
the task is canceled or complete.

The caller is expected to (but not forced to) embed the runnable into its
custom task structure:

```c
struct my_task
{
    /* custom data */
    int num;
    char *str;

    /* embedded runnable */
    struct vlc_runnable runnable;
};

void Run(void *userdata)
{
    // ...
}
```

and submit it as follow:

```c
    struct my_task *task = ...;

    task->runnable.run = Run;
    task->runnable.userdata = task; /* userdata passed to Run() */

    vlc_executor_Submit(executor, &task->runnable);
```

Since the task is allocated by the caller, vlc_executor_Submit() may not fail
(it returns void).

The cancelation of a submitted runnable may be requested. It succeeds only if
the runnable is not started yet.

```c
VLC_API void
vlc_executor_Submit(vlc_executor_t *executor, struct vlc_runnable *runnable);

VLC_API bool
vlc_executor_Cancel(vlc_executor_t *executor, struct vlc_runnable *runnable);
```


## Design discussions

The design of this API is the result of many discussions, especially on the
mailing-list:
 - <https://mailman.videolan.org/pipermail/vlc-devel/2020-August/136696.html>
 - <https://mailman.videolan.org/pipermail/vlc-devel/2020-September/136944.html>
 - <https://mailman.videolan.org/pipermail/vlc-devel/2020-September/137188.html>

This documentation aims to explain the rationale for each decision.


### Runnable allocation

The `struct vlc_runnable` must be provided by the client, and the executor uses
only this instance to represent its task (it does not allocate per-task private
data).

Firstly, note that this allows `vlc_executor_Submit()` to return `void`, so no
error handling is necessary. It's minor, but convenient.

An attractive alternative would be to return a new allocated runnable on
submission, which could be used for cancelation:

```c
struct vlc_runnable *
vlc_executor_Submit(vlc_executor_t *, void (*run)(void *), void *userdata);

void vlc_executor_Cancel(vlc_executor_t *, struct vlc_runnable *);
```

But this is inherently racy: a naive implementation could return a pointer to a
`vlc_runnable` already freed. Indeed, `vlc_executor_Submit()` must post the
runnable to some pending queue, and in theory it may be executed by a background
thread even before the function returns.

For example, if the executor frees the runnable as soon as its execution is
complete, this simple code is incorrect:

```c
struct my_task
{
    char *some_custom_data;
    struct vlc_runnable *runnable; /* keep a pointer to cancel it */
};

void Run(void *userdata)
{
    struct my_task *task = userdata;
    /* (task->runnable may be uninitialized here) */
}
```

```c
    struct my_task *task = ...;
    task->runnable = vlc_executor_Submit(executor, Run, task);
    /* task->runnable may be already freed here */
    vlc_executor_Cancel(executor, task->runnable); /* boom, use-after-free */
```

To avoid the use-after-free, the runnable ownership must be shared. This makes
the API more complex:

```c
struct vlc_runnable_t *runnable = vlc_executor_Submit(executor, Run, task);
vlc_runnable_Release(runnable); /* don't need the runnable anymore */
```

However, this would still be insufficient: the task resources must typically be
released at the end of the `run()` callback, but at this point
`vlc_executor_Submit()` may not have returned yet, so the runnable instance
would not be known by the user.

Since in practice, the client needs its own task structure (to store the
parameters, state and result of the execution), so it must allocate it and pass
it to the executor (as userdata) anyway. Therefore, it's simpler if the task
already embed the runnable.

Note that it is not mandatory to embed the runnable into the client task
structure (it's just convenient). The runnable could be anywhere, for example on
the stack (provided that it lives long enough).

This design also has disadvantages. In particular, it forces the runnable queue
to be implemented as an intrusive list. As a consequence, the public `struct
vlc_runnable` must include an executor-private field (`struct vlc_list node`),
and the same runnable may not be queued twice (it is not possible to put the
same item twice in the same intrusive list).

Therefore, a client must always explicitly create a new runnable for each
execution. In practice, it should not be a problem though, since it must often
create a new custom task for the execution state anyway.

In addition, further extensions of the executor API are constrained by the fact
that it could not allocate and store per-task data (other than the intrusive
list node).


### Cancelation

It is important to be able to cancel and interrupt a running task.

However, in C, the interruption mechanism is very specific to the concrete task
implementation, so it could not be provided by the executor: it has to be
provided by the user. And since the user is also at the origin of the
cancelation request (which could lead to the interruption), the executor just
let the user handle interruption manually.

Since it does not handle interruption, it could not provide a generic timeout
mechanism either (see below). The user has to handle timeout manually.

It just provides a way to cancel a queued task not started yet:

```c
VLC_API bool
vlc_executor_Cancel(vlc_executor_t *executor, struct vlc_runnable *runnable);
```

The runnable instance is created and destroyed by the user, so there is no
possible use-after-free race condition.

Since the runnable is queued via an intrusive list, it can be removed in O(1).

The tricky part is that it must indicate to the user if the runnable has
actually been dequeued or not (i.e. if it has already been taken by a thread to
be run). Indeed, the user must be able to know if the `run()` callback will be
executed or not, in order to release the task resources correctly in all cases,
without race conditions. This adds some complexity for the user.

For the implementation, the problem is that even if we can remove an item in an
intrusive list in O(1), there is a priori no way to know immediately if the item
was in a specific list or not. To circumvent this problem, when an item is
dequeued, the executor resets the list nodes to `NULL`. This allows to store 1
bit of information indicating whether the item has been dequeued or not.

Note that the use-after-free race condition on cancel could alternatively be
solved by passing an optional user-provided `void *id`:

```c
struct vlc_runnable
{
    void (*run)(void *userdata)
    void *userdata;
    struct vlc_list node;

    /* add a user-provided id (may be NULL) used for cancelation */
    void *id;
}

void vlc_executor_Submit(vlc_executor_t *executor, struct vlc_runnable *);

void vlc_executor_Cancel(vlc_executor_t *executor, void *id);
```

This would solve the problem because the user owns the `id`, so it is guaranteed
to exist on cancel. This is also practical, because the user could just pass a
pointer to its custom task structure.

An additional advantage is that a higher-level API (like the preparser) could
directly benefit from the cancelation API, without tracking its submitted tasks
to find the matching runnable.

But it would only make sense if the user was not already forced to track its
submitted tasks, to be able to cancel them on deletion. Indeed, since the
executor must report for each task if it has been dequeued or not, it is not
possible to provide a function to cancel all tasks at once. Therefore, the user
has to keep track of submitted tasks, which is inconvenient.


### Timeout

A timeout implementation is often very specific to the concrete task
implementation (for example, calling `vlc_cond_timedwait()` instead of
`vlc_cond_wait()`), so it makes sense to let the user implement it.

However, we could see the timeout as a cancelation where the deadline is known
in advance. Since the user must already implement the interruption on its own,
it could have been used to provide a timeout mechanism "for free" (for the
user).

We decided against it, for several reasons:
 - in theory, the user may want to do something different on timeout and
   cancelation (it may for example consider that one is an error and the other
   is not);
 - this would require more callbacks and would complexify the executor API;
 - if it's necessary, the user could just use a separate component (timer) to
   trigger the interruption.


## Conclusion

The resulting executor is a "minimal" API, simple and general: it just handles
execution.

As a drawback, it does not help for interruption, cancelation and timeout, so
the user has more work to do on its own. For example, it must always track its
submitted tasks, and implement boilerplate to correctly handle cancelation and
interruption on deletion.

But I can't think of an executor API design in C with all the desirable
properties. Any design I considered sucked one way or another. :/

#!/bin/sh

usage()
{
cat << EOF
usage: $0 <original_branch> [commit_range]

Check that cherry pick commits are properly formatted/documented.
Differing patches are put in patches-new/ and patches-orig/

original_branch:  the branch where the cherry-picked commits come from
commit_range   :  the commits to check (origin/master..HEAD if not specified)

- edits are marked as
(cherry picked from commit <hash>) (edited)
edited:

- rebases (unmodified code) are marked as
(cherry picked from commit <hash>) (rebased)
rebased:
EOF
}

if test -z "$1"; then
    echo "Missing original branch"
    usage
    exit 1
fi
ORIGINAL_BRANCH=$1

if test -n "$2"; then
    COMMIT_RANGE=$2
else
    COMMIT_RANGE="origin/master..HEAD"
fi

CHERY_PICK_DETECT="cherry picked from commit "

NOT_CHERRY_PICKED=`git log --invert-grep --grep "$CHERY_PICK_DETECT" --pretty=%h $COMMIT_RANGE`
if test -n "$NOT_CHERRY_PICKED"; then
    echo "ERROR: some commits are not cherry-picked:"
    git log --invert-grep --grep "$CHERY_PICK_DETECT" --pretty=oneline $COMMIT_RANGE
    exit 1
fi

CHERRY_PICKED=`git log --grep "$CHERY_PICK_DETECT" --pretty=%H $COMMIT_RANGE`
mkdir -p patches-new
mkdir -p patches-orig

EXIT_CODE=0
for l in $CHERRY_PICKED; do
    GIT_LOG=`git log -n 1 $l~1..$l`

    h=`echo "$GIT_LOG" | grep "$CHERY_PICK_DETECT" | sed -e "s/cherry picked from commit//" -e "s/Edited and //" -e "s/(//" | cut -c6-45`

    # Check that the cherry picked hash exist in the original branch
    HASH_EXISTS=`git merge-base --is-ancestor $h $ORIGINAL_BRANCH 2> /dev/null || echo FAIL`
    if test -n "$HASH_EXISTS"; then
        echo "Invalid Hash for:"
        git log -n 1 $l
        exit 1
    fi

    # Check the cherry picked commit has a Signed Off mark
    SIGNED_OFF=`echo "$GIT_LOG" | grep "Signed-off-by: "`
    if test -z "$SIGNED_OFF"; then
        echo "Missing signed off for:"
        git log -n 1 $l
    fi

    IS_MARKED_REBASED=`echo "$GIT_LOG" | grep "(cherry picked from commit $h) (rebased)"`
    IS_MARKED_EDITED=`echo "$GIT_LOG" | grep "(cherry picked from commit $h) (edited)"`

    git diff --no-color --minimal --ignore-all-space $l~1..$l | sed -e "/index /d" -e "s/@@ .*/@@/" > patches-new/$h.diff
    git diff --no-color --minimal --ignore-all-space $h~1..$h | sed -e "/index /d" -e "s/@@ .*/@@/" > patches-orig/$h.diff
    PATCH_DIFF=`diff -t -w patches-new/$h.diff patches-orig/$h.diff | sed -e '/^> --- /d' -e '/^> +++ /d' -e '/^---/d' -e '/^> @@/d' -e '/^< @@/d' -e '/^[0-9]\+/d'`
    if test -z "$PATCH_DIFF"; then
        if test -n "$IS_MARKED_EDITED"; then
            echo "Incorrectly marked edited: $(git log $l~1..$l --oneline)"
            EXIT_CODE=1
        fi
        if test -n "$IS_MARKED_REBASED"; then
            echo "Incorrectly marked rebased: $(git log $l~1..$l --oneline)"
            EXIT_CODE=1
        fi
        rm -rf patches-new/$h.diff patches-orig/$h.diff
        continue
    fi

    PATCH_DIFF2=`echo "$PATCH_DIFF" | sed -e "/\(> -\|> +\| diff \)/!d" | sed -e "/> +$/d" -e "/> -$/d"`

    if test -n "$IS_MARKED_REBASED"; then
        if test -n "$PATCH_DIFF2"; then
            echo "Edit marked as rebase: $(git log $l~1..$l)"
            EXIT_CODE=1
            continue
        fi

        IS_EXPLAINED=`echo "$GIT_LOG" | grep "rebased:"`
        if test -z "$IS_EXPLAINED"; then
            echo "Unexplained rebase: $(git log $l~1..$l)"
            EXIT_CODE=1
        else
            rm -rf patches-new/$h.diff patches-orig/$h.diff
        fi
    else
        if test -z "$PATCH_DIFF2"; then
            echo "Rebase marked as edit: $(git log $l~1..$l)"
            EXIT_CODE=1
            continue
        fi

        if test -z "$IS_MARKED_EDITED"; then
            echo "Unmarked edit: $(git log $l~1..$l)"
            EXIT_CODE=1
        else
            IS_EXPLAINED=`echo "$GIT_LOG" | grep "edited:"`
            if test -z "$IS_EXPLAINED"; then
                echo "Unexplained edit: $(git log $l~1..$l)"
                EXIT_CODE=1
            else
                rm -rf patches-new/$h.diff patches-orig/$h.diff
            fi
        fi
    fi
done

exit $EXIT_CODE

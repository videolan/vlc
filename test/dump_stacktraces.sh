#!/bin/sh

# Helper used to print more information (log + core dump) about a failing test

if [ $# -gt 0 ];then
    # exit code passed in from stdin
    ret=$1
else
    ret=1
fi

if [ $ret -eq 0 ] || ! (command -v gdb >/dev/null);then
    exit $ret
fi

# test failed

for i in $(find -name test-suite.log);do
    # Search for a failing test
    error_cnt=$(sed -n 's/^# FAIL: *\([^ ]\+\)/\1/p' "$i")
    if [ $error_cnt -gt 0 ];then
        cat "$i"

        test_path=$(dirname "$i")
        core_path="$test_path/core"
        failing_test=$(sed -n 's/^FAIL \([^ ]\+\) (exit status:.*/\1/p' ${test_path}/test-suite.log)
        if [ -f "$core_path" -a ! -z "$failing_test" ];then
            if [ -x "$test_path/.libs/$failing_test" ];then
                failing_test_path="$test_path/.libs/$failing_test"
            else
                failing_test_path="$test_path/$failing_test"
            fi
            echo "Printing core dump:"
            echo ""
            gdb "$failing_test_path" -c "$core_path" \
                -ex "set pagination off" \
                -ex "thread apply all bt" \
                -ex "quit"
        fi
    fi
done

exit $ret

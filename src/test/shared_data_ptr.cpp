#include <assert.h>
#include <algorithm>
#include <vector>
#include <vlc_cxx_helpers.hpp>

struct mockrc
{
    int count = 1;
};

static void mockrc_Hold(mockrc *p)
{
    assert(p->count > 0);
    p->count++;
}

static void mockrc_Release(mockrc *p)
{
    assert(p->count > 0);
    p->count--;
}

using MockRcPtr = vlc_shared_data_ptr_type(mockrc, mockrc_Hold, mockrc_Release);

static void test_raii()
{
    mockrc mock;

    {
        MockRcPtr ptr(&mock, true);
        assert(ptr);
        assert(mock.count == 2);
        assert(ptr->count == 2);

        {
            mockrc_Hold(&mock);
            /* transfer the ownership */
            MockRcPtr ptr2(&mock, false);
            assert(ptr->count == 3);
        }

        assert(ptr->count == 2);
    }

    assert(mock.count == 1);
}

static void test_assignment()
{
    mockrc mock;

    MockRcPtr ptr(&mock, false);
    assert(mock.count == 1);

    {
        MockRcPtr ptr2 = ptr;
        assert(mock.count == 2);

        {
            MockRcPtr ptr3 = std::move(ptr2);
            assert(mock.count == 2);

            /* assign a nullptr, this releases the previous one */
            ptr3 = {};
            assert(mock.count == 1);

            ptr3 = MockRcPtr(&mock);
            assert(mock.count == 2);

            MockRcPtr ptr4(std::move(ptr3));
            assert(mock.count == 2);
        }
        /* ptr4 is the only wrapper alive (ptr3 had been moved), count is
         * decremented by 1 */
        assert(mock.count == 1);
    }

    /* ptr2 had been moved, no decrement */
    assert(mock.count == 1);

    ptr = ptr; /* self-assignement should have no effect */
    assert(mock.count == 1);
}

static void test_reset()
{
    mockrc mock;

    {
        MockRcPtr ptr(&mock, false);
        assert(mock.count == 1);

        /* hold once, release once, count is not changed */
        ptr.reset(&mock, true);
        assert(mock.count == 1);

        mockrc_Hold(&mock);
        assert(mock.count == 2);

        ptr.reset();
        assert(mock.count == 1);

        ptr.reset(&mock, false);
        assert(mock.count == 1);
    }

    /* ownership had transferred to ptr, which has been destroyed */
    assert(mock.count == 0);
}

static void test_vector()
{
    mockrc mock;

    {
        std::vector<MockRcPtr> vec(10, MockRcPtr(&mock, false));
        assert(mock.count == 10);

        std::random_shuffle(vec.begin(), vec.end());
        assert(mock.count == 10);

        {
            auto vec2 = vec;
            assert(mock.count == 20);

            vec2.push_back(MockRcPtr(&mock, true));
            assert(mock.count == 21);
        }

        assert(mock.count == 10);

        vec.erase(vec.begin() + 2, vec.begin() + 6);
        assert(mock.count == 6);

        vec.emplace_back(&mock, true);
        assert(mock.count == 7);
    }

    /* ownership was transferred to vec constructor */
    assert(mock.count == 0);
}

int main() {
    test_raii();
    test_assignment();
    test_reset();
    test_vector();
    return 0;
}

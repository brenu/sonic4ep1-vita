#include "sonic_dialog_policy.h"
#include <assert.h>
#include <stdio.h>
int main(void) {
    assert(sonic_dialog_button_count(0, 0) == 1);
    assert(sonic_dialog_button_count(0, 1) == 1);
    assert(sonic_dialog_button_count(1, 0) == 2);
    assert(sonic_dialog_button_count(1, 1) == 3);
    for (int count = 1; count <= 3; ++count) {
        for (int id = 1; id <= count; ++id)
            assert(sonic_dialog_button_index(count, id) == id - 1);
        assert(sonic_dialog_button_index(count, 0) == -1);
        assert(sonic_dialog_button_index(count, count + 1) == -1);
    }
    assert(sonic_dialog_button_index(0, 1) == -1);
    puts("Dialog buttons: Android one/two/three-button ordering and cancellation passed");
}

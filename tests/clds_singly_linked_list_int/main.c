// Licensed under the MIT license. See LICENSE file in the project root for full license information.

#include <stddef.h>
#include "testrunnerswitcher.h"

int main(void)
{
    size_t failedTestCount = 0;
    RUN_TEST_SUITE(clds_singly_linked_list_inttests, failedTestCount);
    return failedTestCount;
}

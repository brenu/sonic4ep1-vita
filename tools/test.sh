#!/usr/bin/env bash
set -euo pipefail
SONIC_TEST_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$SONIC_TEST_ROOT"
mkdir -p work/tests
cc -std=c11 -Wall -Wextra -Werror -I lib -I source \
  source/sonic_jni_registry.c tools/test_jni_registry.c -o work/tests/jni_registry
work/tests/jni_registry
cc -std=c11 -Wall -Wextra -Werror -I lib -I source \
  source/sonic_java_descriptors.c tools/test_java_descriptors.c -o work/tests/java_descriptors
work/tests/java_descriptors
cc -std=c11 -Wall -Wextra -Werror -I source \
  tools/test_memory_policy.c -o work/tests/memory_policy
work/tests/memory_policy
cc -std=c11 -Wall -Wextra -Werror -I source \
  source/sonic_platform_data.c tools/test_platform_data.c -o work/tests/platform_data
work/tests/platform_data
cc -std=c11 -Wall -Wextra -Werror -I source \
  source/sonic_library_policy.c tools/test_library_policy.c -o work/tests/library_policy
work/tests/library_policy
cc -std=c11 -Wall -Wextra -Werror -I source \
  tools/test_graphics_policy.c -o work/tests/graphics_policy
work/tests/graphics_policy
cc -std=c11 -Wall -Wextra -Werror -I source \
  tools/test_canvas.c -o work/tests/canvas
work/tests/canvas
cc -std=c11 -Wall -Wextra -Werror -I source \
  source/sonic_pcm.c tools/test_pcm.c -o work/tests/pcm
work/tests/pcm
cc -std=c11 -Wall -Wextra -Werror -pthread -I source \
  source/sonic_os_queue.c tools/test_os_queue.c -o work/tests/os_queue
work/tests/os_queue
cc -std=c11 -Wall -Wextra -Werror -I source \
  source/sonic_expansion.c tools/test_expansion.c -o work/tests/expansion
work/tests/expansion
cc -std=c11 -Wall -Wextra -Werror -D_DEFAULT_SOURCE -I source \
  tools/test_android_stat.c -o work/tests/android_stat
work/tests/android_stat
cc -std=c11 -Wall -Wextra -Werror -I source \
  tools/test_dialog_policy.c -o work/tests/dialog_policy
work/tests/dialog_policy
cc -std=c11 -Wall -Wextra -Werror -I source \
  tools/test_stack_policy.c -o work/tests/stack_policy
work/tests/stack_policy
cc -std=c11 -Wall -Wextra -Werror -I source \
  tools/test_media_range.c -o work/tests/media_range
work/tests/media_range
cc -std=c11 -Wall -Wextra -Werror -I source \
  tools/test_touch_policy.c -o work/tests/touch_policy
work/tests/touch_policy
cc -std=c11 -Wall -Wextra -Werror -I source \
  tools/test_pad_policy.c -o work/tests/pad_policy
work/tests/pad_policy
cc -std=c11 -Wall -Wextra -Werror -I source \
  tools/test_title_policy.c -o work/tests/title_policy
work/tests/title_policy
cc -std=c11 -Wall -Wextra -Werror -I source \
  tools/test_perf_policy.c -o work/tests/perf_policy
work/tests/perf_policy

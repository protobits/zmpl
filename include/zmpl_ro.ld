/*
 * Copyright (c) 2020 The ZMK Contributors
 *
 * SPDX-License-Identifier: MIT
 */
 
#include <linker/linker-defs.h>

#ifdef CONFIG_BOARD_NATIVE_POSIX
zmpl_topic_sections :
{
#endif
	__start__zmpl_topics = .;
	KEEP(*(__zmpl_topics));
	__stop__zmpl_topics = .;
#ifdef CONFIG_BOARD_NATIVE_POSIX
}
#endif


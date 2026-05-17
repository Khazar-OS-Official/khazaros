#ifndef AC97_H
#define AC97_H

#include <libk/types.h>

void ac97_init(void);
void ac97_play_sample(void *buffer, uint32_t size);

#endif // AC97_H

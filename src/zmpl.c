#include <zmpl.h>

const struct zmpl_topic* zmpl_get_topic(const char* topic)
{
  /* Do search assuming static string used */
  
  __ASSERT((size_t)((uint8_t*)__stop__zmpl_topics - (uint8_t*)__start__zmpl_topics) % sizeof(struct zmpl_topic) == 0,
    "no padding inserted by linker (apropriate struct zmpl_topic alignment for platform)");

  for (const struct zmpl_topic* t = __start__zmpl_topics; t != __stop__zmpl_topics; ++t)
    {
    if (t->name == topic)
    {
      return t;
    }
  }

  return NULL;
}

void zmpl_publish(struct zmpl_publisher* pub, void* msg)
{
  int ret;
  struct zmpl_subscriber* sub;

  __ASSERT((size_t)((uint8_t*)pub->topic->subscribers_end - (uint8_t*)pub->topic->subscribers_start) % sizeof(struct zmpl_subscriber) == 0,
    "no padding inserted by linker (apropriate struct zmpl_subscriber alignment for platform)");

  for (sub = pub->topic->subscribers_start; sub != pub->topic->subscribers_end; sub++)
  {
    ret = k_msgq_put(&sub->queue, msg, pub->mode == ZMPL_BLOCKING ? K_FOREVER : K_NO_WAIT);
  }
}
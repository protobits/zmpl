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

  /*__ASSERT((size_t)((uint8_t*)pub->topic->subscribers_end - (uint8_t*)pub->topic->subscribers_start) % sizeof(struct zmpl_subscriber) == 0,
    "no padding inserted by linker (apropriate struct zmpl_subscriber alignment for platform)");*/

  for (sub = pub->topic->subscribers_start; sub != pub->topic->subscribers_end; sub++)
  {
    __ASSERT(pub->topic == sub->topic, "corrupted pub/sub topic mapping, did you defined the topic with zmpl_define_topics() in cmake?");
    ret = k_msgq_put(&sub->queue, msg, pub->mode == ZMPL_BLOCKING ? K_FOREVER : K_NO_WAIT);
  }
}

void zmpl_handle_messages(struct zmpl_subscriber* const sub_list[],
  struct k_poll_event* pe, size_t num_events)
{
  for (size_t i = 0; i < num_events; i++)
  {
    if (pe[i].state == K_POLL_STATE_MSGQ_DATA_AVAILABLE)
    {
      if (sub_list[i]->callback)
      {
        (*sub_list[i]->callback)(sub_list[i]);
      }

      pe[i].state = K_POLL_STATE_NOT_READY;
    }
  }
}

int zmpl_spin_once(struct zmpl_subscriber* const sub_list[],
  struct k_poll_event* pe, size_t num_events, k_timeout_t timeout)
{
  int ret = k_poll(pe, num_events, timeout);
  
  if (ret == 0)
  {
    zmpl_handle_messages(sub_list, pe, num_events);
  }

  return ret;
}
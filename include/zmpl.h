#ifndef __ZMPL_H__
#define __ZMPL_H__

#include <zephyr.h>
#include <sys/util.h>
#include <kernel.h>

struct zmpl_subscriber;

struct zmpl_topic
{
  const char* name;
  size_t msg_size;
  struct zmpl_subscriber* subscribers_start;
  const struct zmpl_subscriber* subscribers_end;
};

/* NOTE: since the placement of these structs in memory will be done
 * by the linker and the linker decides how to align these in memory
 * (potentially leaving gaps between elements), we need to add padding
 * to the structs to fill these holes. To do so, we instruct the compiler
 * to align these to the require size, depending on platform
 */

/* TODO: check if these alignments work on other platforms */

#ifdef CONFIG_BOARD_NATIVE_POSIX
#define _ZMPL_STRUCT_ALIGN 64
#else
#define _ZMPL_STRUCT_ALIGN 8
#endif

struct zmpl_subscriber
{
  const struct zmpl_topic* topic;
  struct k_msgq queue;
} __aligned(_ZMPL_STRUCT_ALIGN);

enum zmpl_publish_mode
{
  ZMPL_BLOCKING = 0,
  ZMPL_DROP,
  /* ZMPL_OVERWRITE, */ /* TODO: not supported since we need to manually drop data and is inefficient */
};

struct zmpl_publisher
{
  const struct zmpl_topic* topic;
  struct k_msgq queue; /* Not used yet */
  enum zmpl_publish_mode mode;
} __aligned(_ZMPL_STRUCT_ALIGN);

/* Section markers for topics */

extern const struct zmpl_topic __start__zmpl_topics[];
extern const struct zmpl_topic __stop__zmpl_topics[];

/* Auxiliary macros */

#define _SLASH_JOIN(component) "/" STRINGIFY(component)

/* Convert topic from individual components to string joined with '/':
 *   ZMPL_TOPIC_NAME(a,b,c) = "/a/b/c"
 */
#define ZMPL_TOPIC_NAME(topic_components...) FOR_EACH(_SLASH_JOIN,(),topic_components)

/* Convert topic from individual components to token joined by '_':
  *   ZMPL_TOPIC(a,b,c) = a_b_c
  */
#define ZMPL_TOPIC(topic_components) FOR_EACH(IDENTITY,(_),topic_components)

/* Convert topic from individual components to corresponding subscriber identifier:
  *   ZMPL_SUBSCRIBER_IDENTIFIER(a,b,c) = __zmpl_subscribers_a_b_c
  */
#define ZMPL_SUBSCRIBER_IDENTIFIER(topic_components...) _CONCAT(__zmpl_subscribers_, ZMPL_TOPIC(topic_components))

/* Convert topic from individual components to corresponding topic identifier:
 *   ZMPL_TOPIC_IDENTIFIER(a,b,c) = __zmpl_topic_a_b_c
 */
#define ZMPL_TOPIC_IDENTIFIER(topic_components...) _CONCAT(__zmpl_topic_, ZMPL_TOPIC(topic_components))

 /* Static topic definition:
  *
  *   Example: ZMPL_TOPIC_DEFINE(struct battery_state_msg, ZMPL_TOPIC(battery_controller, state))
  *     This corresponds to a /battery_controller/state topic, used to send 'struct battery_state_msg'
  *     instances.
  *
  * This defines a "struct zmpl_topic" instance, pointing to the start and end of the subscribers array
  * for this topic. As all zmpl_subscriber instances for a topic are placed in an individual section for
  * that topic, we use the special sections __start_<section name> and __stop_<section_name>
  * (provided by the linker) to get these pointers.
  */

#define ZMPL_TOPIC_DEFINE(msg_type, topic_components...) \
  extern struct zmpl_subscriber _CONCAT(__start_,ZMPL_SUBSCRIBER_IDENTIFIER(topic_components))[]; \
  extern const struct zmpl_subscriber _CONCAT(__stop_,ZMPL_SUBSCRIBER_IDENTIFIER(topic_components))[]; \
  static const struct zmpl_topic ZMPL_TOPIC_IDENTIFIER(topic_components) \
    __used __attribute__((__section__("__zmpl_topics"))) \
      = { .name = ZMPL_TOPIC_NAME(topic_components), \
          .msg_size = sizeof(msg_type), \
          .subscribers_start = _CONCAT(__start_,ZMPL_SUBSCRIBER_IDENTIFIER(topic_components)), \
          .subscribers_end = _CONCAT(__stop_,ZMPL_SUBSCRIBER_IDENTIFIER(topic_components)), \
        };

/* Static subscriber definition:
 *   Example: ZMPL_SUBSCRIBER_DEFINE(sub1, struct battery_state_msg, 10, ZMPL_TOPIC(battery_controller, state))
 *     Defines a "struct zmpl_subscriber sub1" for topic /battery_controller/state and an outgoig queue of size 10.
 *
 * This macro defines the instance as a read-write struct (since the queue is mutable) and also declares a static
 * buffer for the underlying queue.
 */

#define ZMPL_SUBSCRIBER_DEFINE(subscriber, msg_type, queue_size, topic_components...) \
  extern const struct zmpl_topic ZMPL_TOPIC_IDENTIFIER(topic_components); \
  static msg_type __noinit _CONCAT(__zmpl_subscriber_data_, subscriber)[queue_size]; \
  struct zmpl_subscriber subscriber \
    __used __attribute__((__section__(STRINGIFY(ZMPL_SUBSCRIBER_IDENTIFIER(topic_components))))) = \
  { \
    .topic = &ZMPL_TOPIC_IDENTIFIER(topic_components), \
    .queue = Z_MSGQ_INITIALIZER(subscriber.queue, (char*)_CONCAT(__zmpl_subscriber_data_, subscriber), sizeof(msg_type), queue_size), \
  };

/* Static publisher definition:
 *   Example: ZMPL_PUBLISHER_DEFINE(pub1, struct battery_state_msg, 10, ZMPL_TOPIC(battery_controller, state))
 *     Defines a "struct zmpl_publisher pub1" for topic /battery_controller/state and an outgoig queue of size 10.
 *
 * Same as subscriber case.
 */

#define ZMPL_PUBLISHER_DEFINE(publisher, msg_type, queue_size, topic_components...) \
  extern const struct zmpl_topic ZMPL_TOPIC_IDENTIFIER(topic_components); \
  static msg_type __noinit _CONCAT(__zmpl_publisher_data_, publisher)[queue_size]; \
  struct zmpl_publisher publisher = \
  { \
    .topic = &ZMPL_TOPIC_IDENTIFIER(topic_components), \
    .queue = Z_MSGQ_INITIALIZER(publisher.queue, (char*)_CONCAT(__zmpl_publisher_data_, publisher), sizeof(msg_type), queue_size), \
  };

#define _ZMPL_POLL_EVENT_INIT(sub) K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_MSGQ_DATA_AVAILABLE, K_POLL_MODE_NOTIFY_ONLY, &sub.queue, 0)

 /* Initializer for static struct k_poll_event[].
  *
  *  Example: struct k_poll_event[] = { ZMPL_POLL_EVENT_INITIALIZER(sub1, sub2, sub3) };
  */

#define ZMPL_POLL_EVENT_INITIALIZER(subscribers...) \
    FOR_EACH(_ZMPL_POLL_EVENT_INIT, (,), subscribers)

#ifdef __cplusplus
extern "C"
{
#endif

/* Publish a msg on the given publisher */

void zmpl_publish(struct zmpl_publisher* pub, void* msg);

/* Utility function to find a "struct zmpl_topic" instance from the given
 * slash-delimited topic string. This will go through each registered topic.
 * If you already have access to a subscriber/publisher, the topic can be retrieved
 * in O(1) via the .topic member.
 * Note that "topic" needs to be constructed from ZMPL_TOPIC_NAME() as direct
 * pointer comparison is used (not string content comparison).
 */

const struct zmpl_topic* zmpl_get_topic(const char* topic);

#if 0
/* Dynamic advertise/subscribe support */

struct zmpl_subscriber* zmpl_subscribe(struct zmpl_topic* topic);
struct zmpl_publisher* zmpl_advertise(struct zmpl_topic* topic);

void zmpl_poll_event_init(struct zmpl_subscriber* subscribers,
  size_t num_subscribers, struct k_poll_event* events);
#endif

#ifdef __cplusplus
}
#endif

#endif /* __ZMPL_H__ */
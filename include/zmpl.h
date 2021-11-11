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

typedef void (*zmpl_msg_callback)(struct zmpl_subscriber* sub);

struct zmpl_subscriber
{
  const struct zmpl_topic* const topic;
  zmpl_msg_callback callback;
  struct k_msgq queue;  
};

enum zmpl_publish_mode
{
  ZMPL_BLOCKING = 0,
  ZMPL_DROP,
  /* ZMPL_OVERWRITE, */ /* TODO: not supported since we need to manually drop data and is inefficient */
};

struct zmpl_publisher
{
  const struct zmpl_topic* const topic;
  enum zmpl_publish_mode mode;
  struct k_msgq queue; /* Not used yet */
};

/* Section markers for topics */

extern const struct zmpl_topic __start__zmpl_topics[];
extern const struct zmpl_topic __stop__zmpl_topics[];

/* Auxiliary macros */

#define _SLASH_JOIN(component) "/" STRINGIFY(component)
#define _UNDERSCORE_JOIN(component) _CONCAT(_, component)

/* Convert topic from individual components to string joined with '/':
 *   ZMPL_TOPIC_NAME(a,b,c) = "/a/b/c"
 */
#define ZMPL_TOPIC_NAME(topic_components...) FOR_EACH(_SLASH_JOIN,(),topic_components)

/* Convert topic from individual components to token joined by '_':
  *   ZMPL_TOPIC(a,b,c) = a_b_c
  */
#define ZMPL_TOPIC(topic_components...) \
  COND_CODE_0(NUM_VA_ARGS_LESS_1(topic_components), \
    (topic_components), \
    (_CONCAT(GET_ARG_N(1, topic_components), MACRO_MAP_CAT(_UNDERSCORE_JOIN, GET_ARGS_LESS_N(1, topic_components)))))

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
  *   Example: ZMPL_TOPIC_DEFINE(struct battery_state_msg, battery_controller, state)
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
  const struct zmpl_topic ZMPL_TOPIC_IDENTIFIER(topic_components) \
    __used \
    __section("__zmpl_topics") \
    __aligned(__alignof__(struct zmpl_topic)) \
    = \
      { .name = ZMPL_TOPIC_NAME(topic_components), \
        .msg_size = sizeof(msg_type), \
        .subscribers_start = _CONCAT(__start_,ZMPL_SUBSCRIBER_IDENTIFIER(topic_components)), \
        .subscribers_end = _CONCAT(__stop_,ZMPL_SUBSCRIBER_IDENTIFIER(topic_components)), \
      };

/* Static subscriber definition:
 *   Example: ZMPL_SUBSCRIBER_DEFINE(sub1, struct battery_state_msg, 10, callback, ZMPL_TOPIC(battery_controller, state))
 *     Defines a "struct zmpl_subscriber sub1" for topic /battery_controller/state and an outgoig queue of size 10.
 *
 * This macro defines the instance as a read-write struct (since the queue is mutable) and also declares a static
 * buffer for the underlying queue.
 *
 * NOTE: we require that the placement of the subscriber variable be within the required section AND with
 * the underlying's struct alignment. This ensures all subscribers for this topic are placed contiguously
 * in memory within the section (so that we can iterate it as an array within __start/__stop symbols).
 */

#define ZMPL_SUBSCRIBER_DEFINE(subscriber, msg_type, queue_size, cb, topic_components...) \
  extern const struct zmpl_topic ZMPL_TOPIC_IDENTIFIER(topic_components); \
  static msg_type __noinit _CONCAT(__zmpl_subscriber_data_, subscriber)[queue_size]; \
  struct zmpl_subscriber subscriber \
    __used \
    __section(STRINGIFY(ZMPL_SUBSCRIBER_IDENTIFIER(topic_components))) \
    __aligned(__alignof__(struct zmpl_subscriber)) = \
  { \
    .topic = &ZMPL_TOPIC_IDENTIFIER(topic_components), \
    .queue = Z_MSGQ_INITIALIZER(subscriber.queue, (char*)_CONCAT(__zmpl_subscriber_data_, subscriber), sizeof(msg_type), queue_size), \
    .callback = cb \
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
    .mode = ZMPL_BLOCKING \
  };

#define _ZMPL_ADDRESS_OF(var) &var

/* Initializer for static subscriber list of pointers:
 *
 *  Examples: struct zmpl_subscriber* const sub_list[] = { ZMPL_SUBSCRIBER_LIST_INITIALIZER(sub1, sub2, sub3) };
 */

#define ZMPL_SUBSCRIBER_LIST_INITIALIZER(subscribers...) \
  FOR_EACH(_ZMPL_ADDRESS_OF, (,), subscribers)

#define _ZMPL_POLL_EVENT_INIT(sub) K_POLL_EVENT_STATIC_INITIALIZER(K_POLL_TYPE_MSGQ_DATA_AVAILABLE, K_POLL_MODE_NOTIFY_ONLY, &sub.queue, 0)

 /* Initializer for static struct k_poll_event[].
  *
  *  Example:
  *    struct k_poll_event pe[] = { ZMPL_POLL_EVENT_INITIALIZER(sub1, sub2, sub3) };
  */

#define ZMPL_POLL_EVENT_INITIALIZER(subscribers...) \
  FOR_EACH(_ZMPL_POLL_EVENT_INIT, (,), subscribers)

/* Convenience macro which defines both the subscriber list and poll event array
 * directly. This allows only listing subscribers once and is mostly useful when k_poll()
 * is used only on subscribers and nothing else.
 *
 * Example:
 *   ZMPL_SUBSCRIBER_LIST_DEFINE(list, pe, sub1, sub2, sub3);
 *   zmpl_handle_messages(list, pe, ARRAY_SIZE(list));
 */

#define ZMPL_SUBSCRIBER_LIST_DEFINE(list, pe, subscribers...) \
  struct zmpl_subscriber* const list[] = { ZMPL_SUBSCRIBER_LIST_INITIALIZER(subscribers) }; \
  struct k_poll_event pe[] = { ZMPL_POLL_EVENT_INITIALIZER(subscribers) }

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

/* Go through each entry in the subscriber list and
 * invoke the callback for each subscriber (if not NULL) which appears to have
 * pending messages according to its poll event state.
 * It is up to the callback to retrieve the pending message on the queue
 * (or all, or purge it, etc.). Note that if this step blocks, this whole function
 * will block.
 * After each call, the corresponding poll event entry will be reset to the
 * unready state (so that poll can be directly called afterwards).
 */

void zmpl_handle_messages(struct zmpl_subscriber* const sub_list[],
  struct k_poll_event* pe, size_t num_events);


/* Convenience method which invokes k_poll and then zmpl_handle_messages */

int zmpl_spin_once(struct zmpl_subscriber* const sub_list[],
  struct k_poll_event* pe, size_t num_events, k_timeout_t timeout);

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

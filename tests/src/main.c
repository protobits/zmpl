#include <ztest.h>
#include <zmpl.h>

struct msg_type_a
{
  int32_t a;
  uint8_t b[5];
} __aligned(16);

struct msg_type_b
{
  int8_t a;
  uint8_t b;
} __aligned(4);

/* Setup /topic_a and /topic_b, each with its own datatype */

ZMPL_TOPIC_DEFINE(struct msg_type_a, ZMPL_TOPIC(topic_a));
ZMPL_TOPIC_DEFINE(struct msg_type_b, ZMPL_TOPIC(topic_b));

/* Setup two publishers and three subscribers on both topics */

ZMPL_SUBSCRIBER_DEFINE(sub1, struct msg_type_a, 10, ZMPL_TOPIC(topic_a));
ZMPL_SUBSCRIBER_DEFINE(sub2, struct msg_type_a, 10, ZMPL_TOPIC(topic_a));
ZMPL_SUBSCRIBER_DEFINE(sub3, struct msg_type_a, 10, ZMPL_TOPIC(topic_a));

ZMPL_PUBLISHER_DEFINE(pub1, struct msg_type_a, 10, ZMPL_TOPIC(topic_a));
ZMPL_PUBLISHER_DEFINE(pub2, struct msg_type_a, 10, ZMPL_TOPIC(topic_a));

ZMPL_SUBSCRIBER_DEFINE(sub4, struct msg_type_b, 10, ZMPL_TOPIC(topic_b));
ZMPL_SUBSCRIBER_DEFINE(sub5, struct msg_type_b, 10, ZMPL_TOPIC(topic_b));
ZMPL_SUBSCRIBER_DEFINE(sub6, struct msg_type_b, 10, ZMPL_TOPIC(topic_b));

ZMPL_PUBLISHER_DEFINE(pub3, struct msg_type_b, 10, ZMPL_TOPIC(topic_b));
ZMPL_PUBLISHER_DEFINE(pub4, struct msg_type_b, 10, ZMPL_TOPIC(topic_b));

void test_static_pubsub(void)
{
  int i;

  /* Publish 5 times, twice on each topic, so expected sequence
   * on subscribers is 0,0,1,1,2,2,3,3,4,4 */

  for (i = 0; i < 5; i++)
  {
    struct msg_type_a msg_a = { .a = i };
    struct msg_type_b msg_b = { .a = i };

    zmpl_publish(&pub1, &msg_a);
    zmpl_publish(&pub2, &msg_a);

    zmpl_publish(&pub3, &msg_b);
    zmpl_publish(&pub4, &msg_b);
  }

  for (i = 0; i < 10; i++)
  {
    struct msg_type_a msg_a;
    struct msg_type_b msg_b;
    
    zassert_true(k_msgq_get(&sub1.queue, &msg_a, K_NO_WAIT) == 0, "receive sub1");
    zassert_true(msg_a.a == i / 2, "valid message sub1");
    
    zassert_true(k_msgq_get(&sub2.queue, &msg_a, K_NO_WAIT) == 0, "receive sub2");
    zassert_true(msg_a.a == i / 2, "valid message sub2");
    
    zassert_true(k_msgq_get(&sub3.queue, &msg_a, K_NO_WAIT) == 0, "receive sub3");
    zassert_true(msg_a.a == i / 2, "valid message sub3");

    zassert_true(k_msgq_get(&sub4.queue, &msg_b, K_NO_WAIT) == 0, "receive sub4");
    zassert_true(msg_b.a == i / 2, "valid message sub4");
    
    zassert_true(k_msgq_get(&sub5.queue, &msg_b, K_NO_WAIT) == 0, "receive sub5");
    zassert_true(msg_b.a == i / 2, "valid message sub5");
    
    zassert_true(k_msgq_get(&sub6.queue, &msg_b, K_NO_WAIT) == 0, "receive sub6");
    zassert_true(msg_b.a == i / 2, "valid message sub6");
  }

  struct msg_type_a msg_a;
  zassert_true(k_msgq_get(&sub1.queue, &msg_a, K_NO_WAIT) == -ENOMSG, "no more receive sub1");
  zassert_true(k_msgq_get(&sub2.queue, &msg_a, K_NO_WAIT) == -ENOMSG, "no more receive sub2");
  zassert_true(k_msgq_get(&sub3.queue, &msg_a, K_NO_WAIT) == -ENOMSG, "no more receive sub3");  

  struct msg_type_b msg_b;
  zassert_true(k_msgq_get(&sub4.queue, &msg_b, K_NO_WAIT) == -ENOMSG, "no more receive sub4");
  zassert_true(k_msgq_get(&sub5.queue, &msg_b, K_NO_WAIT) == -ENOMSG, "no more receive sub5");
  zassert_true(k_msgq_get(&sub6.queue, &msg_b, K_NO_WAIT) == -ENOMSG, "no more receive sub6");  
}

void static_poll_thread(void* arg1, void* arg2, void* arg3)
{
  int i, j;
  struct k_poll_event pe[] = { ZMPL_POLL_EVENT_INITIALIZER(sub1, sub2, sub3) };
  struct msg_type_a prev_msg[3];

  struct zmpl_subscriber* subs[3] = { &sub1, &sub2, &sub3 };

  for (i = 0; i < 3; i++)
  {
    prev_msg[i].a = -1;
  }

  /* Do poll */
  while (k_poll(pe, ARRAY_SIZE(pe), K_FOREVER) == 0)
  {
    struct msg_type_a msg_a;

    /* Receive message */

    for (j = 0; j < 3; j++)
    {
      zassert_true(k_msgq_get(&(subs[j]->queue), &msg_a, K_NO_WAIT) == 0, "receive");
      zassert_true(msg_a.a == prev_msg[j].a + 1, "valid message");        
      
      prev_msg[j] = msg_a;
    }
      
    /* reset poll event */
    for (j = 0; j < ARRAY_SIZE(pe); j++)
    {
      pe[j].state = K_POLL_STATE_NOT_READY;
    }

    /* check if we received everything */

    bool all_done = true;
    for (i = 0; i < 3; i++)
    {
      if (prev_msg[i].a != 4)
      {
        all_done = false;
      }
    }

    if (all_done)
    {
      break;
    }
  }

  /* Verify all messages received */

  zassert_true(k_poll(pe, ARRAY_SIZE(pe), K_NO_WAIT) == -EAGAIN, "no pending poll");
  
  for (i = 0; i < 3; i++)
  {
    zassert_true(k_msgq_num_used_get(&(subs[i]->queue)) == 0, "no pending");
  }
}

/* Start receiving thread */
K_THREAD_DEFINE(tid, 2048, static_poll_thread, NULL, NULL, NULL, 5, 0, 0);

void test_static_poll(void)
{
  int i = 0;
  
  /* Publish messages */
  
  for (i = 0; i < 5; i++)
  {
    struct msg_type_a msg_a = { .a = i };
    zmpl_publish(&pub1, &msg_a);
  }

  /* Wait for receiving thread to complete */
  k_thread_join(tid, K_FOREVER);
}

void test_get_topic(void)
{
  zassert_true(zmpl_get_topic(ZMPL_TOPIC_NAME(topic_a)) == sub1.topic, "get_topic");
  zassert_true(zmpl_get_topic(ZMPL_TOPIC_NAME(topic_b)) == sub4.topic, "get_topic");
}

void test_main(void)
{
  ztest_test_suite(zmpl,
    ztest_unit_test(test_static_pubsub),
    ztest_unit_test(test_static_poll),
    ztest_unit_test(test_get_topic)
  );
  ztest_run_test_suite(zmpl);
}
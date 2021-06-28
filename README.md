# zmpl: message passing library for Zephyr RTOS

This is a Zephyr RTOS module containing a very minimalistic publish-subscribe framework.
The main idea is to have a series of "topics", subscribers and publishers. A subscriber
*subscribes* to a topic, expecting to receive messages sent by publishers who *advertise*
on that topic. It supports many-to-many connections as each subscriber has its own message
queue.

Currently all topic definition, subscription and advertisement is done statically via a series
of macros which (via some compiler section and linker script magic) allow to define a series of
static arrays holding topic definitions, subscribers and advertisers, even from different files.

The runtime API for publishing is quite simple and efficient: for each subscriber on the topic
corresponding to the given advertiser, a message is added to the message queue. To receive messages,
the subscriber's message queue can be used either directly or (more commonly) handled using Zephyr's
polling API (his framework includes a series of utility functions to simplify creation of the poll
event struct from a static list of subscribers).

Messages themselves are simply any possible C type. This framework does not care about the contents
of the message nor it imposes any restriction (besides the alignment requirement from Zephyr's message
queue itself).

# Usage

For example usage please have a look inside tests/ directory. I will eventually add some proper samples.
Documentation is mostly in `zmpl.h` file. Also look at `prj.conf` to see which options are required.

## Limitations/potential improvements

- Dynamic subscription/advertisement: this would add support for defining topics and creating
  subscribers/publishers during runtime (in parallel to the statically defined ones). This is not 
  prioritary since I don't see a real need for this at the moment.
- Lossy vs. lossless / blocking vs. non-blocking: currently the publish operation can be either
  lossless (and blocking), which means that any subscriber's queue which is full will be waited on
  during a publish operation until space becomes available to add the message, or lossy (non-blocking),
  which means that if any subscriber's queue is full it is simply skipped (thus, publish operation is
  non blocking). An intermediate alternative would be to make the publish operation non-blocking and
  discard the oldest message in subscriber's queue when publishing (similar to how Robot Operating System)
  handles this. Zephyr's message queue API does not support this directly so the available options to do
  this are not ideal. Similarly, an outgoing queue can be added to publishers so that the publish
  operation is non blocking but at the same time a worker thread is used to move messages from this queue
  to the subscriber's queues (this would still require message discarding otherwise the worker could
  still hang waiting for space and the publisher also hang waiting for the worker).
- If you define a topic you need to define at least one subscriber for it: this is needed since a topic definition
  requires to be mapped during link time to the internal static array of subscribers. If there are no subscribers,
  you will get linker errors such as "undefined reference to __start___zmpl_subscribers_<topic>". 
 
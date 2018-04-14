#include "eth.h"
#include "log.h"
#include "stats.h"
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/module.h> /* Needed by all modules */
#include <linux/netdevice.h>
#include <linux/time.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Paulo Coelho");
MODULE_VERSION("1.0");
MODULE_DESCRIPTION("Client with outstanding messages in kernel space.");

#define PAXOS_ETH_TYPE 0xcafe
const char* MOD_NAME = "KCLIENT";

static struct net_device* dev = NULL;
static struct stats*      st = NULL;

static char* if_name = "enp4s0";
module_param(if_name, charp, 0000);

static char* dst_addr = "00:24:81:b4:1c:49";
module_param(dst_addr, charp, 0000);

static int nclients = 1;
module_param(nclients, int, 0000);

static struct timer_list stats_ev;
static struct timeval    stats_interval;

struct my_msg
{
  char msg[32];
  int  id;
};

struct client
{
  int            id;
  struct timeval tv;
  long           count;
};

static struct client*     cl = NULL;
static unsigned long long total_count = 0;

static void
rcv_paxos_msg(struct net_device* dev, uint8_t src_addr[ETH_ALEN], char* rmsg,
              size_t len, void* arg)
{
  static int     rcv = 0, i;
  struct my_msg* msg = (struct my_msg*)rmsg;
  struct client* c = &cl[msg->id];

  if (rcv == 0)
    st = stats_new();

  rcv++;
  if (rcv % 20000 == 0) {
    stats_add(st, rcv);
    LOG_DEBUG("got message '%s' with %zu bytes.", msg->msg, len);
  }

  do_gettimeofday(&(c->tv));
  ++(c->count);
  total_count++;

  for (i = 0; i < nclients; ++i) {
    if (i == msg->id || delta_t(&(cl[i].tv), &(c->tv)) > 500000) {
      msg->id = i;
      eth_send(dev, src_addr, PAXOS_ETH_TYPE, (const char*)msg, len);
    }
  }
}

static void
on_stats(unsigned long arg)
{
  printk(KERN_INFO "Client: %llu value/sec\n", total_count);
  total_count = 0;
  mod_timer(&stats_ev, jiffies + timeval_to_jiffies(&stats_interval));
}

int
run(void)
{
  int           i;
  uint8_t       daddr[ETH_ALEN];
  struct my_msg msg = { .msg = "HELLO" };

  if (!str_to_mac(dst_addr, daddr)) {
    LOG_ERROR("invalid destination address: '%s'.", dst_addr);
    return 1;
  }

  eth_listen(dev, PAXOS_ETH_TYPE, rcv_paxos_msg, NULL);

  cl = kmalloc(nclients * sizeof(struct client), GFP_ATOMIC);

  setup_timer(&stats_ev, on_stats, 0);
  stats_interval = (struct timeval){ 1, 0 };
  mod_timer(&stats_ev, jiffies + timeval_to_jiffies(&stats_interval));

  for (i = 0; i < nclients; ++i) {
    cl[i].id = i;
    cl[i].count = 0;
    do_gettimeofday(&cl[i].tv);
    msg.id = i;
    // LOG_INFO("client %d sending msgs to %02x:%02x:%02x:%02x:%02x:%02x", i,
    //          daddr[0], daddr[1], daddr[2], daddr[3], daddr[4], daddr[5]);
    eth_send(dev, daddr, PAXOS_ETH_TYPE, (void*)&msg, sizeof(msg));
  }

  return 0;
}

static int __init
           init_client(void)
{
  dev = eth_init(if_name);
  if (dev) {
    LOG_INFO("Interface %s has MAC address %02x:%02x:%02x:%02x:%02x:%02x",
             dev->name, dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2],
             dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5]);

    run();

    return 0;
  }

  LOG_ERROR("Interface not found: %s.", if_name);
  return 0;
}

static void __exit
            client_exit(void)
{
  eth_destroy(dev);
  del_timer(&stats_ev);
  if (st) {
    stats_save(st, NULL, 0);
    stats_destroy(st);
  }
  if (cl)
    kfree(cl);
  LOG_INFO("unloading module.");
}

module_init(init_client);
module_exit(client_exit);

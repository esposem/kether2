#include "eth.h"
#include "log.h"
#include "stats.h"
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/module.h> /* Needed by all modules */
#include <linux/netdevice.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Paulo Coelho - Emanuele Giuseppe Esposito");
MODULE_VERSION("1.0");
MODULE_DESCRIPTION("Client with outstanding messages in kernel space.");

const char* MOD_NAME = "KCLIENT";

static struct net_device* dev = NULL;
// static struct stats*      st = NULL;

static char* if_name = "enp0s3";
module_param(if_name, charp, 0000);

static char* dst_addr = "02:78:11:2a:a4:64";
module_param(dst_addr, charp, 0000);

static int nclients = 1;
module_param(nclients, int, 0000);

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

static struct client* cl = NULL;
#ifndef PRINT
static struct timer_list  stats_ev;
static struct timeval     stats_interval;
static unsigned long long total_count = 0;
#endif

static void
rcv_paxos_msg(struct net_device* dev, uint8_t src_addr[ETH_ALEN], char* rmsg,
              size_t len)
{
  struct my_msg* msg = (struct my_msg*)rmsg;

#ifndef PRINT
  struct client* c = &cl[msg->id];
  do_gettimeofday(&(c->tv));
  ++(c->count);
  total_count++;
  for (int i = 0; i < nclients; ++i) {
    if (i == msg->id || delta_t(&(cl[i].tv), &(c->tv)) > 500000) {
      msg->id = i;
      eth_send(dev, src_addr, PAXOS_ETH_TYPE, (const char*)msg, len);
    }
  }
#else
  LOG_DEBUG("cl %d got message '%s' with %zu bytes.", msg->id, msg->msg, len);
#endif
}

#ifndef PRINT
static void
on_stats(unsigned long arg)
{
  printk(KERN_INFO "Client: %llu value/sec\n", total_count);
  total_count = 0;
  mod_timer(&stats_ev, jiffies + timeval_to_jiffies(&stats_interval));
}
#endif

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

  if (eth_listen(dev, PAXOS_ETH_OK, rcv_paxos_msg) > 0) {

    cl = kmalloc(nclients * sizeof(struct client), GFP_ATOMIC);
    for (i = 0; i < nclients; ++i) {
      cl[i].id = i;
      cl[i].count = 0;
      do_gettimeofday(&cl[i].tv);
      msg.id = i;
#ifdef PRINT
      LOG_INFO("client %d sending msgs %s %zu to %02x:%02x:%02x:%02x:%02x:%02x",
               i, msg.msg, sizeof(msg), daddr[0], daddr[1], daddr[2], daddr[3],
               daddr[4], daddr[5]);
#endif
      eth_send(dev, daddr, PAXOS_ETH_TYPE, (char*)&msg, sizeof(msg));
    }

#ifndef PRINT
    setup_timer(&stats_ev, on_stats, 0);
    stats_interval = (struct timeval){ 1, 0 };
    mod_timer(&stats_ev, jiffies + timeval_to_jiffies(&stats_interval));
#endif

  } else {
    LOG_ERROR("error while setting up interface %s", dev->name);
    return 1;
  }

  return 0;
}

int
init_module(void)
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

void
cleanup_module(void)
{
  eth_destroy(dev);
#ifndef PRINT
  del_timer(&stats_ev);
#endif
  if (cl)
    kfree(cl);
  LOG_INFO("unloading module.");
}

// module_init(init_module);
// module_exit(cleanup_module);

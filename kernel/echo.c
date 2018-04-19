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
MODULE_DESCRIPTION("Raw sockets in kernel space.");

#define PAXOS_ETH_TYPE 0xcafe
const char* MOD_NAME = "KECHO";

static struct net_device* dev;
// static struct stats*      st = NULL;

static char* if_name = "enp0s3";
module_param(if_name, charp, 0000);

struct my_msg
{
  char msg[32];
  int  id;
};

static void
rcv_paxos_msg(struct net_device* dev, uint8_t src_addr[ETH_ALEN], char* rmsg,
              size_t len)
{
#ifdef PRINT
  struct my_msg* msg = (struct my_msg*)rmsg;
  LOG_INFO("Received id %d mess %s. Sending it back", msg->id, msg->msg);
#endif
  eth_send(dev, src_addr, PAXOS_ETH_OK, rmsg, len);
}

static int __init
           init_echo(void)
{
  dev = eth_init(if_name);
  if (dev) {
    LOG_INFO("Interface %s has MAC address %02x:%02x:%02x:%02x:%02x:%02x",
             dev->name, dev->dev_addr[0], dev->dev_addr[1], dev->dev_addr[2],
             dev->dev_addr[3], dev->dev_addr[4], dev->dev_addr[5]);

    if (eth_listen(dev, PAXOS_ETH_TYPE, rcv_paxos_msg) == 0) {
      LOG_ERROR("error while setting up interface %s", dev->name);
      return 0;
    }
    return 0;
  }

  LOG_ERROR("Interface not found: %s.", if_name);
  return 1;
}

static void __exit
            cleanup_echo(void)
{
  eth_destroy(dev);
  LOG_INFO("unloading module.");
}

module_init(init_echo);
module_exit(cleanup_echo);

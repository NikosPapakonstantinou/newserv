#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <time.h>

#include "../nick/nick.h"
#include "../localuser/localuserchannel.h"
#include "../core/hooks.h"
#include "../core/schedule.h"
#include "../lib/array.h"
#include "../lib/base64.h"
#include "../lib/irc_string.h"
#include "../lib/splitline.h"
#include "../control/control.h"
#include "../lib/version.h"

MODULE_VERSION("")

int nc_cmd_dumptree(void *source, int cargc, char **cargv);
int nc_cmd_usercount(void *source, int cargc, char **cargv);

void _init() {
  registercontrolhelpcmd("dumptree", NO_DEVELOPER, 2, &nc_cmd_dumptree, 
                                  "Dumps diaganostic information on the patricia trie structure\n"
                                  "Usage: dumptree <ipv4|ipv6|cidr4|cidr6> [int]\n" 
                                  "Nodes with prefixies Only:\n"
                                  "No arguments - default prints: ptr, ip\n"
                                  "1: ptr, prefixptr, bitlen, refcount, ip\n"
                                  "2: ptr, bit, usercount, ip\n"
                                  "3: ptr, leftptr, rightptr, parentptr\n"
                                  "4: ptr, ext0, ext1, ext2, ext3, ext4\n"
                                  "All Notes (inc no prefixies):\n"
                                  "10: ptr, prefixptr, ip\n"
                                  "11: ptr, prefixbitlen, refcount,ip\n"
                                  "12: ptr, bitlen, usercount, ip\n"
                                  "13: ptr, leftptr, rightptr, parentptr\n"
                                  "14: ptr, ext0, ext1, ext2, ext3, ext4");
  registercontrolhelpcmd("usercount", NO_OPER, 1, &nc_cmd_usercount, "Usage: usercount <ip|cidr>\nDisplays number of users on a given ipv4/6 or cidr4/6");
}

void _fini() {
  deregistercontrolcmd("dumptree", &nc_cmd_dumptree);
  deregistercontrolcmd("usercount", &nc_cmd_usercount);
}

int nc_cmd_dumptree(void *source, int cargc, char **cargv) {
  nick *np=(nick *)source; 
  struct irc_in_addr sin;
  unsigned char bits;
  patricia_node_t *head, *node;
  unsigned int level=0;
  int i = 0;
 
  if (cargc < 1) {
    return CMD_USAGE;
  }

  if (ipmask_parse(cargv[0], &sin, &bits) == 0) {
    controlreply(np, "Invalid mask.");
    return CMD_OK;
  }

  if (cargc>1) {
    level=strtoul(cargv[1],NULL,10);
  }

  head = refnode(iptree, &sin, bits);

  if (level < 10) { 
    PATRICIA_WALK(head, node)
    {
      switch (level) {
        case 0:
          controlreply(np,"%p: %s", node, IPtostr(node->prefix->sin)); 
          break;
        case 1:
          controlreply(np,"%p: prefix %p, bit %d, ref_count %d, IP: %s",node, node->prefix, 
                           node->prefix->bitlen, node->prefix->ref_count, IPtostr(node->prefix->sin));
          break;
        case 2:
          controlreply(np,"%p: bit: %d, usercount: %d, IP: %s", node, node->bit, node->usercount, IPtostr(node->prefix->sin));
          break;
        case 3: 
          controlreply(np,"%p: L: %p, R: %p P: %p", node, node->l, node->r, node->parent);
          break;
        case 4: 
          controlreply(np,"%p: 0: %p, 1: %p, 2: %p, 3: %p, 4: %p", node, 
                          node->exts[0],  node->exts[1],  node->exts[2],  node->exts[3],  node->exts[4]);
          break; 
        default:
          if( i == 0 ) controlreply(np,"Invalid Level");
      }
      if ( i++ > 500) {
        controlreply(np,"too many... aborting...");
        break;
      }
    }
    PATRICIA_WALK_END;
  } else {
    PATRICIA_WALK_ALL(head, node)
    {
      switch (level) {
        case 10:
          controlreply(np,"%p: prefix: %p %s", node, node->prefix, node->prefix?IPtostr(node->prefix->sin):"");
          break;
        case 11:
          if(node->prefix) 
            controlreply(np,"%p: prefix bit: %d, ref_count %d, IP: %s",node,
                           node->prefix->bitlen, node->prefix->ref_count, IPtostr(node->prefix->sin));
          else
            controlreply(np,"%p: --", node);
          break;
        case 12:
          controlreply(np,"%p: bit: %d, usercount: %d, IP: %s", node, node->bit, node->usercount, node->prefix?IPtostr(node->prefix->sin):"");
          break;
        case 13:
          controlreply(np,"%p: L: %p, R: %p P: %p", node, node->l, node->r, node->parent);
          break;
        case 14:
          controlreply(np,"%p%s 0: %p, 1: %p, 2: %p, 3: %p, 4: %p", node, node->prefix?"-":":",
                          node->exts[0],  node->exts[1],  node->exts[2],  node->exts[3],  node->exts[4]);
          break;
        default:
          if ( i == 0 ) controlreply(np,"Invalid Level");
      }
      if ( i++ > 500) {
        controlreply(np,"too many... aborting...");
        break;
      }
    }
    PATRICIA_WALK_END;
  }
  derefnode(iptree, head);
  return CMD_OK;
}

int nc_cmd_usercount(void *source, int cargc, char **cargv) {
  nick *np = (nick *)source;
  struct irc_in_addr sin;
  unsigned char bits;
  patricia_node_t *head;
  int count;

  if (cargc < 1) {
    return CMD_USAGE;
  }

  if (ipmask_parse(cargv[0], &sin, &bits) == 0) {
    controlreply(np, "Invalid mask.");

    return CMD_OK;
  }

  head = refnode(iptree, &sin, bits);

  count = head->usercount;

  derefnode(iptree, head);

  controlreply(np, "%d user(s) found.", count);

  return CMD_OK;
}


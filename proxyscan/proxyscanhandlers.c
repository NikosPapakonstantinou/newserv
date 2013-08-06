#include <stdio.h>
#include "proxyscan.h"
#include "../irc/irc.h"
#include "../lib/irc_string.h"
#include "../core/error.h"
#include "../glines/glines.h"

void proxyscan_newnick(int hooknum, void *arg) {
  nick *np=(nick *)arg;
  cachehost *chp;
  foundproxy *fpp, *nfpp;
  extrascan *esp, *espp;
  char reason[200];

  int i;

  /* Skip 127.* and 0.* hosts */
  if (irc_in_addr_is_loopback(&np->p_ipaddr))
    return;

  /* slug: why is this here? why isn't it with the other queuing stuff? */
  /* we're given a list of ip/subnets and port pairs which someone else has
     seen a proxy on in the past, so we scan these very aggressively
     (even ignoring the cache)
   */
  /* disabled as the list is hopelessly out of date */
  if ((esp=findextrascan(np->ipnode))) {
    Error("proxyextra", ERR_ERROR, "connection from possible proxy %s", IPtostr(np->p_ipaddr)); 
    for (espp=esp;espp;espp=espp->nextbynode) { 
      /* we force a scan on any hosts that may be an open proxy, even if they are:
       * a) already in the queue, b) we've been running < 120 seconds */
      queuescan(np->ipnode, espp->type, espp->port, SCLASS_NORMAL, time(NULL));
    }
  }

/* slug: this BREAKS all of P's design assumptions, do NOT REENABLE THIS UNDER ANY CIRCUMSTANCES */
/* ignore newnick until initial burst complete */
/*  if (!ps_ready)
    return;
*/

  /*
   * Logic for connecting hosts:
   *
   * If they're in the cache and clean, return.
   * If they're in the cache, dirty, and last scanned < 30 
   *   mins ago, return (they will probably go away in a minute)
   * If they're in the cache and dirty:
   *  - gline them
   *  - trigger the "check" scans on the known proxies
   *  - trigger normal scans as for the case below..
   *
   * If they're not in the cache, we queue up their scans
   */
  if ((chp=findcachehost(np->ipnode))) {
    if (!chp->proxies)
      return;

    if (time(NULL) < (chp->lastscan + 1800))
      return;

    /* Queue up all the normal scans - on the normal queue */
    for (i=0;i<numscans;i++) {
      /* If this port is open DON'T queue the scan - we'll start it later in the CHECK class */
      for (fpp=chp->proxies;fpp;fpp=fpp->next) {
        if (fpp->type == thescans[i].type && fpp->port == thescans[i].port)
          break;
      
      if (!fpp)
        queuescan(np->ipnode, thescans[i].type, thescans[i].port, SCLASS_NORMAL, 0);
      }
    }

    /* We want these scans to start around now, so we put them at the front of the priority queue */
    for (fpp=chp->proxies;fpp;fpp=nfpp) {
      nfpp=fpp->next;
      queuescan(np->ipnode, fpp->type, fpp->port, SCLASS_CHECK, time(NULL));
      freefoundproxy(fpp);
    }

    /* set a SHORT gline - if they really have an open proxy the gline will be re-set, with a new ID */
    snprintf(reason, sizeof(reason), "Open Proxy, see http://www.quakenet.org/openproxies.html - ID: %d", chp->glineid);
    glinebynick(np, 600, reason, GLINE_IGNORE_TRUST, "proxyscan");

    chp->lastscan=time(NULL);
    chp->proxies=NULL;
    chp->glineid=0;
  } else {
    chp=addcleanhost(time(NULL));
    np->ipnode->exts[ps_cache_ext] = chp;
    patricia_ref_prefix(np->ipnode->prefix);

    /* Queue up all the normal scans - on the normal queue */
    for (i=0;i<numscans;i++)
      queuescan(np->ipnode, thescans[i].type, thescans[i].port, SCLASS_NORMAL, 0);
  }
}

#include "proxyscan.h"
#include "../irc/irc.h"
#include "../lib/irc_string.h"

void proxyscan_newnick(int hooknum, void *arg) {
  nick *np=(nick *)arg;
  cachehost *chp;
  foundproxy *fpp, *nfpp;
  int i;

  /* Skip 127.* and 0.* hosts */
  if ((np->ipaddress>>24==0) || (np->ipaddress>>24==127))
    return;

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

  if ((chp=findcachehost(np->ipaddress))) {
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
        queuescan(np->ipaddress, thescans[i].type, thescans[i].port, SCLASS_NORMAL, 0);
      }
    }

    /* We want these scans to start around now, so we put them at the front of the priority queue */
    for (fpp=chp->proxies;fpp;fpp=nfpp) {
      nfpp=fpp->next;
      queuescan(np->ipaddress, fpp->type, fpp->port, SCLASS_CHECK, time(NULL));
      freefoundproxy(fpp);
    }

    /* set a SHORT gline - if they really have an open proxy the gline will be re-set, with a new ID */
    irc_send("%s GL * +*@%s 600 :Open Proxy, see http://www.quakenet.org/openproxies.html - ID: %d",
	     mynumeric->content,IPtostr(np->ipaddress),chp->glineid);

    chp->lastscan=time(NULL);
    chp->proxies=NULL;
    chp->glineid=0;
  } else {
    chp=addcleanhost(np->ipaddress, time(NULL));

    /* Queue up all the normal scans - on the normal queue */
    for (i=0;i<numscans;i++)
      queuescan(np->ipaddress, thescans[i].type, thescans[i].port, SCLASS_NORMAL, 0);
  }
}


/* proxyscanalloc.c */

#include "proxyscan.h"
#include "../core/nsmalloc.h"

#include <stdlib.h>

#define ALLOCUNIT 1024

scan *freescans;
cachehost *freecachehosts;
pendingscan *freependingscans;
foundproxy *freefoundproxies;
extrascan *freeextrascans;

scan *getscan() {
  int i;
  scan *sp;
  
  if (freescans==NULL) {
    /* Eep.  Allocate more. */
    freescans=(scan *)nsmalloc(POOL_PROXYSCAN,ALLOCUNIT*sizeof(scan));
    for (i=0;i<(ALLOCUNIT-1);i++) {
      freescans[i].next=&(freescans[i+1]);
    }
    freescans[ALLOCUNIT-1].next=NULL;
  }
  
  sp=freescans;
  freescans=sp->next;
  
  return sp;
}

void freescan(scan *sp) {
  sp->next=freescans;
  freescans=sp;
}

cachehost *getcachehost() {
  int i;
  cachehost *chp;
  
  if (freecachehosts==NULL) {
    freecachehosts=(cachehost *)nsmalloc(POOL_PROXYSCAN,ALLOCUNIT*sizeof(cachehost));
    for (i=0;i<(ALLOCUNIT-1);i++) {
      freecachehosts[i].next=&(freecachehosts[i+1]);
    }
    freecachehosts[ALLOCUNIT-1].next=NULL;
  }
  
  chp=freecachehosts;
  freecachehosts=chp->next;
  
  return chp;
}

void freecachehost(cachehost *chp) {
  chp->next=freecachehosts;
  freecachehosts=chp;
} 

pendingscan *getpendingscan() {
  int i;
  pendingscan *psp;

  if (!freependingscans) {
    freependingscans=(pendingscan *)nsmalloc(POOL_PROXYSCAN,ALLOCUNIT * sizeof(pendingscan));
    for (i=0;i<(ALLOCUNIT-1);i++)
      freependingscans[i].next = (pendingscan *)&(freependingscans[i+1]);
    freependingscans[ALLOCUNIT-1].next=NULL;
  }

  psp=freependingscans;
  freependingscans=(pendingscan *)psp->next;

  return psp;
}

void freependingscan(pendingscan *psp) {
  psp->next=freependingscans;
  freependingscans=psp;
}

foundproxy *getfoundproxy() {
  int i;
  foundproxy *fpp;

  if (!freefoundproxies) {
    freefoundproxies=(foundproxy *)nsmalloc(POOL_PROXYSCAN,ALLOCUNIT * sizeof(foundproxy));
    for (i=0;i<(ALLOCUNIT-1);i++)
      freefoundproxies[i].next = (foundproxy *)&(freefoundproxies[i+1]);
    freefoundproxies[ALLOCUNIT-1].next=NULL;
  }

  fpp=freefoundproxies;
  freefoundproxies=(foundproxy *)fpp->next;

  return fpp;
}

void freefoundproxy(foundproxy *fpp) {
  fpp->next=freefoundproxies;
  freefoundproxies=fpp;
}

extrascan *getextrascan() {
  int i;
  extrascan *esp;

  if (freeextrascans==NULL) {
    freeextrascans=(extrascan *)nsmalloc(POOL_PROXYSCAN,ALLOCUNIT*sizeof(extrascan));
    for (i=0;i<(ALLOCUNIT-1);i++) {
      freeextrascans[i].next=&(freeextrascans[i+1]);
    }
    freeextrascans[ALLOCUNIT-1].next=NULL;
  }

  esp=freeextrascans;
  freeextrascans=esp->next;

  return esp;
}

void freeextrascan(extrascan *esp) {
  esp->next=freeextrascans;
  freeextrascans=esp;
}


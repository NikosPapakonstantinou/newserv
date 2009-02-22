#include "../control/control.h"
#include "../nick/nick.h"
#include "../channel/channel.h"
#include "../lib/version.h"

MODULE_VERSION("");

int ho_horse(void *source, int cargc, char **cargv) {
  nick *sender=(nick *)source;
  nick *victim;
  channel *cp;
  
  if (cargc<1)
    return CMD_USAGE;
  
  if ((victim=getnickbynick(cargv[0]))!=NULL) {
    controlreply(victim,"       _ ___,;;;^");
    controlreply(victim,"    ,;( )__, )~\\|");
    controlreply(victim,"   ;; //   '--;");
    controlreply(victim,"   '  ;\\    |");
    controlreply(sender,"Gave %s a horse.",victim->nick); 
  } else if ((cp=findchannel(cargv[0]))!=NULL) {
    controlchanmsg(cp,"       _ ___,;;;^");
    controlchanmsg(cp,"    ,;( )__, )~\\|");
    controlchanmsg(cp,"   ;; //   '--;");
    controlchanmsg(cp,"   '  ;\\    |");    
    controlreply(sender,"Spammed horse in %s.",cp->index->name->content); 
  } else {
    controlreply(sender,"Can't find %s.",cargv[0]);
  }

  return CMD_OK;  
}

void _init() {
  registercontrolhelpcmd("horse",NO_OPERED,2,ho_horse,"Usage: horse <target>\nSpams a horse at target.");
}

void _fini() {
  deregistercontrolcmd("horse",ho_horse);
}


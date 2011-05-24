#include "perms.h"

#include <stdio.h>
#include <unistd.h>

void
perm_print(void)
{
  printf("User id : %i, group : %i\n", getuid(), getgid());
  printf("Effective user id : %i, group : %i\n", geteuid(), getegid());
}

int
perm_root_check(void)
{
  return (geteuid() == 0);
}

void
perm_drop(void) {
#ifdef USER_ID
  seteuid(USER_ID);
#else
  seteuid(getuid());
#endif /* USER_ID */

#ifdef GROUP_ID
  setegid(GROUP_ID);
#else
  setegid(getgid());
#endif /* GROUP_ID */
  printf("Permissions dropped\n");
}


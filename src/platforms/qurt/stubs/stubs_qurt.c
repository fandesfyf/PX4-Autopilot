#include <dlfcn.h>
#include <qurt_log.h>
#include <dspal_platform.h>

#ifdef CONFIG_ARCH_BOARD_EAGLE

void HAP_debug(const char *msg, int level, const char *filename, int line)
{
}

void HAP_power_request(int a, int b, int c)
{
}

int dlinit(int a, char **b)
{
	return 1;
}
#endif

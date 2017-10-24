#include <getopt.h>
#include <stdio.h>
#include <xtables.h>
#include <linux/netfilter/xt_cgroup.h>

static void cgroup_help(void)
{
	printf(
"cgroup match options:\n"
"[!] --cgroup fwid  Match cgroup fwid\n");
}

static const struct option cgroup_opts[] = {
	{ "cgroup", 1, NULL, 'c' },
	{ .name = NULL }
};

static int
cgroup_parse(int c, char **argv, int invert, unsigned int *flags,
      const void *entry,
      struct xt_entry_match **target)
{
	struct xt_cgroup_info *cgroupinfo
		= (struct xt_cgroup_info *)(*target)->data;

	switch (c) {
	case 'c': /* TODO 1 or 0 */
		/* use optarg, due libopt is used */
		if (sscanf(optarg, "%u", &cgroupinfo->id) != 1)
			return 1;

		cgroupinfo->invert = invert;
		*flags = 1;
		break;

	default:
		return 0;
	}

	return 1;
}

static void
cgroup_print(const void *ip, const struct xt_entry_match *match, int numeric)
{
	const struct xt_cgroup_info *info = (void *) match->data;

	printf(" cgroup %s%u", info->invert ? "! ":"", info->id);
}

static void cgroup_save(const void *ip, const struct xt_entry_match *match)
{
	const struct xt_cgroup_info *info = (void *) match->data;

	printf("%s --cgroup %u", info->invert ? " !" : "", info->id);
}

static struct xtables_match cgroup_match = {
	.family		= NFPROTO_UNSPEC,
	.name		= "cgroup",
	.version	= XTABLES_VERSION,
	.size		= XT_ALIGN(sizeof(struct xt_cgroup_info)),
	.userspacesize	= XT_ALIGN(sizeof(struct xt_cgroup_info)),
	.help		= cgroup_help,
	.print		= cgroup_print,
	.save		= cgroup_save,
	.parse		= cgroup_parse,
	.extra_opts     = cgroup_opts,
};

void _init(void)
{
	xtables_register_match(&cgroup_match);
}

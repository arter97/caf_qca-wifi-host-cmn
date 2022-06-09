/*
 * Copyright (c) 2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021-2022 Qualcomm Innovation Center, Inc. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

#include <qcatools_lib.h>
#include <ieee80211_external.h>
#include <wlan_stats_define.h>
#include <stats_lib.h>
#if UMAC_SUPPORT_CFG80211
#ifndef BUILD_X86
#include <netlink/genl/genl.h>
#endif
#endif

#define FL "%s(): %d:"

#define STATS_ERR(fmt, args...) \
	fprintf(stderr, "stats_lib:E:" FL ": "fmt, __func__, __LINE__, ## args)
#define STATS_WARN(fmt, args...) \
	fprintf(stdout, "stats_lib:W:" FL ": "fmt, __func__, __LINE__, ## args)
#define STATS_MSG(fmt, args...) \
	fprintf(stdout, "stats_lib:M:" FL ": "fmt, __func__, __LINE__, ## args)
#define STATS_INFO(fmt, args...) \
	fprintf(stdout, "stats_lib:I:" FL ": "fmt, __func__, __LINE__, ## args)

/* Expected MAX STAs per VAP */
#define EXPECTED_MAX_STAS_PERVAP       (50)
/* MAX NL buffer size of station list */
#define LIST_STATION_CFG_ALLOC_SIZE    (3500)
/* MAX Radio count */
#define MAX_RADIO_NUM                3
/* MAX SoC count expected same as Radio count */
#define MAX_SOC_NUM                  3
/* MAX Possible VAP count based on radio count */
#define MAX_VAP_NUM                  (17 * MAX_RADIO_NUM)
/* This path is for network interfaces */
#define PATH_SYSNET_DEV              "/sys/class/net/"
/* This path is created if phy is brought up in cfg80211 mode */
#define CFG80211_MODE_FILE_PATH      "/sys/class/net/wifi%d/phy80211/"
/* Radio interface name size */
#define RADIO_IFNAME_SIZE            5

/* NL80211 command and event socket IDs */
#define STATS_NL80211_CMD_SOCK_ID    DEFAULT_NL80211_CMD_SOCK_ID
#define STATS_NL80211_EVENT_SOCK_ID  DEFAULT_NL80211_EVENT_SOCK_ID

#define SET_FLAG(_flg, _mask)              \
	do {                               \
		if (!(_flg))               \
			(_flg) = (_mask);  \
		else                       \
			(_flg) &= (_mask); \
	} while (0)

/* Global socket context to create nl80211 command and event interface */
static struct socket_context g_sock_ctx = {0};
/* Global parent vap to build child sta object list */
static struct object_list *g_parent_vap_obj;
/**
 * Global sta object pointor to avoid sta list traversal to find current
 * sta pointer while building sta object list.
 */
static struct object_list *g_curr_sta_obj;

/**
 * struct soc_ifnames: Radio interface of a soc
 * @ifname: Radio Interface name
 */
struct soc_ifnames {
	char ifname[IFNAME_LEN];
};

/**
 * struct interface: Each interface details
 * @added: flag to indicate that the interface is added to object list
 * @active: flag to indicate that the interface is up and running
 * @name: Name of the interface
 * @hw_addr: Hardware address of the interface
 */
struct interface {
	bool added;
	bool active;
	char *name;
	uint8_t hw_addr[ETH_ALEN];
};

/**
 * struct interface_list: List of interfaces in sys entry
 * @s_count: Number of socs
 * @r_count: Number of radios
 * @v_count: Number of vaps
 * @soc: Array of Soc Interface details
 * @radio: Array of Radio interface details
 * @vap: Array of vap interface details
 */
struct interface_list {
	uint8_t s_count;
	uint8_t r_count;
	uint8_t v_count;
	struct interface soc[MAX_SOC_NUM];
	struct interface radio[MAX_RADIO_NUM];
	struct interface vap[MAX_VAP_NUM];
};

/**
 * struct object_list: Holds request object hierarchy
 * @nlsent: Flag to indicate nl request is sent
 * @obj_type: Specifies stats_object_e type
 * @ifname: Interface to be used for sending nl command
 * @hw_addr: Hardware address if it is a STA type object
 * @parent: Pionter to parent object list
 * @child: Pointer to child object list
 * @next: Pointer to next object in the list
 */
struct object_list {
	bool nlsent;
	enum stats_object_e obj_type;
	char ifname[IFNAME_LEN];
	uint8_t hw_addr[ETH_ALEN];
	struct object_list *parent;
	struct object_list *child;
	struct object_list *next;
};

/**
 * struct feat_parser_t: Defines feature name and corresponding ID
 * @name: Name of the feature passed from userspace
 * @id:   Feature flag corresponding to name
 */
struct feat_parser_t {
	char *name;
	u_int64_t id;
};

/**
 * Mapping for Supported Features
 */
static struct feat_parser_t g_feat[] = {
	{ "ALL", STATS_FEAT_FLG_ALL },
	{ "ME", STATS_FEAT_FLG_ME },
	{ "TX", STATS_FEAT_FLG_TX },
	{ "RX", STATS_FEAT_FLG_RX },
	{ "AST", STATS_FEAT_FLG_AST },
	{ "CFR", STATS_FEAT_FLG_CFR },
	{ "FWD", STATS_FEAT_FLG_FWD },
	{ "HTT", STATS_FEAT_FLG_HTT },
	{ "RAW", STATS_FEAT_FLG_RAW },
	{ "RDK", STATS_FEAT_FLG_PEER },
	{ "TSO", STATS_FEAT_FLG_TSO },
	{ "TWT", STATS_FEAT_FLG_TWT },
	{ "VOW", STATS_FEAT_FLG_VOW },
	{ "WDI", STATS_FEAT_FLG_WDI },
	{ "WMI", STATS_FEAT_FLG_WMI },
	{ "IGMP", STATS_FEAT_FLG_IGMP },
	{ "LINK", STATS_FEAT_FLG_LINK },
	{ "MESH", STATS_FEAT_FLG_MESH },
	{ "RATE", STATS_FEAT_FLG_RATE },
	{ "NAWDS", STATS_FEAT_FLG_NAWDS },
	{ "DELAY", STATS_FEAT_FLG_DELAY },
	{ "JITTER", STATS_FEAT_FLG_JITTER },
	{ "TXCAP", STATS_FEAT_FLG_TXCAP },
	{ "MONITOR", STATS_FEAT_FLG_MONITOR },
	{ "SAWFDELAY", STATS_FEAT_FLG_SAWFDELAY },
	{ "SAWFTX", STATS_FEAT_FLG_SAWFTX },
	{ NULL, 0 },
};

/* Global nl policy for response attributes */
struct nla_policy g_policy[QCA_WLAN_VENDOR_ATTR_FEAT_MAX] = {
	[QCA_WLAN_VENDOR_ATTR_FEAT_ME] = { .type = NLA_UNSPEC },
	[QCA_WLAN_VENDOR_ATTR_FEAT_RX] = { .type = NLA_UNSPEC },
	[QCA_WLAN_VENDOR_ATTR_FEAT_TX] = { .type = NLA_UNSPEC },
	[QCA_WLAN_VENDOR_ATTR_FEAT_AST] = { .type = NLA_UNSPEC },
	[QCA_WLAN_VENDOR_ATTR_FEAT_CFR] = { .type = NLA_UNSPEC },
	[QCA_WLAN_VENDOR_ATTR_FEAT_FWD] = { .type = NLA_UNSPEC },
	[QCA_WLAN_VENDOR_ATTR_FEAT_HTT] = { .type = NLA_UNSPEC },
	[QCA_WLAN_VENDOR_ATTR_FEAT_RAW] = { .type = NLA_UNSPEC },
	[QCA_WLAN_VENDOR_ATTR_FEAT_TSO] = { .type = NLA_UNSPEC },
	[QCA_WLAN_VENDOR_ATTR_FEAT_TWT] = { .type = NLA_UNSPEC },
	[QCA_WLAN_VENDOR_ATTR_FEAT_WDI] = { .type = NLA_UNSPEC },
	[QCA_WLAN_VENDOR_ATTR_FEAT_WMI] = { .type = NLA_UNSPEC },
	[QCA_WLAN_VENDOR_ATTR_FEAT_IGMP] = { .type = NLA_UNSPEC },
	[QCA_WLAN_VENDOR_ATTR_FEAT_LINK] = { .type = NLA_UNSPEC },
	[QCA_WLAN_VENDOR_ATTR_FEAT_MESH] = { .type = NLA_UNSPEC },
	[QCA_WLAN_VENDOR_ATTR_FEAT_RATE] = { .type = NLA_UNSPEC },
	[QCA_WLAN_VENDOR_ATTR_FEAT_NAWDS] = { .type = NLA_UNSPEC },
	[QCA_WLAN_VENDOR_ATTR_FEAT_DELAY] = { .type = NLA_UNSPEC },
	[QCA_WLAN_VENDOR_ATTR_FEAT_JITTER] = { .type = NLA_UNSPEC },
	[QCA_WLAN_VENDOR_ATTR_FEAT_TXCAP] = { .type = NLA_UNSPEC },
	[QCA_WLAN_VENDOR_ATTR_FEAT_MONITOR] = { .type = NLA_UNSPEC },
	[QCA_WLAN_VENDOR_ATTR_FEAT_SAWFDELAY] = { .type = NLA_UNSPEC },
	[QCA_WLAN_VENDOR_ATTR_FEAT_SAWFTX] = { .type = NLA_UNSPEC },
};

static int is_ifname_valid(const char *ifname, enum stats_object_e obj)
{
	int i, ret;
	ssize_t size = 0;
	char *str = 0;
	char path[100];
	FILE *fp;

	assert(ifname);

	/*
	 * At this step, we only validate if the string makes sense.
	 * If the interface doesn't actually exist, we'll throw an
	 * error at the place where we make system calls to try and
	 * use the interface.
	 * Reduces the no. of ioctl calls.
	 */
	if (STATS_OBJ_VAP == obj) {
		ret = 0;
		size = sizeof(path);
		if ((strlcpy(path, PATH_SYSNET_DEV, size) >= size) ||
		    (strlcat(path, ifname, size) >= size) ||
		    (strlcat(path, "/parent", size) >= size)) {
			STATS_ERR("Error creating pathname\n");
			return 0;
		}
		fp = fopen(path, "r");
		if (fp) {
			fclose(fp);
			ret = 1;
		}
	} else {
		ret = 1;
		switch (obj) {
		case STATS_OBJ_AP:
			str = "soc";
			size = 4;
			break;
		case STATS_OBJ_RADIO:
			str = "wifi";
			size = 5;
			break;
		default:
			return 0;
		}

		if ((strncmp(ifname, str, size - 1) == 0) &&
		    ifname[size - 1] && isdigit(ifname[size - 1])) {
			/*
			 * We don't make any assumptions on max no. of radio
			 * interfaces, at this step.
			 */
			for (i = size; ifname[i]; i++) {
				if (!isdigit(ifname[i]))
					return 0;
			}
		} else {
			ret = 0;
		}
	}

	return ret;
}

static int is_cfg80211_mode_enabled(void)
{
	FILE *fp;
	char filename[32] = {0};
	int i = 0;
	int ret = 0;

	for (i = 0; i < MAX_RADIO_NUM; i++) {
		snprintf(filename, sizeof(filename),
			 CFG80211_MODE_FILE_PATH, i);
		fp = fopen(filename, "r");
		if (fp) {
			ret = 1;
			fclose(fp);
		}
	}

	return ret;
}

static void free_interface_list(struct interface_list *if_list)
{
	u_int8_t inx = 0;

	for (inx = 0; inx < MAX_SOC_NUM; inx++) {
		if (if_list->soc[inx].name)
			free(if_list->soc[inx].name);
	}
	if_list->s_count = 0;
	for (inx = 0; inx < MAX_RADIO_NUM; inx++) {
		if (if_list->radio[inx].name)
			free(if_list->radio[inx].name);
	}
	if_list->r_count = 0;
	for (inx = 0; inx < MAX_VAP_NUM; inx++) {
		if (if_list->vap[inx].name)
			free(if_list->vap[inx].name);
	}
	if_list->v_count = 0;
}

static int get_active_radio_intf_for_soc(struct interface_list *if_list,
					 struct soc_ifnames *soc_if,
					 char *soc_name, uint8_t *count)
{
	DIR *dir = NULL;
	char path[100] = {'\0'};
	ssize_t bufsize = sizeof(path);
	uint8_t inx = 0;

	assert(soc_name && soc_if);

	if ((strlcpy(path, PATH_SYSNET_DEV, bufsize) >= bufsize) ||
	    (strlcat(path, soc_name, bufsize) >= bufsize) ||
	    (strlcat(path, "/device/net", bufsize) >= bufsize)) {
		STATS_ERR("Error creating pathname\n");
		return -EINVAL;
	}

	dir = opendir(path);
	if (!dir) {
		perror(path);
		return -ENOENT;
	}

	while (1) {
		struct dirent *entry;
		char *d_name;
		uint8_t rinx;
		bool found = false;

		entry = readdir(dir);
		if (!entry)
			break;
		if ((inx >= MAX_RADIO_NUM) || !soc_if)
			break;
		d_name = entry->d_name;
		if (entry->d_type & (DT_DIR | DT_LNK)) {
			if (strlcpy(soc_if->ifname, d_name, IFNAME_LEN) >=
				    IFNAME_LEN) {
				STATS_ERR("Unable to fetch interface name\n");
				closedir(dir);
				return -EIO;
			}
			for (rinx = 0; rinx < if_list->r_count; rinx++) {
				if (!strncmp(soc_if->ifname,
					     if_list->radio[rinx].name,
					     RADIO_IFNAME_SIZE)) {
					found = true;
					break;
				}
			}
			if (found && if_list->radio[rinx].active) {
				inx++;
				soc_if++;
			} else {
				memset(soc_if->ifname, '\0', IFNAME_LEN);
			}
		}
	}

	closedir(dir);
	*count = inx;

	return 0;
}

static int fetch_all_interfaces(struct interface_list *if_list)
{
	char temp_name[IFNAME_LEN] = {'\0'};
	DIR *dir = NULL;
	u_int8_t rinx = 0;
	u_int8_t vinx = 0;
	u_int8_t sinx = 0;
	ssize_t size = 0;

	dir = opendir(PATH_SYSNET_DEV);
	if (!dir) {
		perror(PATH_SYSNET_DEV);
		return -ENOENT;
	}
	while (1) {
		struct dirent *entry;
		char *d_name;

		entry = readdir(dir);
		if (!entry)
			break;
		d_name = entry->d_name;
		if (entry->d_type & (DT_DIR | DT_LNK)) {
			if (strlcpy(temp_name, d_name, IFNAME_LEN) >=
				    IFNAME_LEN) {
				STATS_ERR("Unable to fetch interface name\n");
				closedir(dir);
				return -EIO;
			}
		} else {
			continue;
		}
		if (is_ifname_valid(temp_name, STATS_OBJ_AP)) {
			if (sinx >= MAX_SOC_NUM) {
				STATS_WARN("SOC Interfaces exceeded limit\n");
				continue;
			}
			size = strlen(temp_name) + 1;
			if_list->soc[sinx].name = (char *)malloc(size);
			if (!if_list->soc[sinx].name) {
				STATS_ERR("Unable to Allocate Memory!\n");
				closedir(dir);
				return -ENOMEM;
			}
			strlcpy(if_list->soc[sinx].name, temp_name, size);
			if_list->soc[sinx].added = false;
			sinx++;
		} else if (is_ifname_valid(temp_name, STATS_OBJ_RADIO)) {
			if (rinx >= MAX_RADIO_NUM) {
				STATS_WARN("Radio Interfaces exceeded limit\n");
				continue;
			}
			size = strlen(temp_name) + 1;
			if_list->radio[rinx].name = (char *)malloc(size);
			if (!if_list->radio[rinx].name) {
				STATS_ERR("Unable to Allocate Memory!\n");
				closedir(dir);
				return -ENOMEM;
			}
			strlcpy(if_list->radio[rinx].name, temp_name, size);
			if_list->radio[rinx].added = false;
			rinx++;
		} else if (is_ifname_valid(temp_name, STATS_OBJ_VAP)) {
			if (vinx >= MAX_VAP_NUM) {
				STATS_WARN("Vap Interfaces exceeded limit\n");
				continue;
			}
			size = strlen(temp_name) + 1;
			if_list->vap[vinx].name = (char *)malloc(size);
			if (!if_list->vap[vinx].name) {
				STATS_ERR("Unable to Allocate Memory!\n");
				closedir(dir);
				return -ENOMEM;
			}
			strlcpy(if_list->vap[vinx].name, temp_name, size);
			if_list->vap[vinx].added = false;
			vinx++;
		}
	}

	closedir(dir);
	if_list->s_count = sinx;
	if_list->r_count = rinx;
	if_list->v_count = vinx;

	return 0;
}

static int stats_lib_socket_init(void)
{
	return init_socket_context(&g_sock_ctx, STATS_NL80211_CMD_SOCK_ID,
				   STATS_NL80211_EVENT_SOCK_ID);
}

static int stats_lib_init(void)
{
	g_sock_ctx.cfg80211 = is_cfg80211_mode_enabled();

	if (!g_sock_ctx.cfg80211) {
		STATS_ERR("CFG80211 mode is disabled!\n");
		return -EINVAL;
	}

	if (stats_lib_socket_init()) {
		STATS_ERR("Failed to initialise socket\n");
		return -EIO;
	}
	/* There are few generic IOCTLS, so we need to have sockfd */
	g_sock_ctx.sock_fd = socket(AF_INET, SOCK_DGRAM, 0);
	if (g_sock_ctx.sock_fd < 0) {
		STATS_ERR("socket creation failed\n");
		return -EIO;
	}

	return 0;
}

static void stats_lib_deinit(void)
{
	if (!g_sock_ctx.cfg80211)
		return;
	destroy_socket_context(&g_sock_ctx);
	close(g_sock_ctx.sock_fd);
}

static int32_t get_hw_address(const char *if_name, uint8_t *hw_addr)
{
	struct ifreq dev = {0};

	dev.ifr_addr.sa_family = AF_INET;
	memset(dev.ifr_name, '\0', IFNAMSIZ);
	if (strlcpy(dev.ifr_name, if_name, IFNAMSIZ) > IFNAMSIZ)
		return -EIO;

	if (ioctl(g_sock_ctx.sock_fd, SIOCGIFHWADDR, &dev) < 0)
		return -EIO;

	memcpy(hw_addr, dev.ifr_hwaddr.sa_data, ETH_ALEN);

	return 0;
}

static bool is_interface_active(const char *if_name, enum stats_object_e obj)
{
	struct ifreq  dev = {0};

	dev.ifr_addr.sa_family = AF_INET;
	memset(dev.ifr_name, '\0', IFNAMSIZ);
	if (strlcpy(dev.ifr_name, if_name, IFNAMSIZ) > IFNAMSIZ)
		return false;

	if (ioctl(g_sock_ctx.sock_fd, SIOCGIFFLAGS, &dev) < 0)
		return false;

	if ((dev.ifr_flags & IFF_RUNNING) &&
	    ((obj == STATS_OBJ_RADIO) || (dev.ifr_flags & IFF_UP)))
		return true;

	return false;
}

static bool is_vap_radiochild(const char *rif_name, const uint8_t *rhw_addr,
			      const char *vif_name, const uint8_t *vhw_addr)
{
	char path[100];
	char parent[IFNAMSIZ];
	FILE *fp;
	ssize_t bufsize = sizeof(path);

	/* Exclude MSB to compare as LA bit would have been set */
	if (!memcmp(&vhw_addr[1], &rhw_addr[1], 5))
		return true;

	if ((strlcpy(path, PATH_SYSNET_DEV, bufsize) >= bufsize) ||
	    (strlcat(path, vif_name, bufsize) >= bufsize) ||
	    (strlcat(path, "/parent", bufsize) >= bufsize)) {
		STATS_ERR("Error creating pathname\n");
		return false;
	}
	fp = fopen(path, "r");
	if (!fp)
		return false;
	fgets(parent, IFNAMSIZ, fp);
	fclose(fp);

	if (!strncmp(parent, rif_name, RADIO_IFNAME_SIZE))
		return true;

	return false;
}

static int is_valid_cmd(struct stats_command *cmd)
{
	u_int8_t *sta_mac = cmd->sta_mac.ether_addr_octet;

	switch (cmd->obj) {
	case STATS_OBJ_AP:
		if (sta_mac[0])
			STATS_WARN("Ignore STA MAC for AP stats\n");
		break;
	case STATS_OBJ_RADIO:
		if (!cmd->if_name[0]) {
			STATS_ERR("Radio Interface name not configured.\n");
			return -EINVAL;
		}
		if (!is_ifname_valid(cmd->if_name, STATS_OBJ_RADIO)) {
			STATS_ERR("Radio Interface name invalid.\n");
			return -EINVAL;
		}
		if (sta_mac[0])
			STATS_WARN("Ignore STA MAC address input for Radio\n");
		break;
	case STATS_OBJ_VAP:
		if (!cmd->if_name[0]) {
			STATS_ERR("VAP Interface name not configured.\n");
			return -EINVAL;
		}
		if (!is_ifname_valid(cmd->if_name, STATS_OBJ_VAP)) {
			STATS_ERR("VAP Interface name invalid.\n");
			return -EINVAL;
		}
		if (sta_mac[0])
			STATS_WARN("Ignore STA MAC address input for VAP\n");
		break;
	case STATS_OBJ_STA:
		if (cmd->recursive)
			STATS_WARN("Ignore recursive display for STA\n");
		cmd->recursive = false;
		break;
	default:
		STATS_ERR("Unknown Stats object.\n");
		return -EINVAL;
	}

	return 0;
}

static int32_t prepare_request(struct nl_msg *nlmsg, struct stats_command *cmd)
{
	int32_t ret = 0;

	if (nla_put_u8(nlmsg, QCA_WLAN_VENDOR_ATTR_TELEMETRIC_LEVEL,
		       cmd->lvl)) {
		STATS_ERR("failed to put stats level\n");
		return -EIO;
	}
	if (nla_put_u8(nlmsg, QCA_WLAN_VENDOR_ATTR_TELEMETRIC_OBJECT,
		       cmd->obj)) {
		STATS_ERR("failed to put stats object\n");
		return -EIO;
	}
	if (nla_put_u8(nlmsg, QCA_WLAN_VENDOR_ATTR_TELEMETRIC_TYPE,
		       cmd->type)) {
		STATS_ERR("failed to put stats category type\n");
		return -EIO;
	}
	if (!cmd->recursive &&
	    nla_put_flag(nlmsg, QCA_WLAN_VENDOR_ATTR_TELEMETRIC_AGGREGATE)) {
		STATS_ERR("failed to put aggregate flag\n");
		return -EIO;
	}
	if (nla_put_u64(nlmsg, QCA_WLAN_VENDOR_ATTR_TELEMETRIC_FEATURE_FLAG,
			cmd->feat_flag)) {
		STATS_ERR("failed to put feature flag\n");
		return -EIO;
	}
	if (cmd->obj == STATS_OBJ_STA) {
		if (nla_put(nlmsg, QCA_WLAN_VENDOR_ATTR_TELEMETRIC_STA_MAC,
			    ETH_ALEN, cmd->sta_mac.ether_addr_octet)) {
			STATS_ERR("failed to put sta MAC\n");
			return -EIO;
		}
		if (nla_put_u8(nlmsg, QCA_WLAN_VENDOR_ATTR_TELEMETRIC_SERVICEID,
			       cmd->serviceid)) {
			STATS_ERR("failed to put serviceid\n");
			return -EIO;
		}
	}

	return ret;
}

static struct stats_obj *allocate_stats_obj()
{
	struct stats_obj *obj;

	obj = malloc(sizeof(struct stats_obj));
	if (!obj) {
		STATS_ERR("Unable to allocate stats_obj!\n");
		return NULL;
	}
	memset(obj, 0, sizeof(struct stats_obj));

	return obj;
}

static void set_parent_stats_obj(struct stats_obj *obj_last,
				 struct stats_obj *obj)
{
	struct stats_obj *temp = obj_last;

	if (!obj->pif_name[0])
		return;

	while (temp) {
		if (!strncmp(temp->u_id.if_name, obj->pif_name,
			     IFNAME_LEN)) {
			obj->parent = temp;
			break;
		}
		temp = temp->parent;
	}
}

static void add_stats_obj(struct reply_buffer *reply, struct stats_obj *obj)
{
	if (!reply || !obj)
		return;

	if (!reply->obj_head) {
		reply->obj_head = obj;
		reply->obj_last = obj;
	} else {
		reply->obj_last->next = obj;
		set_parent_stats_obj(reply->obj_last, obj);
		reply->obj_last = obj;
	}
}

static void parse_basic_sta(struct nlattr *rattr, struct stats_obj *obj)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_FEAT_MAX + 1] = {0};
	struct nlattr *attr = NULL;
	struct basic_peer_data *data = NULL;
	struct basic_peer_ctrl *ctrl = NULL;

	if (!rattr || nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_FEAT_MAX,
				       rattr, g_policy)) {
		STATS_ERR("NLA Parsing failed\n");
		return;
	}
	switch (obj->type) {
	case STATS_TYPE_DATA:
		if (obj->stats) {
			data = obj->stats;
		} else {
			data = malloc(sizeof(struct basic_peer_data));
			if (!data)
				return;
			memset(data, 0, sizeof(struct basic_peer_data));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX];
			data->tx = malloc(sizeof(struct basic_peer_data_tx));
			if (data->tx)
				memcpy(data->tx, nla_data(attr),
				       sizeof(struct basic_peer_data_tx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX];
			data->rx = malloc(sizeof(struct basic_peer_data_rx));
			if (data->rx)
				memcpy(data->rx, nla_data(attr),
				       sizeof(struct basic_peer_data_rx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_LINK]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_LINK];
			data->link =
				malloc(sizeof(struct basic_peer_data_link));
			if (data->link)
				memcpy(data->link, nla_data(attr),
				       sizeof(struct basic_peer_data_link));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RATE]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RATE];
			data->rate =
				malloc(sizeof(struct basic_peer_data_rate));
			if (data->rate)
				memcpy(data->rate, nla_data(attr),
				       sizeof(struct basic_peer_data_rate));
		}
		obj->stats = data;
		break;
	case STATS_TYPE_CTRL:
		if (obj->stats) {
			ctrl = obj->stats;
		} else {
			ctrl = malloc(sizeof(struct basic_peer_ctrl));
			if (!ctrl)
				return;
			memset(ctrl, 0, sizeof(struct basic_peer_ctrl));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX];
			ctrl->tx = malloc(sizeof(struct basic_peer_ctrl_tx));
			if (ctrl->tx)
				memcpy(ctrl->tx, nla_data(attr),
				       sizeof(struct basic_peer_ctrl_tx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX];
			ctrl->rx = malloc(sizeof(struct basic_peer_ctrl_rx));
			if (ctrl->rx)
				memcpy(ctrl->rx, nla_data(attr),
				       sizeof(struct basic_peer_ctrl_rx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_LINK]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_LINK];
			ctrl->link =
				malloc(sizeof(struct basic_peer_ctrl_link));
			if (ctrl->link)
				memcpy(ctrl->link, nla_data(attr),
				       sizeof(struct basic_peer_ctrl_link));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RATE]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RATE];
			ctrl->rate =
				malloc(sizeof(struct basic_peer_ctrl_rate));
			if (ctrl->rate)
				memcpy(ctrl->rate, nla_data(attr),
				       sizeof(struct basic_peer_ctrl_rate));
		}
		obj->stats = ctrl;
		break;
	}
}

static void parse_basic_vap(struct nlattr *rattr, struct stats_obj *obj)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_FEAT_MAX + 1] = {0};
	struct nlattr *attr = NULL;
	struct basic_vdev_data *data = NULL;
	struct basic_vdev_ctrl *ctrl = NULL;

	if (!rattr || nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_FEAT_MAX,
				       rattr, g_policy)) {
		STATS_ERR("NLA Parsing failed\n");
		return;
	}

	switch (obj->type) {
	case STATS_TYPE_DATA:
		if (obj->stats) {
			data = obj->stats;
		} else {
			data = malloc(sizeof(struct basic_vdev_data));
			if (!data)
				return;
			memset(data, 0, sizeof(struct basic_vdev_data));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX];
			data->tx = malloc(sizeof(struct basic_vdev_data_tx));
			if (data->tx)
				memcpy(data->tx, nla_data(attr),
				       sizeof(struct basic_vdev_data_tx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX];
			data->rx = malloc(sizeof(struct basic_vdev_data_rx));
			if (data->rx)
				memcpy(data->rx, nla_data(attr),
				       sizeof(struct basic_vdev_data_rx));
		}
		obj->stats = data;
		break;
	case STATS_TYPE_CTRL:
		if (obj->stats) {
			ctrl = obj->stats;
		} else {
			ctrl = malloc(sizeof(struct basic_vdev_ctrl));
			if (!ctrl)
				return;
			memset(ctrl, 0, sizeof(struct basic_vdev_ctrl));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX];
			ctrl->tx = malloc(sizeof(struct basic_vdev_ctrl_tx));
			if (ctrl->tx)
				memcpy(ctrl->tx, nla_data(attr),
				       sizeof(struct basic_vdev_ctrl_tx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX];
			ctrl->rx = malloc(sizeof(struct basic_vdev_ctrl_rx));
			if (ctrl->rx)
				memcpy(ctrl->rx, nla_data(attr),
				       sizeof(struct basic_vdev_ctrl_rx));
		}
		obj->stats = ctrl;
		break;
	}
}

static void parse_basic_radio(struct nlattr *rattr, struct stats_obj *obj)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_FEAT_MAX + 1] = {0};
	struct nlattr *attr = NULL;
	struct basic_pdev_data *data = NULL;
	struct basic_pdev_ctrl *ctrl = NULL;

	if (!rattr || nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_FEAT_MAX,
				       rattr, g_policy)) {
		STATS_ERR("NLA Parsing failed\n");
		return;
	}
	switch (obj->type) {
	case STATS_TYPE_DATA:
		if (obj->stats) {
			data = obj->stats;
		} else {
			data = malloc(sizeof(struct basic_pdev_data));
			if (!data)
				return;
			memset(data, 0, sizeof(struct basic_pdev_data));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX];
			data->tx = malloc(sizeof(struct basic_pdev_data_tx));
			if (data->tx)
				memcpy(data->tx, nla_data(attr),
				       sizeof(struct basic_pdev_data_tx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX];
			data->rx = malloc(sizeof(struct basic_pdev_data_rx));
			if (data->rx)
				memcpy(data->rx, nla_data(attr),
				       sizeof(struct basic_pdev_data_rx));
		}
		obj->stats = data;
		break;
	case STATS_TYPE_CTRL:
		if (obj->stats) {
			ctrl = obj->stats;
		} else {
			ctrl = malloc(sizeof(struct basic_pdev_ctrl));
			if (!ctrl)
				return;
			memset(ctrl, 0, sizeof(struct basic_pdev_ctrl));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX];
			ctrl->tx = malloc(sizeof(struct basic_pdev_ctrl_tx));
			if (ctrl->tx)
				memcpy(ctrl->tx, nla_data(attr),
				       sizeof(struct basic_pdev_ctrl_tx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX];
			ctrl->rx = malloc(sizeof(struct basic_pdev_ctrl_rx));
			if (ctrl->rx)
				memcpy(ctrl->rx, nla_data(attr),
				       sizeof(struct basic_pdev_ctrl_rx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_LINK]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_LINK];
			ctrl->link =
				malloc(sizeof(struct basic_pdev_ctrl_link));
			if (ctrl->link)
				memcpy(ctrl->link, nla_data(attr),
				       sizeof(struct basic_pdev_ctrl_link));
		}
		obj->stats = ctrl;
		break;
	}
}

static void parse_basic_ap(struct nlattr *rattr, struct stats_obj *obj)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_FEAT_MAX + 1] = {0};
	struct basic_psoc_data *data = NULL;

	if (!rattr || nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_FEAT_MAX,
				       rattr, g_policy)) {
		STATS_ERR("NLA Parsing failed\n");
		return;
	}
	if (obj->type == STATS_TYPE_CTRL)
		return;

	if (obj->stats) {
		data = obj->stats;
	} else {
		data = malloc(sizeof(struct basic_psoc_data));
		if (!data)
			return;
		memset(data, 0, sizeof(struct basic_psoc_data));
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX]) {
		data->tx = malloc(sizeof(struct basic_psoc_data_tx));
		if (data->tx)
			memcpy(data->tx,
			       nla_data(tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX]),
			       sizeof(struct basic_psoc_data_tx));
	}
	if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX]) {
		data->rx = malloc(sizeof(struct basic_psoc_data_rx));
		if (data->rx)
			memcpy(data->rx,
			       nla_data(tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX]),
			       sizeof(struct basic_psoc_data_rx));
	}

	obj->stats = data;
}

#if WLAN_ADVANCE_TELEMETRY
static void parse_advance_sta(struct nlattr *rattr, struct stats_obj *obj)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_FEAT_MAX + 1] = {0};
	struct nlattr *attr = NULL;
	struct advance_peer_data *data = NULL;
	struct advance_peer_ctrl *ctrl = NULL;

	if (!rattr || nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_FEAT_MAX,
				       rattr, g_policy)) {
		STATS_ERR("NLA Parsing failed\n");
		return;
	}

	switch (obj->type) {
	case STATS_TYPE_DATA:
		if (obj->stats) {
			data = obj->stats;
		} else {
			data = malloc(sizeof(struct advance_peer_data));
			if (!data)
				return;
			memset(data, 0, sizeof(struct advance_peer_data));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX];
			data->tx = malloc(sizeof(struct advance_peer_data_tx));
			if (data->tx)
				memcpy(data->tx, nla_data(attr),
				       sizeof(struct advance_peer_data_tx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX];
			data->rx = malloc(sizeof(struct advance_peer_data_rx));
			if (data->rx)
				memcpy(data->rx, nla_data(attr),
				       sizeof(struct advance_peer_data_rx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RAW]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RAW];
			data->raw =
				malloc(sizeof(struct advance_peer_data_raw));
			if (data->raw)
				memcpy(data->raw, nla_data(attr),
				       sizeof(struct advance_peer_data_raw));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_FWD]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_FWD];
			data->fwd =
				malloc(sizeof(struct advance_peer_data_fwd));
			if (data->fwd)
				memcpy(data->fwd, nla_data(attr),
				       sizeof(struct advance_peer_data_fwd));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TWT]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_TWT];
			data->twt =
				malloc(sizeof(struct advance_peer_data_twt));
			if (data->twt)
				memcpy(data->twt, nla_data(attr),
				       sizeof(struct advance_peer_data_twt));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_LINK]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_LINK];
			data->link =
				malloc(sizeof(struct advance_peer_data_link));
			if (data->link)
				memcpy(data->link, nla_data(attr),
				       sizeof(struct advance_peer_data_link));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RATE]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RATE];
			data->rate =
				malloc(sizeof(struct advance_peer_data_rate));
			if (data->rate)
				memcpy(data->rate, nla_data(attr),
				       sizeof(struct advance_peer_data_rate));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_NAWDS]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_NAWDS];
			data->nawds =
				malloc(sizeof(struct advance_peer_data_nawds));
			if (data->nawds)
				memcpy(data->nawds, nla_data(attr),
				       sizeof(struct advance_peer_data_nawds));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_DELAY]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_DELAY];
			data->delay =
				malloc(sizeof(struct advance_peer_data_delay));
			if (data->delay)
				memcpy(data->delay, nla_data(attr),
				       sizeof(struct advance_peer_data_delay));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_JITTER]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_JITTER];
			data->jitter =
				malloc(sizeof(struct advance_peer_data_jitter));
			if (data->jitter)
				memcpy(data->jitter, nla_data(attr),
				       sizeof(struct advance_peer_data_jitter));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_SAWFDELAY]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_SAWFDELAY];
			data->sawfdelay =
				malloc(sizeof(struct advance_peer_data_sawfdelay));
			if (data->sawfdelay)
				memcpy(data->sawfdelay, nla_data(attr),
				       sizeof(struct advance_peer_data_sawfdelay));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_SAWFTX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_SAWFTX];
			data->sawftx =
				malloc(sizeof(struct advance_peer_data_sawftx));
			if (data->sawftx)
				memcpy(data->sawftx, nla_data(attr),
				       sizeof(struct advance_peer_data_sawftx));
		}
		obj->stats = data;
		break;
	case STATS_TYPE_CTRL:
		if (obj->stats) {
			ctrl = obj->stats;
		} else {
			ctrl = malloc(sizeof(struct advance_peer_ctrl));
			if (!ctrl)
				return;
			memset(ctrl, 0, sizeof(struct advance_peer_ctrl));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX];
			ctrl->tx = malloc(sizeof(struct advance_peer_ctrl_tx));
			if (ctrl->tx)
				memcpy(ctrl->tx, nla_data(attr),
				       sizeof(struct advance_peer_ctrl_tx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX];
			ctrl->rx = malloc(sizeof(struct advance_peer_ctrl_rx));
			if (ctrl->rx)
				memcpy(ctrl->rx, nla_data(attr),
				       sizeof(struct advance_peer_ctrl_rx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TWT]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_TWT];
			ctrl->twt =
				malloc(sizeof(struct advance_peer_ctrl_twt));
			if (ctrl->twt)
				memcpy(ctrl->twt, nla_data(attr),
				       sizeof(struct advance_peer_ctrl_twt));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_LINK]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_LINK];
			ctrl->link =
				malloc(sizeof(struct advance_peer_ctrl_link));
			if (ctrl->link)
				memcpy(ctrl->link, nla_data(attr),
				       sizeof(struct advance_peer_ctrl_link));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RATE]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RATE];
			ctrl->rate =
				malloc(sizeof(struct advance_peer_ctrl_rate));
			if (ctrl->rate)
				memcpy(ctrl->rate, nla_data(attr),
				       sizeof(struct advance_peer_ctrl_rate));
		}
		obj->stats = ctrl;
		break;
	}
}

static void parse_advance_vap(struct nlattr *rattr, struct stats_obj *obj)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_FEAT_MAX + 1] = {0};
	struct nlattr *attr = NULL;
	struct advance_vdev_data *data = NULL;
	struct advance_vdev_ctrl *ctrl = NULL;

	if (!rattr || nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_FEAT_MAX,
				       rattr, g_policy)) {
		STATS_ERR("NLA Parsing failed\n");
		return;
	}

	switch (obj->type) {
	case STATS_TYPE_DATA:
		if (obj->stats) {
			data = obj->stats;
		} else {
			data = malloc(sizeof(struct advance_vdev_data));
			if (!data)
				return;
			memset(data, 0, sizeof(struct advance_vdev_data));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX];
			data->tx = malloc(sizeof(struct advance_vdev_data_tx));
			if (data->tx)
				memcpy(data->tx, nla_data(attr),
				       sizeof(struct advance_vdev_data_tx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX];
			data->rx = malloc(sizeof(struct advance_vdev_data_rx));
			if (data->rx)
				memcpy(data->rx, nla_data(attr),
				       sizeof(struct advance_vdev_data_rx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_ME]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_ME];
			data->me = malloc(sizeof(struct advance_vdev_data_me));
			if (data->me)
				memcpy(data->me, nla_data(attr),
				       sizeof(struct advance_vdev_data_me));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RAW]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RAW];
			data->raw =
				malloc(sizeof(struct advance_vdev_data_raw));
			if (data->raw)
				memcpy(data->raw, nla_data(attr),
				       sizeof(struct advance_vdev_data_raw));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TSO]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_TSO];
			data->tso =
				malloc(sizeof(struct advance_vdev_data_tso));
			if (data->tso)
				memcpy(data->tso, nla_data(attr),
				       sizeof(struct advance_vdev_data_tso));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_IGMP]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_IGMP];
			data->igmp =
				malloc(sizeof(struct advance_vdev_data_igmp));
			if (data->igmp)
				memcpy(data->igmp, nla_data(attr),
				       sizeof(struct advance_vdev_data_igmp));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_MESH]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_MESH];
			data->mesh =
				malloc(sizeof(struct advance_vdev_data_mesh));
			if (data->mesh)
				memcpy(data->mesh, nla_data(attr),
				       sizeof(struct advance_vdev_data_mesh));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_NAWDS]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_NAWDS];
			data->nawds =
				malloc(sizeof(struct advance_vdev_data_nawds));
			if (data->nawds)
				memcpy(data->nawds, nla_data(attr),
				       sizeof(struct advance_vdev_data_nawds));
		}
		obj->stats = data;
		break;
	case STATS_TYPE_CTRL:
		if (obj->stats) {
			ctrl = obj->stats;
		} else {
			ctrl = malloc(sizeof(struct advance_vdev_ctrl));
			if (!ctrl)
				return;
			memset(ctrl, 0, sizeof(struct advance_vdev_ctrl));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX];
			ctrl->tx = malloc(sizeof(struct advance_vdev_ctrl_tx));
			if (ctrl->tx)
				memcpy(ctrl->tx, nla_data(attr),
				       sizeof(struct advance_vdev_ctrl_tx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX];
			ctrl->rx = malloc(sizeof(struct advance_vdev_ctrl_rx));
			if (ctrl->rx)
				memcpy(ctrl->rx, nla_data(attr),
				       sizeof(struct advance_vdev_ctrl_rx));
		}
		obj->stats = ctrl;
		break;
	}
}

static void parse_advance_radio(struct nlattr *rattr, struct stats_obj *obj)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_FEAT_MAX + 1] = {0};
	struct nlattr *attr = NULL;
	struct advance_pdev_data *data = NULL;
	struct advance_pdev_ctrl *ctrl = NULL;

	if (!rattr || nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_FEAT_MAX,
				       rattr, g_policy)) {
		STATS_ERR("NLA Parsing failed\n");
		return;
	}

	switch (obj->type) {
	case STATS_TYPE_DATA:
		if (obj->stats) {
			data = obj->stats;
		} else {
			data = malloc(sizeof(struct advance_pdev_data));
			if (!data)
				return;
			memset(data, 0, sizeof(struct advance_pdev_data));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX];
			data->tx = malloc(sizeof(struct advance_pdev_data_tx));
			if (data->tx)
				memcpy(data->tx, nla_data(attr),
				       sizeof(struct advance_pdev_data_tx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX];
			data->rx = malloc(sizeof(struct advance_pdev_data_rx));
			if (data->rx)
				memcpy(data->rx, nla_data(attr),
				       sizeof(struct advance_pdev_data_rx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_ME]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_ME];
			data->me = malloc(sizeof(struct advance_pdev_data_me));
			if (data->me)
				memcpy(data->me, nla_data(attr),
				       sizeof(struct advance_pdev_data_me));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RAW]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RAW];
			data->raw =
				malloc(sizeof(struct advance_pdev_data_raw));
			if (data->raw)
				memcpy(data->raw, nla_data(attr),
				       sizeof(struct advance_pdev_data_raw));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TSO]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_TSO];
			data->tso =
				malloc(sizeof(struct advance_pdev_data_tso));
			if (data->tso)
				memcpy(data->tso, nla_data(attr),
				       sizeof(struct advance_pdev_data_tso));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_IGMP]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_IGMP];
			data->igmp =
				malloc(sizeof(struct advance_pdev_data_igmp));
			if (data->igmp)
				memcpy(data->igmp, nla_data(attr),
				       sizeof(struct advance_pdev_data_igmp));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_MESH]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_MESH];
			data->mesh =
				malloc(sizeof(struct advance_pdev_data_mesh));
			if (data->mesh)
				memcpy(data->mesh, nla_data(attr),
				       sizeof(struct advance_pdev_data_mesh));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_NAWDS]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_NAWDS];
			data->nawds =
				malloc(sizeof(struct advance_pdev_data_nawds));
			if (data->nawds)
				memcpy(data->nawds, nla_data(attr),
				       sizeof(struct advance_pdev_data_nawds));
		}
		obj->stats = data;
		break;
	case STATS_TYPE_CTRL:
		if (obj->stats) {
			ctrl = obj->stats;
		} else {
			ctrl = malloc(sizeof(struct advance_pdev_ctrl));
			if (!ctrl)
				return;
			memset(ctrl, 0, sizeof(struct advance_pdev_ctrl));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX];
			ctrl->tx = malloc(sizeof(struct advance_pdev_ctrl_tx));
			if (ctrl->tx)
				memcpy(ctrl->tx, nla_data(attr),
				       sizeof(struct advance_pdev_ctrl_tx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX];
			ctrl->rx = malloc(sizeof(struct advance_pdev_ctrl_rx));
			if (ctrl->rx)
				memcpy(ctrl->rx, nla_data(attr),
				       sizeof(struct advance_pdev_ctrl_rx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_LINK]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_LINK];
			ctrl->link =
				malloc(sizeof(struct advance_pdev_ctrl_link));
			if (ctrl->link)
				memcpy(ctrl->link, nla_data(attr),
				       sizeof(struct advance_pdev_ctrl_link));
		}
		obj->stats = ctrl;
		break;
	}
}

static void parse_advance_ap(struct nlattr *rattr, struct stats_obj *obj)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_FEAT_MAX + 1] = {0};
	struct advance_psoc_data *data = NULL;

	if (!rattr || nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_FEAT_MAX,
				       rattr, g_policy)) {
		STATS_ERR("NLA Parsing failed\n");
		return;
	}

	if (obj->type == STATS_TYPE_CTRL)
		return;

	if (obj->stats) {
		data = obj->stats;
	} else {
		data = malloc(sizeof(struct advance_psoc_data));
		if (!data)
			return;
		memset(data, 0, sizeof(struct advance_psoc_data));
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX]) {
		data->tx = malloc(sizeof(struct advance_psoc_data_tx));
		if (data->tx)
			memcpy(data->tx,
			       nla_data(tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX]),
			       sizeof(struct advance_psoc_data_tx));
	}
	if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX]) {
		data->rx = malloc(sizeof(struct advance_psoc_data_rx));
		if (data->rx)
			memcpy(data->rx,
			       nla_data(tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX]),
			       sizeof(struct advance_psoc_data_rx));
	}

	obj->stats = data;
}
#endif /* WLAN_ADVANCE_TELEMETRY */

#if WLAN_DEBUG_TELEMETRY
static void parse_debug_sta(struct nlattr *rattr, struct stats_obj *obj)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_FEAT_MAX + 1] = {0};
	struct nlattr *attr = NULL;
	struct debug_peer_data *data = NULL;
	struct debug_peer_ctrl *ctrl = NULL;

	if (!rattr || nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_FEAT_MAX,
				       rattr, g_policy)) {
		STATS_ERR("NLA Parsing failed\n");
		return;
	}

	switch (obj->type) {
	case STATS_TYPE_DATA:
		if (obj->stats) {
			data = obj->stats;
		} else {
			data = malloc(sizeof(struct debug_peer_data));
			if (!data)
				return;
			memset(data, 0, sizeof(struct debug_peer_data));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX];
			data->tx = malloc(sizeof(struct debug_peer_data_tx));
			if (data->tx)
				memcpy(data->tx, nla_data(attr),
				       sizeof(struct debug_peer_data_tx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX];
			data->rx = malloc(sizeof(struct debug_peer_data_rx));
			if (data->rx)
				memcpy(data->rx, nla_data(attr),
				       sizeof(struct debug_peer_data_rx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_LINK]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_LINK];
			data->link =
				malloc(sizeof(struct debug_peer_data_link));
			if (data->link)
				memcpy(data->link, nla_data(attr),
				       sizeof(struct debug_peer_data_link));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RATE]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RATE];
			data->rate =
				malloc(sizeof(struct debug_peer_data_rate));
			if (data->rate)
				memcpy(data->rate, nla_data(attr),
				       sizeof(struct debug_peer_data_rate));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TXCAP]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_TXCAP];
			data->txcap =
				malloc(sizeof(struct debug_peer_data_txcap));
			if (data->txcap)
				memcpy(data->txcap, nla_data(attr),
				       sizeof(struct debug_peer_data_txcap));
		}
		obj->stats = data;
		break;
	case STATS_TYPE_CTRL:
		if (obj->stats) {
			ctrl = obj->stats;
		} else {
			ctrl = malloc(sizeof(struct debug_peer_ctrl));
			if (!ctrl)
				return;
			memset(ctrl, 0, sizeof(struct debug_peer_ctrl));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX];
			ctrl->tx = malloc(sizeof(struct debug_peer_ctrl_tx));
			if (ctrl->tx)
				memcpy(ctrl->tx, nla_data(attr),
				       sizeof(struct debug_peer_ctrl_tx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX];
			ctrl->rx = malloc(sizeof(struct debug_peer_ctrl_rx));
			if (ctrl->rx)
				memcpy(ctrl->rx, nla_data(attr),
				       sizeof(struct debug_peer_ctrl_rx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_LINK]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_LINK];
			ctrl->link =
				malloc(sizeof(struct debug_peer_ctrl_link));
			if (ctrl->link)
				memcpy(ctrl->link, nla_data(attr),
				       sizeof(struct debug_peer_ctrl_link));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RATE]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RATE];
			ctrl->rate =
				malloc(sizeof(struct debug_peer_ctrl_rate));
			if (ctrl->rate)
				memcpy(ctrl->rate, nla_data(attr),
				       sizeof(struct debug_peer_ctrl_rate));
		}
		obj->stats = ctrl;
		break;
	}
}

static void parse_debug_vap(struct nlattr *rattr, struct stats_obj *obj)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_FEAT_MAX + 1] = {0};
	struct nlattr *attr = NULL;
	struct debug_vdev_data *data = NULL;
	struct debug_vdev_ctrl *ctrl = NULL;

	if (!rattr || nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_FEAT_MAX,
				       rattr, g_policy)) {
		STATS_ERR("NLA Parsing failed\n");
		return;
	}

	switch (obj->type) {
	case STATS_TYPE_DATA:
		if (obj->stats) {
			data = obj->stats;
		} else {
			data = malloc(sizeof(struct debug_vdev_data));
			if (!data)
				return;
			memset(data, 0, sizeof(struct debug_vdev_data));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX];
			data->tx = malloc(sizeof(struct debug_vdev_data_tx));
			if (data->tx)
				memcpy(data->tx, nla_data(attr),
				       sizeof(struct debug_vdev_data_tx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX];
			data->rx = malloc(sizeof(struct debug_vdev_data_rx));
			if (data->rx)
				memcpy(data->rx, nla_data(attr),
				       sizeof(struct debug_vdev_data_rx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_ME]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_ME];
			data->me = malloc(sizeof(struct debug_vdev_data_me));
			if (data->me)
				memcpy(data->me, nla_data(attr),
				       sizeof(struct debug_vdev_data_me));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RAW]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RAW];
			data->raw =
				malloc(sizeof(struct debug_vdev_data_raw));
			if (data->raw)
				memcpy(data->raw, nla_data(attr),
				       sizeof(struct debug_vdev_data_raw));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TSO]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_TSO];
			data->tso =
				malloc(sizeof(struct debug_vdev_data_tso));
			if (data->tso)
				memcpy(data->tso, nla_data(attr),
				       sizeof(struct debug_vdev_data_tso));
		}
		obj->stats = data;
		break;
	case STATS_TYPE_CTRL:
		if (obj->stats) {
			ctrl = obj->stats;
		} else {
			ctrl = malloc(sizeof(struct debug_vdev_ctrl));
			if (!ctrl)
				return;
			memset(ctrl, 0, sizeof(struct debug_vdev_ctrl));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX];
			ctrl->tx = malloc(sizeof(struct debug_vdev_ctrl_tx));
			if (ctrl->tx)
				memcpy(ctrl->tx, nla_data(attr),
				       sizeof(struct debug_vdev_ctrl_tx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX];
			ctrl->rx = malloc(sizeof(struct debug_vdev_ctrl_rx));
			if (ctrl->rx)
				memcpy(ctrl->rx, nla_data(attr),
				       sizeof(struct debug_vdev_ctrl_rx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_WMI]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_WMI];
			ctrl->wmi = malloc(sizeof(struct debug_vdev_ctrl_wmi));
			if (ctrl->wmi)
				memcpy(ctrl->wmi, nla_data(attr),
				       sizeof(struct debug_vdev_ctrl_wmi));
		}
		obj->stats = ctrl;
		break;
	}
}

static void parse_debug_radio(struct nlattr *rattr, struct stats_obj *obj)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_FEAT_MAX + 1] = {0};
	struct nlattr *attr = NULL;
	struct debug_pdev_data *data = NULL;
	struct debug_pdev_ctrl *ctrl = NULL;

	if (!rattr || nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_FEAT_MAX,
				       rattr, g_policy)) {
		STATS_ERR("NLA Parsing failed\n");
		return;
	}

	switch (obj->type) {
	case STATS_TYPE_DATA:
		if (obj->stats) {
			data = obj->stats;
		} else {
			data = malloc(sizeof(struct debug_pdev_data));
			if (!data)
				return;
			memset(data, 0, sizeof(struct debug_pdev_data));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX];
			data->tx = malloc(sizeof(struct debug_pdev_data_tx));
			if (data->tx)
				memcpy(data->tx, nla_data(attr),
				       sizeof(struct debug_pdev_data_tx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX];
			data->rx = malloc(sizeof(struct debug_pdev_data_rx));
			if (data->rx)
				memcpy(data->rx, nla_data(attr),
				       sizeof(struct debug_pdev_data_rx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_ME]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_ME];
			data->me = malloc(sizeof(struct debug_pdev_data_me));
			if (data->me)
				memcpy(data->me, nla_data(attr),
				       sizeof(struct debug_pdev_data_me));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RAW]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RAW];
			data->raw =
				malloc(sizeof(struct debug_pdev_data_raw));
			if (data->raw)
				memcpy(data->raw, nla_data(attr),
				       sizeof(struct debug_pdev_data_raw));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TSO]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_TSO];
			data->tso =
				malloc(sizeof(struct debug_pdev_data_tso));
			if (data->tso)
				memcpy(data->tso, nla_data(attr),
				       sizeof(struct debug_pdev_data_tso));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_CFR]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_CFR];
			data->cfr = malloc(sizeof(struct debug_pdev_data_cfr));
			if (data->cfr)
				memcpy(data->cfr, nla_data(attr),
				       sizeof(struct debug_pdev_data_cfr));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_HTT]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_HTT];
			data->htt = malloc(sizeof(struct debug_pdev_data_htt));
			if (data->htt)
				memcpy(data->htt, nla_data(attr),
				       sizeof(struct debug_pdev_data_htt));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_WDI]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_WDI];
			data->wdi = malloc(sizeof(struct debug_pdev_data_wdi));
			if (data->wdi)
				memcpy(data->wdi, nla_data(attr),
				       sizeof(struct debug_pdev_data_wdi));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_MESH]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_MESH];
			data->mesh =
				malloc(sizeof(struct debug_pdev_data_mesh));
			if (data->mesh)
				memcpy(data->mesh, nla_data(attr),
				       sizeof(struct debug_pdev_data_mesh));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TXCAP]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_TXCAP];
			data->txcap =
				malloc(sizeof(struct debug_pdev_data_txcap));
			if (data->txcap)
				memcpy(data->txcap, nla_data(attr),
				       sizeof(struct debug_pdev_data_txcap));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_MONITOR]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_MONITOR];
			data->monitor =
				malloc(sizeof(struct debug_pdev_data_monitor));
			if (data->monitor)
				memcpy(data->monitor, nla_data(attr),
				       sizeof(struct debug_pdev_data_monitor));
		}
		obj->stats = data;
		break;
	case STATS_TYPE_CTRL:
		if (obj->stats) {
			ctrl = obj->stats;
		} else {
			ctrl = malloc(sizeof(struct debug_pdev_ctrl));
			if (!ctrl)
				return;
			memset(ctrl, 0, sizeof(struct debug_pdev_ctrl));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX];
			ctrl->tx = malloc(sizeof(struct debug_pdev_ctrl_tx));
			if (ctrl->tx)
				memcpy(ctrl->tx, nla_data(attr),
				       sizeof(struct debug_pdev_ctrl_tx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX];
			ctrl->rx = malloc(sizeof(struct debug_pdev_ctrl_rx));
			if (ctrl->rx)
				memcpy(ctrl->rx, nla_data(attr),
				       sizeof(struct debug_pdev_ctrl_rx));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_WMI]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_WMI];
			ctrl->wmi = malloc(sizeof(struct debug_pdev_ctrl_wmi));
			if (ctrl->wmi)
				memcpy(ctrl->wmi, nla_data(attr),
				       sizeof(struct debug_pdev_ctrl_wmi));
		}
		if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_LINK]) {
			attr = tb[QCA_WLAN_VENDOR_ATTR_FEAT_LINK];
			ctrl->link =
				malloc(sizeof(struct debug_pdev_ctrl_link));
			if (ctrl->link)
				memcpy(ctrl->link, nla_data(attr),
				       sizeof(struct debug_pdev_ctrl_link));
		}
		obj->stats = ctrl;
		break;
	}
}

static void parse_debug_ap(struct nlattr *rattr, struct stats_obj *obj)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_FEAT_MAX + 1] = {0};
	struct debug_psoc_data *data = NULL;

	if (!rattr || nla_parse_nested(tb, QCA_WLAN_VENDOR_ATTR_FEAT_MAX,
				       rattr, g_policy)) {
		STATS_ERR("NLA Parsing failed\n");
		return;
	}

	if (obj->type == STATS_TYPE_CTRL)
		return;

	if (obj->stats) {
		data = obj->stats;
	} else {
		data = malloc(sizeof(struct debug_psoc_data));
		if (!data)
			return;
		memset(data, 0, sizeof(struct debug_psoc_data));
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX]) {
		data->tx = malloc(sizeof(struct debug_psoc_data_tx));
		if (data->tx)
			memcpy(data->tx,
			       nla_data(tb[QCA_WLAN_VENDOR_ATTR_FEAT_TX]),
			       sizeof(struct debug_psoc_data_tx));
	}
	if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX]) {
		data->rx = malloc(sizeof(struct debug_psoc_data_rx));
		if (data->rx)
			memcpy(data->rx,
			       nla_data(tb[QCA_WLAN_VENDOR_ATTR_FEAT_RX]),
			       sizeof(struct debug_psoc_data_rx));
	}
	if (tb[QCA_WLAN_VENDOR_ATTR_FEAT_AST]) {
		data->ast = malloc(sizeof(struct debug_psoc_data_ast));
		if (data->ast)
			memcpy(data->ast,
			       nla_data(tb[QCA_WLAN_VENDOR_ATTR_FEAT_AST]),
			       sizeof(struct debug_psoc_data_ast));
	}

	obj->stats = data;
}
#endif /* WLAN_DEBUG_TELEMETRY */

static void stats_response_handler(struct cfg80211_data *buffer)
{
	bool add_pending;
	struct stats_command *cmd;
	struct reply_buffer *reply;
	struct nlattr *attr;
	struct stats_obj *obj;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_STATS_MAX + 1] = {0};
	struct nla_policy policy[QCA_WLAN_VENDOR_ATTR_STATS_MAX] = {
	[QCA_WLAN_VENDOR_ATTR_STATS_LEVEL] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_STATS_OBJECT] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_STATS_OBJ_ID] = { .type = NLA_UNSPEC },
	[QCA_WLAN_VENDOR_ATTR_STATS_PARENT_IF] = { .type = NLA_UNSPEC },
	[QCA_WLAN_VENDOR_ATTR_STATS_TYPE] = { .type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_STATS_RECURSIVE] = { .type = NLA_NESTED },
	[QCA_WLAN_VENDOR_ATTR_STATS_MULTI_REPLY] = { .type = NLA_FLAG },
	[QCA_WLAN_VENDOR_ATTR_STATS_SERVICEID] = { .type = NLA_U8 },
	};

	if (!buffer) {
		STATS_ERR("Buffer not valid\n");
		return;
	}
	if (!buffer->nl_vendordata) {
		STATS_ERR("Vendor Data is null\n");
		return;
	}
	if (!buffer->data) {
		STATS_ERR("User Data is null\n");
		return;
	}
	cmd = buffer->data;
	reply = cmd->reply;
	if (!reply) {
		STATS_ERR("User reply buffer in null\n");
		return;
	}
	if (nla_parse(tb, QCA_WLAN_VENDOR_ATTR_STATS_MAX,
		      buffer->nl_vendordata, buffer->nl_vendordata_len,
		      policy)) {
		STATS_ERR("NLA Parsing failed\n");
		return;
	}
	if (!tb[QCA_WLAN_VENDOR_ATTR_STATS_LEVEL]) {
		STATS_ERR("NLA Parsing failed for Stats Object\n");
		return;
	}
	if (!tb[QCA_WLAN_VENDOR_ATTR_STATS_OBJECT]) {
		STATS_ERR("NLA Parsing failed for Stats Object\n");
		return;
	}
	if (!tb[QCA_WLAN_VENDOR_ATTR_STATS_OBJ_ID]) {
		STATS_ERR("NLA Parsing failed for Stats Object ID\n");
		return;
	}
	if (!tb[QCA_WLAN_VENDOR_ATTR_STATS_TYPE]) {
		STATS_ERR("NLA Parsing failed for Stats Type\n");
		return;
	}
	if (!tb[QCA_WLAN_VENDOR_ATTR_STATS_RECURSIVE]) {
		STATS_ERR("NLA Parsing failed for Stats Recursive\n");
		return;
	}
	if (!tb[QCA_WLAN_VENDOR_ATTR_STATS_MULTI_REPLY]) {
		obj = allocate_stats_obj();
		add_pending = true;
	} else {
		add_pending = false;
		obj = reply->obj_last;
	}
	if (!obj)
		return;
	obj->lvl = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_STATS_LEVEL]);
	obj->obj_type = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_STATS_OBJECT]);
	obj->type = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_STATS_TYPE]);
	if (tb[QCA_WLAN_VENDOR_ATTR_STATS_PARENT_IF])
		strlcpy(obj->pif_name, nla_get_string(
			tb[QCA_WLAN_VENDOR_ATTR_STATS_PARENT_IF]),
			IFNAME_LEN);
	if (tb[QCA_WLAN_VENDOR_ATTR_STATS_OBJ_ID]) {
		if (obj->obj_type == STATS_OBJ_STA) {
			memcpy(obj->u_id.mac_addr,
			       nla_data(tb[QCA_WLAN_VENDOR_ATTR_STATS_OBJ_ID]),
			       ETH_ALEN);
			if (tb[QCA_WLAN_VENDOR_ATTR_STATS_SERVICEID])
				obj->serviceid = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_STATS_SERVICEID]);
		} else {
			memcpy(obj->u_id.if_name,
			       nla_data(tb[QCA_WLAN_VENDOR_ATTR_STATS_OBJ_ID]),
			       IFNAME_LEN);
		}
	}
	attr = tb[QCA_WLAN_VENDOR_ATTR_STATS_RECURSIVE];
	if (obj->lvl == STATS_LVL_BASIC) {
		switch (obj->obj_type) {
		case STATS_OBJ_STA:
			parse_basic_sta(attr, obj);
			break;
		case STATS_OBJ_VAP:
			parse_basic_vap(attr, obj);
			break;
		case STATS_OBJ_RADIO:
			parse_basic_radio(attr, obj);
			break;
		case STATS_OBJ_AP:
			parse_basic_ap(attr, obj);
			break;
		default:
			STATS_ERR("Unexpected Object\n");
		}
#if WLAN_ADVANCE_TELEMETRY
	} else if (obj->lvl == STATS_LVL_ADVANCE) {
		switch (obj->obj_type) {
		case STATS_OBJ_STA:
			parse_advance_sta(attr, obj);
			break;
		case STATS_OBJ_VAP:
			parse_advance_vap(attr, obj);
			break;
		case STATS_OBJ_RADIO:
			parse_advance_radio(attr, obj);
			break;
		case STATS_OBJ_AP:
			parse_advance_ap(attr, obj);
			break;
		default:
			STATS_ERR("Unexpected Object\n");
		}
#endif /* WLAN_ADVANCE_TELEMETRY */
#if WLAN_DEBUG_TELEMETRY
	} else if (obj->lvl == STATS_LVL_DEBUG) {
		switch (obj->obj_type) {
		case STATS_OBJ_STA:
			parse_debug_sta(attr, obj);
			break;
		case STATS_OBJ_VAP:
			parse_debug_vap(attr, obj);
			break;
		case STATS_OBJ_RADIO:
			parse_debug_radio(attr, obj);
			break;
		case STATS_OBJ_AP:
			parse_debug_ap(attr, obj);
			break;
		default:
			STATS_ERR("Unexpected Object\n");
		}
#endif /* WLAN_DEBUG_TELEMETRY */
	} else {
		STATS_ERR("Level Not Supported!\n");
	}

	/* Add into the list */
	if (add_pending)
		add_stats_obj(reply, obj);
}

static int32_t send_nl_command(struct stats_command *cmd,
			       struct cfg80211_data *buffer,
			       const char *ifname)
{
	struct nl_msg *nlmsg = NULL;
	struct nlattr *nl_ven_data = NULL;
	int32_t ret = 0;

	nlmsg = wifi_cfg80211_prepare_command(&g_sock_ctx.cfg80211_ctxt,
		QCA_NL80211_VENDOR_SUBCMD_TELEMETRIC_DATA, ifname);
	if (nlmsg) {
		nl_ven_data = (struct nlattr *)start_vendor_data(nlmsg);
		if (!nl_ven_data) {
			STATS_ERR("failed to start vendor data\n");
			nlmsg_free(nlmsg);
			return -EIO;
		}
		ret = prepare_request(nlmsg, cmd);
		if (ret) {
			STATS_ERR("failed to prepare stats request\n");
			nlmsg_free(nlmsg);
			return -EINVAL;
		}
		end_vendor_data(nlmsg, nl_ven_data);

		ret = send_nlmsg(&g_sock_ctx.cfg80211_ctxt, nlmsg, buffer);
		if (ret < 0)
			STATS_ERR("Couldn't send NL command, ret = %d\n", ret);
	} else {
		ret = -EINVAL;
		STATS_ERR("Unable to prepare NL command!\n");
	}

	return ret;
}

static void *alloc_object(enum stats_object_e obj_type, char *ifname)
{
	struct object_list *temp_obj = NULL;

	temp_obj = (struct object_list *)malloc(sizeof(struct object_list));
	if (!temp_obj) {
		STATS_ERR("Unable to allocate Memory!\n");
		return NULL;
	}

	temp_obj->nlsent = false;
	temp_obj->next = NULL;
	temp_obj->parent = NULL;
	temp_obj->child = NULL;
	temp_obj->obj_type = obj_type;
	strlcpy(temp_obj->ifname, ifname, IFNAME_LEN);

	return temp_obj;
}

static void free_object_list(struct object_list *temp_obj)
{
	struct object_list *obj_to_free = NULL;

	while (temp_obj) {
		if (temp_obj->child) {
			temp_obj = temp_obj->child;
			continue;
		}
		obj_to_free = temp_obj;
		if (obj_to_free->parent)
			obj_to_free->parent->child = obj_to_free->next;
		if (obj_to_free->next)
			temp_obj = obj_to_free->next;
		else
			temp_obj = obj_to_free->parent;
		obj_to_free->next = NULL;
		obj_to_free->parent = NULL;
		obj_to_free->child = NULL;
		free(obj_to_free);
	}
}

static void get_sta_info(struct cfg80211_data *buffer)
{
	struct ieee80211req_sta_info *si = NULL;
	uint8_t *buf = NULL, *cp = NULL;
	uint32_t remaining_len = 0;
	struct object_list *parent_obj = g_parent_vap_obj;
	struct object_list *temp_obj = NULL;

	buf = buffer->data;
	remaining_len = buffer->length;
	cp = buf;

	while (remaining_len >= sizeof(struct ieee80211req_sta_info)) {
		si = (struct ieee80211req_sta_info *)cp;
		if (!si || !si->isi_len)
			break;
		temp_obj = alloc_object(STATS_OBJ_STA, parent_obj->ifname);
		if (!temp_obj)
			break;
		temp_obj->parent = parent_obj;
		if (!parent_obj->child)
			parent_obj->child = temp_obj;
		else
			g_curr_sta_obj->next = temp_obj;
		g_curr_sta_obj = temp_obj;

		memcpy(g_curr_sta_obj->hw_addr, si->isi_macaddr, ETH_ALEN);

		cp += si->isi_len;
		remaining_len -= si->isi_len;
	}

	buffer->length = LIST_STATION_CFG_ALLOC_SIZE;
}

static int32_t build_child_sta_list(char *ifname,
				    struct object_list *parent_obj)
{
	struct cfg80211_data buffer;
	uint32_t len = 0;
	uint32_t opmode = 0;
	uint8_t *buf = NULL;
	int32_t msg = 0;
	uint16_t cmd = 0;

	buffer.data = &opmode;
	buffer.length = sizeof(uint32_t);
	buffer.parse_data = 0;
	buffer.callback = NULL;
	buffer.parse_data = 0;
	cmd = QCA_NL80211_VENDOR_SUBCMD_WIFI_PARAMS;
	msg = wifi_cfg80211_send_getparam_command(&g_sock_ctx.cfg80211_ctxt,
						  cmd,
						  IEEE80211_PARAM_GET_OPMODE,
						  ifname, (char *)&buffer,
						  sizeof(uint32_t));
	if (msg < 0) {
		STATS_ERR("Couldn't send NL command; %d\n", msg);
		return -EIO;
	}

	if (opmode != IEEE80211_M_HOSTAP)
		return 0;

	memset(&buffer, 0, sizeof(buffer));
	len = sizeof(struct ieee80211req_sta_info) * EXPECTED_MAX_STAS_PERVAP;

	buf = (uint8_t *)malloc(len);
	if (!buf) {
		STATS_ERR("Unable to allocate Memory!\n");
		return -ENOMEM;
	}
	buffer.data = buf;
	buffer.length = LIST_STATION_CFG_ALLOC_SIZE;
	buffer.flags = 0;
	buffer.parse_data = 0;
	buffer.callback = &get_sta_info;
	g_parent_vap_obj = parent_obj;
	cmd = QCA_NL80211_VENDOR_SUBCMD_SET_WIFI_CONFIGURATION;
	wifi_cfg80211_send_generic_command(&g_sock_ctx.cfg80211_ctxt, cmd,
					   QCA_NL80211_VENDOR_SUBCMD_LIST_STA,
					   ifname, (char *)&buffer,
					   buffer.length);

	free(buf);

	return 0;
}

static int32_t build_child_vap_list(struct interface_list *if_list,
				    char *rifname, uint8_t *rhw_addr,
				    struct object_list *parent_obj)
{
	uint8_t inx = 0;
	struct object_list *temp_obj = NULL;
	struct object_list *curr_obj = NULL;
	char *ifname = NULL;

	if (!rifname || !rhw_addr || !parent_obj)
		return -EINVAL;

	for (inx = 0; inx < if_list->v_count; inx++) {
		ifname = if_list->vap[inx].name;
		if (if_list->vap[inx].added)
			continue;
		if (!if_list->vap[inx].active)
			continue;
		if (!is_vap_radiochild(rifname, rhw_addr, ifname,
				       if_list->vap[inx].hw_addr))
			continue;
		temp_obj = alloc_object(STATS_OBJ_VAP, ifname);
		if (!temp_obj) {
			STATS_ERR("Allocation failed for %s object\n", ifname);
			return -EIO;
		}

		if_list->vap[inx].added = true;
		temp_obj->parent = parent_obj;

		if (!parent_obj->child)
			parent_obj->child = temp_obj;
		else if (curr_obj)
			curr_obj->next = temp_obj;
		curr_obj = temp_obj;

		if (build_child_sta_list(ifname, curr_obj))
			return -EIO;
	}

	return 0;
}

static int32_t build_child_radio_list(struct interface_list *if_list,
				      struct soc_ifnames *soc_if,
				      uint8_t if_count,
				      struct object_list *parent_obj)
{
	uint8_t inx = 0;
	uint8_t rinx = 0;
	struct object_list *temp_obj = NULL;
	struct object_list *curr_obj = NULL;
	char *ifname = NULL;

	if (!parent_obj)
		return -EINVAL;

	for (inx = 0; inx < if_count; inx++, soc_if++) {
		ifname = NULL;
		if (!soc_if) {
			STATS_ERR("No valid interface available!\n");
			return -EINVAL;
		}
		for (rinx = 0; rinx < if_list->r_count; rinx++) {
			if (!strncmp(if_list->radio[rinx].name, soc_if->ifname,
				     IFNAME_LEN)) {
				ifname = if_list->radio[rinx].name;
				break;
			}
		}
		if (!ifname)
			continue;
		if (if_list->radio[rinx].added)
			continue;
		temp_obj = alloc_object(STATS_OBJ_RADIO, ifname);
		if (!temp_obj) {
			STATS_ERR("Allocation failed for %s object\n", ifname);
			return -EIO;
		}

		temp_obj->parent = parent_obj;
		if_list->radio[rinx].added = true;
		if (!parent_obj->child)
			parent_obj->child = temp_obj;
		else if (curr_obj)
			curr_obj->next = temp_obj;
		curr_obj = temp_obj;

		if (build_child_vap_list(if_list, ifname,
					 if_list->radio[rinx].hw_addr,
					 curr_obj))
			return -EIO;
	}

	return 0;
}

static void *build_object_list(struct stats_command *cmd)
{
	struct interface_list if_list = {0};
	struct soc_ifnames soc_if[MAX_RADIO_NUM] = {0};
	struct object_list *req_obj_list = NULL;
	struct object_list *temp_obj = NULL;
	struct object_list *curr_obj = NULL;
	char *ifname = NULL;
	int32_t ret = 0;
	uint8_t inx = 0;
	uint8_t sinx = 0;
	uint8_t loop_count = 0;
	uint8_t if_count = 0;

	ret = fetch_all_interfaces(&if_list);
	if (ret < 0) {
		STATS_ERR("Unable to fetch Interfaces!\n");
		free_interface_list(&if_list);
		return NULL;
	}

	/* Check Radio is active or not and get HW address */
	for (inx = 0; inx < if_list.r_count; inx++) {
		ifname = if_list.radio[inx].name;
		if (!is_interface_active(ifname, STATS_OBJ_RADIO)) {
			if_list.radio[inx].active = false;
			continue;
		}
		if_list.radio[inx].active = true;
		get_hw_address(ifname, if_list.radio[inx].hw_addr);
	}

	/* Check Vap is active or not and get HW address */
	for (inx = 0; inx < if_list.v_count; inx++) {
		ifname = if_list.vap[inx].name;
		if (!is_interface_active(ifname, STATS_OBJ_VAP)) {
			if_list.vap[inx].active = false;
			continue;
		}
		if_list.vap[inx].active = true;
		get_hw_address(ifname, if_list.vap[inx].hw_addr);
	}

	/**
	 * If user specifies soc, then build object hierarchy only for
	 * that particular soc.
	 */
	if ((cmd->obj == STATS_OBJ_AP) && cmd->if_name[0]) {
		for (sinx = 0; sinx < if_list.s_count; sinx++) {
			if (!strncmp(cmd->if_name,
			    if_list.soc[sinx].name, 4)) {
				loop_count = sinx + 1;
				break;
			}
		}
	} else {
		loop_count = if_list.s_count;
	}
	for (inx = sinx; inx < loop_count; inx++) {
		if (get_active_radio_intf_for_soc(&if_list, soc_if,
						  if_list.soc[inx].name,
						  &if_count) || !if_count)
			continue;
		temp_obj = alloc_object(STATS_OBJ_AP, soc_if[0].ifname);
		if (!temp_obj) {
			STATS_ERR("Allocation failed for %s object\n",
				  if_list.soc[inx].name);
			ret = -1;
			break;
		}
		if (!req_obj_list)
			req_obj_list = temp_obj;
		else if (curr_obj)
			curr_obj->next = temp_obj;
		curr_obj = temp_obj;

		if (build_child_radio_list(&if_list, soc_if, if_count,
					   curr_obj)) {
			ret = -1;
			break;
		}
		if_count = 0;
		memset(soc_if, 0, (MAX_RADIO_NUM * sizeof(struct soc_ifnames)));
	}

	free_interface_list(&if_list);
	if ((ret < 0) && req_obj_list) {
		STATS_ERR("Failed to build Object List! Clean existing\n");
		free_object_list(req_obj_list);
		req_obj_list = NULL;
	}

	return req_obj_list;
}

static struct object_list *find_head_object(struct stats_command *cmd,
					    struct object_list *obj_list)
{
	struct object_list *temp_obj = NULL;
	struct object_list *ap_obj = NULL;
	struct object_list *radio_obj = NULL;
	struct object_list *vap_obj = NULL;

	temp_obj = obj_list;

	switch (cmd->obj) {
	case STATS_OBJ_AP:
		break;
	case STATS_OBJ_RADIO:
		while (temp_obj) {
			if (temp_obj->obj_type != STATS_OBJ_RADIO) {
				if (temp_obj->child)
					temp_obj = temp_obj->child;
				else
					temp_obj = temp_obj->next;
				continue;
			}
			if (!strncmp(temp_obj->ifname, cmd->if_name, 5))
				break;
			if (temp_obj->next)
				temp_obj = temp_obj->next;
			else
				temp_obj = temp_obj->parent->next;
		}
		break;
	case STATS_OBJ_VAP:
		while (temp_obj) {
			if (temp_obj->obj_type == STATS_OBJ_VAP) {
				if (!strncmp(temp_obj->ifname, cmd->if_name,
					     IFNAME_LEN))
					break;
			} else if (temp_obj->child) {
				if (temp_obj->obj_type == STATS_OBJ_AP)
					ap_obj = temp_obj;
				else
					radio_obj = temp_obj;
				temp_obj = temp_obj->child;
				continue;
			}
			if (temp_obj->next)
				temp_obj = temp_obj->next;
			else if (radio_obj && radio_obj->next)
				temp_obj = radio_obj->next;
			else if (ap_obj)
				temp_obj = ap_obj->next;
			else
				temp_obj = NULL;
		}
		break;
	case STATS_OBJ_STA:
		while (temp_obj) {
			if (temp_obj->obj_type == STATS_OBJ_STA) {
				if (!memcmp(temp_obj->hw_addr,
					    cmd->sta_mac.ether_addr_octet,
					    ETH_ALEN))
					break;
			} else if (temp_obj->child) {
				if (temp_obj->obj_type == STATS_OBJ_AP)
					ap_obj = temp_obj;
				else if (temp_obj->obj_type == STATS_OBJ_RADIO)
					radio_obj = temp_obj;
				else
					vap_obj = temp_obj;
				temp_obj = temp_obj->child;
				continue;
			}
			if (temp_obj->next)
				temp_obj = temp_obj->next;
			else if (vap_obj && vap_obj->next)
				temp_obj = vap_obj->next;
			else if (radio_obj && radio_obj->next)
				temp_obj = radio_obj->next;
			else if (ap_obj)
				temp_obj = ap_obj->next;
			else
				temp_obj = NULL;
		}
		break;
	default:
		STATS_ERR("Invalid Object Type %d!\n", cmd->obj);
	}

	return temp_obj;
}

static bool is_feat_valid_for_obj(struct stats_command *cmd)
{
	switch (cmd->lvl) {
	case STATS_LVL_BASIC:
		switch (cmd->obj) {
		case STATS_OBJ_STA:
			if ((cmd->type == STATS_TYPE_DATA) &&
			    (cmd->feat_flag & STATS_BASIC_STA_DATA_MASK))
				return true;
			if ((cmd->type == STATS_TYPE_CTRL) &&
			    (cmd->feat_flag & STATS_BASIC_STA_CTRL_MASK))
				return true;
			break;
		case STATS_OBJ_VAP:
			if ((cmd->type == STATS_TYPE_DATA) &&
			    (cmd->feat_flag & STATS_BASIC_VAP_DATA_MASK))
				return true;
			if ((cmd->type == STATS_TYPE_CTRL) &&
			    (cmd->feat_flag & STATS_BASIC_VAP_CTRL_MASK))
				return true;
			break;
		case STATS_OBJ_RADIO:
			if ((cmd->type == STATS_TYPE_DATA) &&
			    (cmd->feat_flag & STATS_BASIC_RADIO_DATA_MASK))
				return true;
			if ((cmd->type == STATS_TYPE_CTRL) &&
			    (cmd->feat_flag & STATS_BASIC_RADIO_CTRL_MASK))
				return true;
			break;
		case STATS_OBJ_AP:
			if ((cmd->type == STATS_TYPE_DATA) &&
			    (cmd->feat_flag & STATS_BASIC_AP_DATA_MASK))
				return true;
			if ((cmd->type == STATS_TYPE_CTRL) &&
			    (cmd->feat_flag & STATS_BASIC_AP_CTRL_MASK))
				return true;
			break;
		default:
			STATS_ERR("Invalid obj %d!\n", cmd->obj);
		}
		break;
#if WLAN_ADVANCE_TELEMETRY
	case STATS_LVL_ADVANCE:
		switch (cmd->obj) {
		case STATS_OBJ_STA:
			if ((cmd->type == STATS_TYPE_DATA) &&
			    (cmd->feat_flag & STATS_ADVANCE_STA_DATA_MASK))
				return true;
			if ((cmd->type == STATS_TYPE_CTRL) &&
			    (cmd->feat_flag & STATS_ADVANCE_STA_CTRL_MASK))
				return true;
			break;
		case STATS_OBJ_VAP:
			if ((cmd->type == STATS_TYPE_DATA) &&
			    (cmd->feat_flag & STATS_ADVANCE_VAP_DATA_MASK))
				return true;
			if ((cmd->type == STATS_TYPE_CTRL) &&
			    (cmd->feat_flag & STATS_ADVANCE_VAP_CTRL_MASK))
				return true;
			break;
		case STATS_OBJ_RADIO:
			if ((cmd->type == STATS_TYPE_DATA) &&
			    (cmd->feat_flag & STATS_ADVANCE_RADIO_DATA_MASK))
				return true;
			if ((cmd->type == STATS_TYPE_CTRL) &&
			    (cmd->feat_flag & STATS_ADVANCE_RADIO_CTRL_MASK))
				return true;
			break;
		case STATS_OBJ_AP:
			if ((cmd->type == STATS_TYPE_DATA) &&
			    (cmd->feat_flag & STATS_ADVANCE_AP_DATA_MASK))
				return true;
			if ((cmd->type == STATS_TYPE_CTRL) &&
			    (cmd->feat_flag & STATS_ADVANCE_AP_CTRL_MASK))
				return true;
			break;
		default:
			STATS_ERR("Invalid obj %d!\n", cmd->obj);
		}
		break;
#endif /* WLAN_ADVANCE_TELEMETRY */
#if WLAN_DEBUG_TELEMETRY
	case STATS_LVL_DEBUG:
		switch (cmd->obj) {
		case STATS_OBJ_STA:
			if ((cmd->type == STATS_TYPE_DATA) &&
			    (cmd->feat_flag & STATS_DEBUG_STA_DATA_MASK))
				return true;
			if ((cmd->type == STATS_TYPE_CTRL) &&
			    (cmd->feat_flag & STATS_DEBUG_STA_CTRL_MASK))
				return true;
			break;
		case STATS_OBJ_VAP:
			if ((cmd->type == STATS_TYPE_DATA) &&
			    (cmd->feat_flag & STATS_DEBUG_VAP_DATA_MASK))
				return true;
			if ((cmd->type == STATS_TYPE_CTRL) &&
			    (cmd->feat_flag & STATS_DEBUG_VAP_CTRL_MASK))
				return true;
			break;
		case STATS_OBJ_RADIO:
			if ((cmd->type == STATS_TYPE_DATA) &&
			    (cmd->feat_flag & STATS_DEBUG_RADIO_DATA_MASK))
				return true;
			if ((cmd->type == STATS_TYPE_CTRL) &&
			    (cmd->feat_flag & STATS_DEBUG_RADIO_CTRL_MASK))
				return true;
			break;
		case STATS_OBJ_AP:
			if ((cmd->type == STATS_TYPE_DATA) &&
			    (cmd->feat_flag & STATS_DEBUG_AP_DATA_MASK))
				return true;
			if ((cmd->type == STATS_TYPE_CTRL) &&
			    (cmd->feat_flag & STATS_DEBUG_AP_CTRL_MASK))
				return true;
			break;
		default:
			STATS_ERR("Invalid obj %d!\n", cmd->obj);
		}
		break;
#endif /* WLAN_DEBUG_TELEMETRY */
	default:
		STATS_ERR("Invalid Level %d!\n", cmd->lvl);
	}

	return false;
}

static int32_t send_request_per_object(struct stats_command *user_cmd,
				       struct object_list *obj_list)
{
	struct cfg80211_data buffer;
	struct stats_command cmd;
	struct object_list *temp_obj = NULL;
	struct object_list *root_obj = NULL;
	int32_t ret = 0;

	if (!obj_list) {
		STATS_ERR("Invalid Object List\n");
		return -EINVAL;
	}

	/**
	 * Based on user request find the requested subtree in root_obj.
	 **/
	root_obj = find_head_object(user_cmd, obj_list);
	if (!root_obj) {
		STATS_ERR("No object found for %d obj type\n", user_cmd->obj);
		return -EPERM;
	}
	temp_obj = root_obj;

	memcpy(&cmd, user_cmd, sizeof(struct stats_command));
	cmd.recursive = user_cmd->recursive;
	memset(&buffer, 0, sizeof(struct cfg80211_data));
	buffer.data = &cmd;
	buffer.length = sizeof(struct stats_command);
	buffer.callback = stats_response_handler;
	buffer.parse_data = 1;

	while (temp_obj) {
		if (!temp_obj->nlsent) {
			temp_obj->nlsent = true;
			cmd.obj = temp_obj->obj_type;
			if (temp_obj->obj_type == STATS_OBJ_STA)
				memcpy(cmd.sta_mac.ether_addr_octet,
				       temp_obj->hw_addr, ETH_ALEN);
			if (is_feat_valid_for_obj(&cmd)) {
				ret = send_nl_command(&cmd, &buffer,
						      temp_obj->ifname);
				if (ret < 0)
					break;
			}
		}
		if (!cmd.recursive)
			break;
		if (temp_obj->child && !temp_obj->child->nlsent)
			temp_obj = temp_obj->child;
		else if (temp_obj->next)
			temp_obj = temp_obj->next;
		else
			temp_obj = temp_obj->parent;

		/**
		 * Request is not for All stats.
		 * So, break from here as we only need stats of this subtree.
		 **/
		if ((root_obj->obj_type != STATS_OBJ_AP) &&
		    (root_obj->obj_type <= temp_obj->obj_type))
			break;
	}

	return ret;
}

static int32_t process_and_send_stats_request(struct stats_command *cmd)
{
	struct object_list *req_obj_list = NULL;
	int32_t ret = 0;

	if (!g_sock_ctx.cfg80211) {
		STATS_ERR("cfg80211 interface is not enabled\n");
		return -EPERM;
	}

	if (is_valid_cmd(cmd)) {
		STATS_ERR("Invalid command\n");
		return -EINVAL;
	}

	req_obj_list = build_object_list(cmd);
	if (!req_obj_list) {
		STATS_ERR("Failed to build Object Hierarchy!\n");
		return -EPERM;
	}

	ret = send_request_per_object(cmd, req_obj_list);
	free_object_list(req_obj_list);

	return ret;
}

static void free_basic_sta(struct stats_obj *sta)
{
	switch (sta->type) {
	case STATS_TYPE_DATA:
		{
			struct basic_peer_data *data = sta->stats;

			if (data->tx)
				free(data->tx);
			if (data->rx)
				free(data->rx);
			if (data->link)
				free(data->link);
			if (data->rate)
				free(data->rate);
		}
		break;
	case STATS_TYPE_CTRL:
		{
			struct basic_peer_ctrl *ctrl = sta->stats;

			if (ctrl->tx)
				free(ctrl->tx);
			if (ctrl->rx)
				free(ctrl->rx);
			if (ctrl->link)
				free(ctrl->link);
			if (ctrl->rate)
				free(ctrl->rate);
		}
		break;
	default:
		STATS_ERR("Unexpected Type %d!\n", sta->type);
	}

	free(sta->stats);
}

static void free_basic_vap(struct stats_obj *vap)
{
	switch (vap->type) {
	case STATS_TYPE_DATA:
		{
			struct basic_vdev_data *data = vap->stats;

			if (data->tx)
				free(data->tx);
			if (data->rx)
				free(data->rx);
		}
		break;
	case STATS_TYPE_CTRL:
		{
			struct basic_vdev_ctrl *ctrl = vap->stats;

			if (ctrl->tx)
				free(ctrl->tx);
			if (ctrl->rx)
				free(ctrl->rx);
		}
		break;
	default:
		STATS_ERR("Unexpected Type %d!\n", vap->type);
	}

	free(vap->stats);
}

static void free_basic_radio(struct stats_obj *radio)
{
	switch (radio->type) {
	case STATS_TYPE_DATA:
		{
			struct basic_pdev_data *data = radio->stats;

			if (data->tx)
				free(data->tx);
			if (data->rx)
				free(data->rx);
		}
		break;
	case STATS_TYPE_CTRL:
		{
			struct basic_pdev_ctrl *ctrl = radio->stats;

			if (ctrl->tx)
				free(ctrl->tx);
			if (ctrl->rx)
				free(ctrl->rx);
			if (ctrl->link)
				free(ctrl->link);
		}
		break;
	default:
		STATS_ERR("Unexpected Type %d!\n", radio->type);
	}

	free(radio->stats);
}

static void free_basic_ap(struct stats_obj *ap)
{
	struct basic_psoc_data *data = ap->stats;

	if (!data)
		return;

	if (data->tx)
		free(data->tx);
	if (data->rx)
		free(data->rx);

	free(ap->stats);
}

static void update_basic_pdev_ctrl_tx(struct basic_pdev_ctrl_tx *pdev_tx,
				      struct basic_vdev_ctrl_tx *vdev_tx)
{
	pdev_tx->cs_tx_mgmt += vdev_tx->cs_tx_mgmt;
}

static void update_basic_pdev_ctrl_rx(struct basic_pdev_ctrl_rx *pdev_rx,
				      struct basic_vdev_ctrl_rx *vdev_rx)
{
	pdev_rx->cs_rx_mgmt += vdev_rx->cs_rx_mgmt;
}

static void update_basic_pdev_ctrl_stats(struct basic_pdev_ctrl *pdev_ctrl,
					 struct basic_vdev_ctrl *vdev_ctrl)
{
	if (pdev_ctrl->tx && vdev_ctrl->tx)
		update_basic_pdev_ctrl_tx(pdev_ctrl->tx, vdev_ctrl->tx);
	if (pdev_ctrl->rx && vdev_ctrl->rx)
		update_basic_pdev_ctrl_rx(pdev_ctrl->rx, vdev_ctrl->rx);
}

static void update_basic_pdev_stats(struct stats_obj *radio,
				    struct stats_obj *vap)
{
	if (!radio->stats || !vap->stats)
		return;

	switch (radio->type) {
	case STATS_TYPE_DATA:
		break;
	case STATS_TYPE_CTRL:
		update_basic_pdev_ctrl_stats(radio->stats, vap->stats);
		break;
	default:
		STATS_ERR("Unexpected stats type %d!\n", radio->type);
	}
}

#if WLAN_ADVANCE_TELEMETRY
static void free_advance_sta(struct stats_obj *sta)
{
	if (!sta->stats)
		return;

	switch (sta->type) {
	case STATS_TYPE_DATA:
		{
			struct advance_peer_data *data = sta->stats;

			if (data->tx)
				free(data->tx);
			if (data->rx)
				free(data->rx);
			if (data->fwd)
				free(data->fwd);
			if (data->raw)
				free(data->raw);
			if (data->twt)
				free(data->twt);
			if (data->link)
				free(data->link);
			if (data->rate)
				free(data->rate);
			if (data->nawds)
				free(data->nawds);
			if (data->delay)
				free(data->delay);
			if (data->jitter)
				free(data->jitter);
			if (data->sawfdelay)
				free(data->sawfdelay);
			if (data->sawftx)
				free(data->sawftx);
		}
		break;
	case STATS_TYPE_CTRL:
		{
			struct advance_peer_ctrl *ctrl = sta->stats;

			if (ctrl->tx)
				free(ctrl->tx);
			if (ctrl->rx)
				free(ctrl->rx);
			if (ctrl->twt)
				free(ctrl->twt);
			if (ctrl->link)
				free(ctrl->link);
			if (ctrl->rate)
				free(ctrl->rate);
		}
		break;
	default:
		STATS_ERR("Unexpected Type %d!\n", sta->type);
	}

	free(sta->stats);
}

static void free_advance_vap(struct stats_obj *vap)
{
	if (!vap->stats)
		return;

	switch (vap->type) {
	case STATS_TYPE_DATA:
		{
			struct advance_vdev_data *data = vap->stats;

			if (data->tx)
				free(data->tx);
			if (data->rx)
				free(data->rx);
			if (data->me)
				free(data->me);
			if (data->raw)
				free(data->raw);
			if (data->tso)
				free(data->tso);
			if (data->igmp)
				free(data->igmp);
			if (data->mesh)
				free(data->mesh);
			if (data->nawds)
				free(data->nawds);
		}
		break;
	case STATS_TYPE_CTRL:
		{
			struct advance_vdev_ctrl *ctrl = vap->stats;

			if (ctrl->tx)
				free(ctrl->tx);
			if (ctrl->rx)
				free(ctrl->rx);
		}
		break;
	default:
		STATS_ERR("Unexpected Type %d!\n", vap->type);
	}

	free(vap->stats);
}

static void free_advance_radio(struct stats_obj *radio)
{
	if (!radio->stats)
		return;

	switch (radio->type) {
	case STATS_TYPE_DATA:
		{
			struct advance_pdev_data *data = radio->stats;

			if (data->tx)
				free(data->tx);
			if (data->rx)
				free(data->rx);
			if (data->me)
				free(data->me);
			if (data->raw)
				free(data->raw);
			if (data->tso)
				free(data->tso);
			if (data->igmp)
				free(data->igmp);
			if (data->mesh)
				free(data->mesh);
			if (data->nawds)
				free(data->nawds);
		}
		break;
	case STATS_TYPE_CTRL:
		{
			struct advance_pdev_ctrl *ctrl = radio->stats;

			if (ctrl->tx)
				free(ctrl->tx);
			if (ctrl->rx)
				free(ctrl->rx);
			if (ctrl->link)
				free(ctrl->link);
		}
		break;
	default:
		STATS_ERR("Unexpected Type %d!\n", radio->type);
	}

	free(radio->stats);
}

static void free_advance_ap(struct stats_obj *ap)
{
	struct advance_psoc_data *data = ap->stats;

	if (!data)
		return;

	if (data->tx)
		free(data->tx);
	if (data->rx)
		free(data->rx);

	free(ap->stats);
}

static void update_advance_pdev_ctrl_stats(struct advance_pdev_ctrl *pdev_ctrl,
					   struct advance_vdev_ctrl *vdev_ctrl)
{
	if (pdev_ctrl->tx && vdev_ctrl->tx) {
		pdev_ctrl->tx->cs_tx_beacon +=
					vdev_ctrl->tx->cs_tx_bcn_success +
					vdev_ctrl->tx->cs_tx_bcn_outage;
		update_basic_pdev_ctrl_tx(&pdev_ctrl->tx->b_tx,
					  &vdev_ctrl->tx->b_tx);
	}
	if (pdev_ctrl->rx && vdev_ctrl->rx) {
		update_basic_pdev_ctrl_rx(&pdev_ctrl->rx->b_rx,
					  &vdev_ctrl->rx->b_rx);
	}
}

static void update_advance_pdev_stats(struct stats_obj *radio,
				      struct stats_obj *vap)
{
	if (!radio->stats || !vap->stats)
		return;

	switch (radio->type) {
	case STATS_TYPE_DATA:
		break;
	case STATS_TYPE_CTRL:
		update_advance_pdev_ctrl_stats(radio->stats, vap->stats);
		break;
	default:
		STATS_ERR("Unexpected stats type %d!\n", radio->type);
	}
}
#endif /* WLAN_ADVANCE_TELEMETRY */

#if WLAN_DEBUG_TELEMETRY
static void free_debug_sta(struct stats_obj *sta)
{
	if (!sta->stats)
		return;

	switch (sta->type) {
	case STATS_TYPE_DATA:
		{
			struct debug_peer_data *data = sta->stats;

			if (data->tx)
				free(data->tx);
			if (data->rx)
				free(data->rx);
			if (data->link)
				free(data->link);
			if (data->rate)
				free(data->rate);
			if (data->txcap)
				free(data->txcap);
		}
		break;
	case STATS_TYPE_CTRL:
		{
			struct debug_peer_ctrl *ctrl = sta->stats;

			if (ctrl->tx)
				free(ctrl->tx);
			if (ctrl->rx)
				free(ctrl->rx);
			if (ctrl->link)
				free(ctrl->link);
			if (ctrl->rate)
				free(ctrl->rate);
		}
		break;
	default:
		STATS_ERR("Unexpected Type %d!\n", sta->type);
	}

	free(sta->stats);
}

static void free_debug_vap(struct stats_obj *vap)
{
	if (!vap->stats)
		return;

	switch (vap->type) {
	case STATS_TYPE_DATA:
		{
			struct debug_vdev_data *data = vap->stats;

			if (data->tx)
				free(data->tx);
			if (data->rx)
				free(data->rx);
			if (data->me)
				free(data->me);
			if (data->raw)
				free(data->raw);
			if (data->tso)
				free(data->tso);
		}
		break;
	case STATS_TYPE_CTRL:
		{
			struct debug_vdev_ctrl *ctrl = vap->stats;

			if (ctrl->tx)
				free(ctrl->tx);
			if (ctrl->rx)
				free(ctrl->rx);
			if (ctrl->wmi)
				free(ctrl->wmi);
		}
		break;
	default:
		STATS_ERR("Unexpected Type %d!\n", vap->type);
	}

	free(vap->stats);
}

static void free_debug_radio(struct stats_obj *radio)
{
	if (!radio->stats)
		return;

	switch (radio->type) {
	case STATS_TYPE_DATA:
		{
			struct debug_pdev_data *data = radio->stats;

			if (data->tx)
				free(data->tx);
			if (data->rx)
				free(data->rx);
			if (data->me)
				free(data->me);
			if (data->raw)
				free(data->raw);
			if (data->tso)
				free(data->tso);
			if (data->cfr)
				free(data->cfr);
			if (data->htt)
				free(data->htt);
			if (data->wdi)
				free(data->wdi);
			if (data->mesh)
				free(data->mesh);
			if (data->txcap)
				free(data->txcap);
			if (data->monitor)
				free(data->monitor);
		}
		break;
	case STATS_TYPE_CTRL:
		{
			struct debug_pdev_ctrl *ctrl = radio->stats;

			if (ctrl->tx)
				free(ctrl->tx);
			if (ctrl->rx)
				free(ctrl->rx);
			if (ctrl->wmi)
				free(ctrl->wmi);
			if (ctrl->link)
				free(ctrl->link);
		}
		break;
	default:
		STATS_ERR("Unexpected Type %d!\n", radio->type);
	}

	free(radio->stats);
}

static void free_debug_ap(struct stats_obj *ap)
{
	struct debug_psoc_data *data = ap->stats;

	if (!data)
		return;

	if (data->tx)
		free(data->tx);
	if (data->rx)
		free(data->rx);
	if (data->ast)
		free(data->ast);

	free(ap->stats);
}

static void update_debug_pdev_ctrl_stats(struct debug_pdev_ctrl *pdev_ctrl,
					 struct debug_vdev_ctrl *vdev_ctrl)
{
	if (pdev_ctrl->tx && vdev_ctrl->tx) {
		update_basic_pdev_ctrl_tx(&pdev_ctrl->tx->b_tx,
					  &vdev_ctrl->tx->b_tx);
	}
	if (pdev_ctrl->rx && vdev_ctrl->rx) {
		update_basic_pdev_ctrl_rx(&pdev_ctrl->rx->b_rx,
					  &vdev_ctrl->rx->b_rx);
	}
}

static void update_debug_pdev_stats(struct stats_obj *radio,
				    struct stats_obj *vap)
{
	if (!radio->stats || !vap->stats)
		return;

	switch (radio->type) {
	case STATS_TYPE_DATA:
		break;
	case STATS_TYPE_CTRL:
		update_debug_pdev_ctrl_stats(radio->stats, vap->stats);
		break;
	default:
		STATS_ERR("Unexpected stats type %d!\n", radio->type);
	}
}
#endif /* WLAN_DEBUG_TELEMETRY */

static void aggregate_radio_stats(struct stats_obj *radio)
{
	if (!radio)
		return;

	switch (radio->lvl) {
	case STATS_LVL_BASIC:
		break;
#if WLAN_ADVANCE_TELEMETRY
	case STATS_LVL_ADVANCE:
		break;
#endif /* WLAN_ADVANCE_TELEMETRY */
#if WLAN_DEBUG_TELEMETRY
	case STATS_LVL_DEBUG:
		break;
#endif /* WLAN_DEBUG_TELEMETRY */
	default:
		STATS_ERR("Unexpected Level %d!\n", radio->lvl);
	}
}

static void aggregate_vap_stats(struct stats_obj *vap)
{
	if (!vap || !vap->parent)
		return;

	switch (vap->lvl) {
	case STATS_LVL_BASIC:
		update_basic_pdev_stats(vap->parent, vap);
		break;
#if WLAN_ADVANCE_TELEMETRY
	case STATS_LVL_ADVANCE:
		update_advance_pdev_stats(vap->parent, vap);
		break;
#endif /* WLAN_ADVANCE_TELEMETRY */
#if WLAN_DEBUG_TELEMETRY
	case STATS_LVL_DEBUG:
		update_debug_pdev_stats(vap->parent, vap);
		break;
#endif /* WLAN_DEBUG_TELEMETRY */
	default:
		STATS_ERR("Unexpected Level %d!\n", vap->lvl);
	}

	if (!vap->next || (vap->next->obj_type != STATS_OBJ_VAP))
		aggregate_radio_stats(vap->parent);
}

static void aggregate_sta_stats(struct stats_obj *sta)
{
	if (!sta || !sta->parent)
		return;

	switch (sta->lvl) {
	case STATS_LVL_BASIC:
		break;
#if WLAN_ADVANCE_TELEMETRY
	case STATS_LVL_ADVANCE:
		break;
#endif /* WLAN_ADVANCE_TELEMETRY */
#if WLAN_DEBUG_TELEMETRY
	case STATS_LVL_DEBUG:
		break;
#endif /* WLAN_DEBUG_TELEMETRY */
	default:
		STATS_ERR("Unexpected Level %d!\n", sta->lvl);
	}

	if (!sta->next || (sta->next->obj_type != STATS_OBJ_STA))
		aggregate_vap_stats(sta->parent);
}

static void accumulate_stats(struct stats_command *cmd)
{
	struct stats_obj *obj = NULL;

	if (!cmd->recursive || !cmd->reply)
		return;

	obj = cmd->reply->obj_head;

	while (obj) {
		switch (obj->obj_type) {
		case STATS_OBJ_STA:
			aggregate_sta_stats(obj);
			break;
		case STATS_OBJ_VAP:
			/* No STA connected to this vap */
			if (!obj->next ||
			    (obj->next->obj_type != STATS_OBJ_STA))
				aggregate_vap_stats(obj);
			break;
		case STATS_OBJ_RADIO:
			/* No active VAP for this radio */
			if (!obj->next ||
			    (obj->next->obj_type != STATS_OBJ_VAP))
				aggregate_radio_stats(obj);
			break;
		case STATS_OBJ_AP:
		default:
			break;
		}

		obj = obj->next;
	}
}

static void free_sta(struct stats_obj *sta)
{
	switch (sta->lvl) {
	case STATS_LVL_BASIC:
		free_basic_sta(sta);
		break;
#if WLAN_ADVANCE_TELEMETRY
	case STATS_LVL_ADVANCE:
		free_advance_sta(sta);
		break;
#endif /* WLAN_ADVANCE_TELEMETRY */
#if WLAN_DEBUG_TELEMETRY
	case STATS_LVL_DEBUG:
		free_debug_sta(sta);
		break;
#endif /* WLAN_DEBUG_TELEMETRY */
	default:
		STATS_ERR("Unexpected Level %d!\n", sta->lvl);
	}
	free(sta);
}

static void free_vap(struct stats_obj *vap)
{
	switch (vap->lvl) {
	case STATS_LVL_BASIC:
		free_basic_vap(vap);
		break;
#if WLAN_ADVANCE_TELEMETRY
	case STATS_LVL_ADVANCE:
		free_advance_vap(vap);
		break;
#endif /* WLAN_ADVANCE_TELEMETRY */
#if WLAN_DEBUG_TELEMETRY
	case STATS_LVL_DEBUG:
		free_debug_vap(vap);
		break;
#endif /* WLAN_DEBUG_TELEMETRY */
	default:
		STATS_ERR("Unexpected Level %d!\n", vap->lvl);
	}
	free(vap);
}

static void free_radio(struct stats_obj *radio)
{
	switch (radio->lvl) {
	case STATS_LVL_BASIC:
		free_basic_radio(radio);
		break;
#if WLAN_ADVANCE_TELEMETRY
	case STATS_LVL_ADVANCE:
		free_advance_radio(radio);
		break;
#endif /* WLAN_ADVANCE_TELEMETRY */
#if WLAN_DEBUG_TELEMETRY
	case STATS_LVL_DEBUG:
		free_debug_radio(radio);
		break;
#endif /* WLAN_DEBUG_TELEMETRY */
	default:
		STATS_ERR("Unexpected Level %d!\n", radio->lvl);
	}
	free(radio);
}

static void free_ap(struct stats_obj *ap)
{
	switch (ap->lvl) {
	case STATS_LVL_BASIC:
		free_basic_ap(ap);
		break;
#if WLAN_ADVANCE_TELEMETRY
	case STATS_LVL_ADVANCE:
		free_advance_ap(ap);
		break;
#endif /* WLAN_ADVANCE_TELEMETRY */
#if WLAN_DEBUG_TELEMETRY
	case STATS_LVL_DEBUG:
		free_debug_ap(ap);
		break;
#endif /* WLAN_DEBUG_TELEMETRY */
	default:
		STATS_ERR("Unexpected Level %d!\n", ap->lvl);
	}
	free(ap);
}

void libstats_free_reply_buffer(struct stats_command *cmd)
{
	struct stats_obj *obj;
	struct reply_buffer *reply = cmd->reply;

	if (!reply || !reply->obj_head)
		return;

	while (reply->obj_head) {
		obj = reply->obj_head;
		reply->obj_head = obj->next;
		switch (obj->obj_type) {
		case STATS_OBJ_STA:
			free_sta(obj);
			break;
		case STATS_OBJ_VAP:
			free_vap(obj);
			break;
		case STATS_OBJ_RADIO:
			free_radio(obj);
			break;
		case STATS_OBJ_AP:
			free_ap(obj);
			break;
		}
	}
	reply->obj_last = NULL;
	free(reply);
	cmd->reply = NULL;
}

u_int64_t libstats_get_feature_flag(char *feat_flags)
{
	u_int64_t feats = 0;
	u_int8_t index = 0;
	char *flag = NULL;
	bool found = false;

	flag = strtok_r(feat_flags, ",", &feat_flags);
	while (flag) {
		found = false;
		for (index = 0; g_feat[index].name; index++) {
			if (!strncmp(flag, g_feat[index].name, strlen(flag)) &&
			    (strlen(flag) == strlen(g_feat[index].name))) {
				found = true;
				/* ALL is specified in Feature list */
				if (!index)
					return STATS_FEAT_FLG_ALL;
				feats |= g_feat[index].id;
				break;
			}
		}
		if (!found)
			STATS_WARN("%s not in supported list!\n", flag);
		flag = strtok_r(NULL, ",", &feat_flags);
	}

	return feats;
}

int32_t libstats_request_handle(struct stats_command *cmd)
{
	int32_t ret = 0;

	if (!cmd) {
		STATS_ERR("Invalid Input\n");
		return -EINVAL;
	}

	ret = stats_lib_init();
	if (ret)
		return ret;

	ret = process_and_send_stats_request(cmd);

	stats_lib_deinit();

	if (!ret)
		accumulate_stats(cmd);

	return ret;
}

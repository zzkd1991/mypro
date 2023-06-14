u8_t memp_memory_name_base[LWIP_MEM_ALIGN_BUFFER((num) * (MEMP_SIZE + MEMP_ALIGN_SIZE(size))))]
static struct stats_mem memp_stats_name;
static struct memp *memp_tab_name;
const struct memp_desc memp_name = {
	desc,&memp_stats_name,LWIP_MEM_ALIGN_SIZE(size),num, memp_memory_name_base,&memp_tab_name
};

u8_t memp_memory_RAW_PCB_base[LWIP_MEM_ALIGN_BUFFER((MEMP_NUM_RAW_PCB) * (MEMP_SIZE + MEMP_ALIGN_SIZE(size)))];
u8_t memp_memory_UDP_PCB_base[LWIP_MEM_ALIGN_BUFFER((MEMP_NUM_UDP_PCB) * (MEMP_SIZE + MEMP_ALIGN_SIZE(size)))];
u8_t memp_memory_TCP_PCB_base[LWIP_MEM_ALIGN_BUFFER((MEMP_NUM_TCP_PCB) * (MEMP_SIZE + MEMP_ALIGN_SIZE(size)))];
u8_t memp_memory_TCP_PCB_LISTEN_base[LWIP_MEM_ALIGN_BUFFER((MEMP_NUM_TCP_PCB_LISTEN) * (MEMP_SIZE + MEMP_ALIGN_SIZE(size)))];

static struct stats_mem memp_stats_RAW_PCB;
static struct stats_mem memp_stats_UDP_PCB;
static struct stats_mem memp_stats_TCP_PCB;
static struct stats_mem memp_stats_TCP_PCB_LISTEN;

static struct memp *memp_tab_RAW_PCB;
static struct memp *memp_tab_UDP_PCB;
static struct memp *memp_tab_TCP_PCB;
static struct memp *memp_tab_TCP_PCB_LISTEN;

const struct memp_desc memp_RAW_PCB = {
	"RAW_PCB", &memp_stats_RAW_PCB, LWIP_MEM_ALIGN_SIZE(sizeof(struct raw_pcb)), MEMP_NUM_RAW_PCB, memp_memory_RAW_PCB_base, &memp_tab_RAW_PCB
};

const struct memp_desc memp_UDP_PCB = {
	"UDP_PCB", &memp_stats_UDP_PCB, LWIP_MEM_ALIGN_SIZE(sizeof(struct udp_pcb)), MEMP_NUM_UDP_PCB, memp_memory_UDP_PCB_base, memp_tab_UDP_PCB
};



typedef enum {
    /* Get the first (via:
       MEMP_POOL_HELPER_START = ((u8_t) 1*MEMP_POOL_A + 0*MEMP_POOL_B + 0*MEMP_POOL_C + 0)*/
    MEMP_POOL_HELPER_FIRST = ((u8_t)
#define LWIP_MEMPOOL(name,num,size,desc)
#define LWIP_MALLOC_MEMPOOL_START 1
#define LWIP_MALLOC_MEMPOOL(num, size) * MEMP_POOL_##size + 0
#define LWIP_MALLOC_MEMPOOL_END
#include "lwip/priv/memp_std.h"
    ) ,
    /* Get the last (via:
       MEMP_POOL_HELPER_END = ((u8_t) 0 + MEMP_POOL_A*0 + MEMP_POOL_B*0 + MEMP_POOL_C*1) */
    MEMP_POOL_HELPER_LAST = ((u8_t)
#define LWIP_MEMPOOL(name,num,size,desc)
#define LWIP_MALLOC_MEMPOOL_START
#define LWIP_MALLOC_MEMPOOL(num, size) 0 + MEMP_POOL_##size *
#define LWIP_MALLOC_MEMPOOL_END 1
#include "lwip/priv/memp_std.h"
    )
} memp_pool_helper_t;

lwippools.h定义
LWIP_MALLOC_MEMPOOL_START LWIP_MALLOC_MEMPOOL(20, 256) LWIP_MALLOC_MEMPOOL(10, 512) LWIP_MALLOC_MEMPOOL(5, 1512) LWIP_MALLOC_MEMPOOL_END

MEMP_POOL_HELPER_FIRST = ((uint_8) 1 * MEMP_POOL_256 + 0 * MEMP_POOL_512 + 0 * MEMP_POOL_1512)
MEMP_POOL_HELPER_LAST = ((uint_8) 0 + MEMP_POOL_256 * 0 + MEMP_POOL_512 * 0 + MEMP_POOL_1512 * 1)


const struct memp_desc* const memp_pools[MEMP_MAX] = {
	&memp_POOL_size,
	&memp_name,
	&memp_RAW_PCB,
	&memp_UDP_PCB,
	&memp_TCP_PCB,
	&memp_TCP_PCB_LISTEN,
	&memp_TCP_SEG,
	&memp_REASSDATA,
	&memp_FRAG_PBUF,
	&memp_NETBUF,
	&memp_NETCOMM,
	&memp_TCPIP_MSG_API,
	&memp_API_MGS,
	&memp_DNS_API_MSG,
	&memp_SOCKET_SETGETSOCKOPT_DATA,
	&memp_NETIFAPI_MSG,
	&memp_TCPIP_MSG_INPKT,
	&memp_ARP_QUEUE,
	&memp_IGMP_GROUP,
	&memp_SYS_TIMEOUT,
	&memp_NETDB,
	&memp_LOCALHOSTLIST,
	&memp_ND6_QUEUE,
	&memp_IP6_REASSDATA,
	&memp_MLD6_GROUP,
};






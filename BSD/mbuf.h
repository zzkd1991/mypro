struct m_hdr {
	struct mbuf *mh_next;
	struct mbuf *mh_nextpkt;
	int mh_len;
	caddr_t mh_data;
	short mh_type;
	short mh_flags;
};

struct pkthdr {
	int len;
	struct ifnet *rcvif;
};

struct m_ext{
	caddr_t ext_buf;
	void (*ext_free)();
	u_int ext_size;
};

struct mbuf {
	struct m_hdr m_hdr;
	union{
		struct {
			struct pkthdr MH_pkthdr;
			union {
				struct m_ext MH_ext;
				char MH_databuf[MHLEN];
			}MH_dat;
		}MH;
		char M_databuf[MLEN];
	}M_dat;
};
#define m_next		m_hdr.mh_next
#define m_len		m_hdr.mh_len
#define m_data  	m_hdr.mh_data
#define m_type		m_hdr.mh_type
#define m_flags		m_hdr.mh_flags
#define m_nextpkt	m_hdr.mh_nextpkt
#define m_act		m_nextpkt
#define m_pkthdr	M_dat.MH.MH_pkthdr
#define m_ext		M_dat.MH.MH_dat.MH_ext
#define m_pktdat	M_dat.MH.MH_dat.MH_databuf
#define m_dat		M_dat.M_databuf






#define MGET(m, how, type) { \
	MALLOC((m), struct mbuf *, MSIZE, mbytes[type], (how)); \
	if(m) { \
		(m)->m_type = (type); \
		MBUFLOCK(mbstat.m_mtypes[type]++;) \
		(m)->m_next = (struct mbuf *)NULL;   \
		(m)->m_nextpkt = (struct mbuf *)NULL; \
		(m)->m_data = (m)->m_dat; \
		(m)->m_flags = 0; \
	} else \
		(m) = m_retry((how), (type));	\
}

#define MGETHDR(m, how, type){ \
	MALLOC((m), struct mbuf *, MSIZE, mbytes[type], (how));	\
	if(m){ \
		(m)->m_type = (type); \
		MBUFLOCK(mbstat.m_mtypes[type]++;) \
		(m)->m_next = (struct mbuf *)NULL; \
		(m)->m_data = (m)->m_pktdat;	\
		(m)->m_flags = M_PKTHDR;	\
	} else \
		(m) = m_retryhdr((how), (type));	\
}

#define M_PREPEND(m, plen, how){	\
	if(M_LEADINGSPACE(m) >= (plen))	{	\
		(m)->m_data -= (plen);	\
		(m)->m_len += (plen);	\
	}else \
		(m) = m_prepend((m), (plen), (how));	\
	if((m) && (m)->m_flags & M_PKTHDR)	\
		(m)->m_pkthdr.len += (plen);	\
	
}

#define m_copy(m ,o, l)		m_copym((m), (o), (l), M_DONTWAIT)



















































































































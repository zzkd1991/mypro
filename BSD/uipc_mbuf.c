

struct mbuf *
m_prepend(m, len, how)
	register struct mbuf *m;
	int len, how;
{
		struct mbuf *mn;
		
		MGET(mn, how, m->m_type);
		if(mn == (struct mbuf *)NULL){
			m_free(m);
			return ((struct mbuf *)NULL);
		}
		if(m->m_flags & M_PKTHDR){
			M_COPY_PKTHDR(mn, m);
			m->m_flags &= ~M_PKTHDR;
		}
		mn->m_next = m;
		m = nm;
		if(len < MHLEN)
			MH_ALIGN(m, len);
		return m;
}

struct mbuf *
m_copym(m, off0, len, wait)
	register struct mbuf *m;
	int off0, wait;
	register int len;
{
	register struct mbuf *n, **np;
	register int off = off0;
	struct mbuf *top;
	int copyhdr = 0;
	
	if(off < 0 || len <0)
		panic("m_copym");
	if(off == 0 && m->m_flags & M_PKTHDR)
		copyhdr = 1;
	while(off > 0) {
		if(m == 0)
			panic("m_copym");
		if(off < m_len)
			break;
		off -= m->m_len;
		m = m->m_next;
	}
	np = &top;
	top = 0;
	while(len > 0){
		if(m == 0){
			if(len != M_COPYALL)
				panic("m_copym");
			break;
		}
		MGET(n, wait, m->m_type);
		*np = n;
		if(n == 0)
			goto nospace;
		if(copyhdr){
			M_COPY_PKTHDR(n, m);
			if(len == M_COPYALL)
				n->m_pkthdr.len -= off0;
			else
				n->m_pkthdr.len = len;
			copyhdr = 0;
		}
		
		n->m_len = min(len, m->len - off0);
		if(m->m_flags & M_EXT){
			n->m_data = m->m_data + off;
			mclrefcnt[mtocl(m->m_ext.ext_buf)]++;
			n->m_ext = m->m_ext;
			n->m_flags |= M_EXT;
		} else
			bcopy(mtod(m, caddr_t)+off, mtod(n, caddr_t), (unsigned)n->m_len);
		if(len != M_COPYALL)
			len -= n->m_len;
		off = 0;
		m = m->m_next;
		np = &n->m_next;
	}
	if(top == 0)
		MCFail++;
	return (top);
nospace:
	m_freem(top);
	MCFail++;
	return (0);
	
}

m_copydata(m, off, len, cp)
	register struct mbuf *m;
	register int off;
	register int len;
	caddr_t cp;
{
	register unsigned count;
	
	if(off < 0 || len < 0)
		panic("m_copydata");
	while(off > 0){
		if(m == 0)
			panic("m_copydata");
		if(off < m->m_len)
			break;
		off -= m—>m_len;
		m = m->m_next;
	}
	while(len > 0){
		if(m == 0)
			panic("m_copydata");
		count = min(m->m_len - off, len);
		bcopy(mtod(m, caddr_t) + off, cp, count);
		len -= count;
		cp += count;
		off = 0;
		m = m->m_next;
	}
}










































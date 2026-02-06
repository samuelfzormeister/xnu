/*
 * Copyright (c) 2016-2021 Apple Inc. All rights reserved.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_START@
 *
 * This file contains Original Code and/or Modifications of Original Code
 * as defined in and that are subject to the Apple Public Source License
 * Version 2.0 (the 'License'). You may not use this file except in
 * compliance with the License. The rights granted to you under the License
 * may not be used to create, or enable the creation or redistribution of,
 * unlawful or unlicensed copies of an Apple operating system, or to
 * circumvent, violate, or enable the circumvention or violation of, any
 * terms of an Apple operating system software license agreement.
 *
 * Please obtain a copy of the License at
 * http://www.opensource.apple.com/apsl/ and read it before using this file.
 *
 * The Original Code and all software distributed under the License are
 * distributed on an 'AS IS' basis, WITHOUT WARRANTY OF ANY KIND, EITHER
 * EXPRESS OR IMPLIED, AND APPLE HEREBY DISCLAIMS ALL SUCH WARRANTIES,
 * INCLUDING WITHOUT LIMITATION, ANY WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE, QUIET ENJOYMENT OR NON-INFRINGEMENT.
 * Please see the License for the specific language governing rights and
 * limitations under the License.
 *
 * @APPLE_OSREFERENCE_LICENSE_HEADER_END@
 */

#include <skywalk/os_skywalk_private.h>
#include <netinet/tcp_var.h>

#if (DEBUG || DEVELOPMENT)
__attribute__((noreturn))
void
pkt_subtype_assert_fail(const kern_packet_t ph, uint64_t type, uint64_t subtype)
{
	panic("invalid packet handle 0x%llx (type %llu != %llu || "
	    "subtype %llu != %llu)", ph, SK_PTR_TYPE(ph), type,
	    SK_PTR_SUBTYPE(ph), subtype);
	/* NOTREACHED */
	__builtin_unreachable();
}

__attribute__((noreturn))
void
pkt_type_assert_fail(const kern_packet_t ph, uint64_t type)
{
	panic("invalid packet handle 0x%llx (type %llu != %llu)",
	    ph, SK_PTR_TYPE(ph), type);
	/* NOTREACHED */
	__builtin_unreachable();
}
#endif /* DEBUG || DEVELOPMENT */

errno_t
kern_packet_set_headroom(const kern_packet_t ph, const uint8_t headroom)
{
	return __packet_set_headroom(ph, headroom);
}

uint8_t
kern_packet_get_headroom(const kern_packet_t ph)
{
	return __packet_get_headroom(ph);
}

errno_t
kern_packet_set_link_header_offset(const kern_packet_t ph, const uint8_t off)
{
	return __packet_set_headroom(ph, off);
}

uint16_t
kern_packet_get_link_header_offset(const kern_packet_t ph)
{
	return __packet_get_headroom(ph);
}

errno_t
kern_packet_set_link_header_length(const kern_packet_t ph, const uint8_t off)
{
	return __packet_set_link_header_length(ph, off);
}

uint8_t
kern_packet_get_link_header_length(const kern_packet_t ph)
{
	return __packet_get_link_header_length(ph);
}

errno_t
kern_packet_set_link_broadcast(const kern_packet_t ph)
{
	return __packet_set_link_broadcast(ph);
}

boolean_t
kern_packet_get_link_broadcast(const kern_packet_t ph)
{
	return __packet_get_link_broadcast(ph);
}

errno_t
kern_packet_set_link_multicast(const kern_packet_t ph)
{
	return __packet_set_link_multicast(ph);
}

errno_t
kern_packet_set_link_ethfcs(const kern_packet_t ph)
{
	return __packet_set_link_ethfcs(ph);
}

boolean_t
kern_packet_get_link_multicast(const kern_packet_t ph)
{
	return __packet_get_link_multicast(ph);
}

boolean_t
kern_packet_get_link_ethfcs(const kern_packet_t ph)
{
	return __packet_get_link_ethfcs(ph);
}

/* deprecated -- no effect, use set_link_header_length instead  */
errno_t
kern_packet_set_network_header_offset(const kern_packet_t ph,
    const uint16_t off)
{
#pragma unused(ph, off)
	return 0;
}

/* deprecated -- use get_link_header_length instead  */
uint16_t
kern_packet_get_network_header_offset(const kern_packet_t ph)
{
	return (uint16_t)__packet_get_headroom(ph) +
	       (uint16_t)__packet_get_link_header_length(ph);
}

/* deprecated */
errno_t
kern_packet_set_transport_header_offset(const kern_packet_t ph,
    const uint16_t off)
{
#pragma unused(ph, off)
	return 0;
}

/* deprecated */
uint16_t
kern_packet_get_transport_header_offset(const kern_packet_t ph)
{
#pragma unused(ph)
	return 0;
}

boolean_t
kern_packet_get_transport_traffic_background(const kern_packet_t ph)
{
	return __packet_get_transport_traffic_background(ph);
}

boolean_t
kern_packet_get_transport_traffic_realtime(const kern_packet_t ph)
{
	return __packet_get_transport_traffic_realtime(ph);
}

boolean_t
kern_packet_get_transport_retransmit(const kern_packet_t ph)
{
	return __packet_get_transport_retransmit(ph);
}

boolean_t
kern_packet_get_transport_new_flow(const kern_packet_t ph)
{
	return __packet_get_transport_new_flow(ph);
}

boolean_t
kern_packet_get_transport_last_packet(const kern_packet_t ph)
{
	return __packet_get_transport_last_packet(ph);
}

int
kern_packet_set_service_class(const kern_packet_t ph,
    const kern_packet_svc_class_t sc)
{
	return __packet_set_service_class(ph, sc);
}

kern_packet_svc_class_t
kern_packet_get_service_class(const kern_packet_t ph)
{
	return __packet_get_service_class(ph);
}

errno_t
kern_packet_get_service_class_index(const kern_packet_svc_class_t svc,
    uint32_t *index)
{
	if (index == NULL || !KPKT_VALID_SVC(svc)) {
		return EINVAL;
	}

	*index = KPKT_SVCIDX(svc);
	return 0;
}

errno_t
kern_packet_set_traffic_class(const kern_packet_t ph,
    kern_packet_traffic_class_t tc)
{
	return __packet_set_traffic_class(ph, tc);
}

kern_packet_traffic_class_t
kern_packet_get_traffic_class(const kern_packet_t ph)
{
	return __packet_get_traffic_class(ph);
}

errno_t
kern_packet_set_inet_checksum(const kern_packet_t ph,
    const packet_csum_flags_t flags, const uint16_t start,
    const uint16_t stuff)
{
	return __packet_set_inet_checksum(ph, flags, start, stuff, FALSE);
}

packet_csum_flags_t
kern_packet_get_inet_checksum(const kern_packet_t ph, uint16_t *start,
    uint16_t *val)
{
	return __packet_get_inet_checksum(ph, start, val, TRUE);
}

void
kern_packet_set_flow_uuid(const kern_packet_t ph, const uuid_t flow_uuid)
{
	__packet_set_flow_uuid(ph, flow_uuid);
}

void
kern_packet_get_flow_uuid(const kern_packet_t ph, uuid_t *flow_uuid)
{
	__packet_get_flow_uuid(ph, *flow_uuid);
}

void
kern_packet_clear_flow_uuid(const kern_packet_t ph)
{
	__packet_clear_flow_uuid(ph);
}

void
kern_packet_get_euuid(const kern_packet_t ph, uuid_t euuid)
{
	if (__probable(SK_PTR_TYPE(ph) == NEXUS_META_TYPE_PACKET)) {
		uuid_copy(euuid, PKT_ADDR(ph)->pkt_policy_euuid);
	} else {
		uuid_clear(euuid);
	}
}

void
kern_packet_set_policy_id(const kern_packet_t ph, uint32_t policy_id)
{
	if (__probable(SK_PTR_TYPE(ph) == NEXUS_META_TYPE_PACKET)) {
		PKT_ADDR(ph)->pkt_policy_id = policy_id;
	}
}

uint32_t
kern_packet_get_policy_id(const kern_packet_t ph)
{
	if (__probable(SK_PTR_TYPE(ph) == NEXUS_META_TYPE_PACKET)) {
		return PKT_ADDR(ph)->pkt_policy_id;
	} else {
		return 0;
	}
}

uint32_t
kern_packet_get_data_length(const kern_packet_t ph)
{
	return __packet_get_data_length(ph);
}

errno_t
kern_packet_set_buflet_count(const kern_packet_t ph, uint32_t bcnt)
{
    return __packet_set_buflet_count(ph, bcnt);
}

uint32_t
kern_packet_get_buflet_count(const kern_packet_t ph)
{
	return __packet_get_buflet_count(ph);
}

kern_buflet_t
kern_packet_get_next_buflet(const kern_packet_t ph, const kern_buflet_t bprev)
{
	return __packet_get_next_buflet(ph, bprev);
}

errno_t
kern_packet_finalize(const kern_packet_t ph)
{
	return __packet_finalize(ph);
}

kern_packet_idx_t
kern_packet_get_object_index(const kern_packet_t ph)
{
	return __packet_get_object_index(ph);
}

errno_t
kern_packet_get_timestamp(const kern_packet_t ph, uint64_t *ts,
    boolean_t *valid)
{
	return __packet_get_timestamp(ph, ts, valid);
}

errno_t
kern_packet_set_timestamp(const kern_packet_t ph, uint64_t ts, boolean_t valid)
{
	return __packet_set_timestamp(ph, ts, valid);
}

struct mbuf *
kern_packet_get_mbuf(const kern_packet_t pkt)
{
	struct __kern_packet *kpkt = SK_PTR_ADDR_KPKT(pkt);

	if ((kpkt->pkt_pflags & PKT_F_MBUF_DATA) != 0) {
		return kpkt->pkt_mbuf;
	}
	return NULL;
}

errno_t
kern_packet_get_timestamp_requested(const kern_packet_t ph,
    boolean_t *requested)
{
	return __packet_get_timestamp_requested(ph, requested);
}

void
kern_packet_tx_completion(const kern_packet_t ph, ifnet_t ifp)
{
	uint64_t ts;
	uintptr_t cb_arg, cb_data;
	kern_return_t tx_status;
	struct __kern_packet *kpkt = SK_PTR_ADDR_KPKT(ph);

	PKT_TYPE_ASSERT(ph, NEXUS_META_TYPE_PACKET);
	(void) __packet_get_tx_completion_status(ph, &tx_status);
	if (tx_status != KERN_SUCCESS) {
		(void) kern_channel_event_transmit_status(ph, ifp);
	}
	if ((kpkt->pkt_pflags & PKT_F_TX_COMPL_TS_REQ) == 0) {
		return;
	}
	__packet_get_tx_completion_data(ph, &cb_arg, &cb_data);
	__packet_get_timestamp(ph, &ts, NULL);
	while (kpkt->pkt_tx_compl_callbacks != 0) {
		mbuf_tx_compl_func cb;
		uint32_t i;

		i = ffs(kpkt->pkt_tx_compl_callbacks) - 1;
		kpkt->pkt_tx_compl_callbacks &= ~(1 << i);
		cb = m_get_tx_compl_callback(i);
		if (__probable(cb != NULL)) {
			cb(kpkt->pkt_tx_compl_context, ifp, ts, cb_arg, cb_data,
			    tx_status);
		}
	}
}

errno_t
kern_packet_get_tx_completion_status(const kern_packet_t ph,
    kern_return_t *status)
{
	return __packet_get_tx_completion_status(ph, status);
}

errno_t
kern_packet_set_tx_completion_status(const kern_packet_t ph,
    kern_return_t status)
{
	return __packet_set_tx_completion_status(ph, status);
}

void
kern_packet_set_group_start(const kern_packet_t ph)
{
	(void) __packet_set_group_start(ph);
}

boolean_t
kern_packet_get_group_start(const kern_packet_t ph)
{
	return __packet_get_group_start(ph);
}

void
kern_packet_set_group_end(const kern_packet_t ph)
{
	(void) __packet_set_group_end(ph);
}

boolean_t
kern_packet_get_group_end(const kern_packet_t ph)
{
	return __packet_get_group_end(ph);
}

errno_t
kern_packet_get_expire_time(const kern_packet_t ph, uint64_t *ts)
{
	return __packet_get_expire_time(ph, ts);
}

errno_t
kern_packet_set_expire_time(const kern_packet_t ph, const uint64_t ts)
{
	return __packet_set_expire_time(ph, ts);
}

errno_t
kern_packet_get_token(const kern_packet_t ph, void *token, uint16_t *len)
{
	return __packet_get_token(ph, token, len);
}

errno_t
kern_packet_set_token(const kern_packet_t ph, const void *token,
    const uint16_t len)
{
	return __packet_set_token(ph, token, len);
}

errno_t
kern_packet_get_packetid(const kern_packet_t ph, packet_id_t *pktid)
{
	return __packet_get_packetid(ph, pktid);
}

errno_t
kern_packet_set_vlan_tag(const kern_packet_t ph, const uint16_t tag,
    const boolean_t tag_in_pkt)
{
	return __packet_set_vlan_tag(ph, tag, tag_in_pkt);
}

errno_t
kern_packet_get_vlan_tag(const kern_packet_t ph, uint16_t *tag,
    boolean_t *tag_in_pkt)
{
	return __packet_get_vlan_tag(ph, tag, tag_in_pkt);
}

uint16_t
kern_packet_get_vlan_id(const uint16_t tag)
{
	return __packet_get_vlan_id(tag);
}

uint8_t
kern_packet_get_vlan_priority(const uint16_t tag)
{
	return __packet_get_vlan_priority(tag);
}

uint32_t
kern_inet_checksum(const void *data, uint32_t len, uint32_t sum0)
{
	return __packet_cksum(data, len, sum0);
}

uint32_t
kern_copy_and_inet_checksum(const void *src, void *dst, uint32_t len,
    uint32_t sum0)
{
	uint32_t sum = __packet_copy_and_sum(src, dst, len, sum0);
	return __packet_fold_sum_final(sum);
}

/*
 * Source packet must be finalized (not dropped);
 *
 * Also, the cloned packet is finalized. For some reason.
 */
kern_packet_t
kern_packet_clone(const kern_packet_t ph1, uint32_t skmflag)
{
    struct kern_pbufpool *pool;
	struct __kern_packet *p1 = SK_PTR_ADDR_KPKT(ph1);
	struct __kern_packet *p2 = NULL;
	struct __kern_buflet *p1_buf, *p2_buf;
	uint8_t *saddr, *daddr;
	uint16_t copy_len;
	kern_packet_t ph2;
	uint16_t bufs_cnt_alloc;
	int m_how;
	int err;

	/* TODO: Add quantum support */
	VERIFY(SK_PTR_TYPE(ph1) == NEXUS_META_TYPE_PACKET);

	/* Source needs to be finalized (not dropped) and with 1 buflet */
	if (__improbable((p1->pkt_qum.qum_qflags & QUM_F_FINALIZED) == 0 ||
	    (p1->pkt_qum.qum_qflags & QUM_F_DROPPED) != 0 ||
	    p1->pkt_bufs_cnt == 0)) {
		return EINVAL;
	}

	/* TODO: Add multi-buflet support */
	VERIFY(p1->pkt_bufs_cnt == 1);

	bufs_cnt_alloc = p1->pkt_bufs_cnt;

	pool = __DECONST(struct kern_pbufpool *, SK_PTR_ADDR_KQUM(ph1)->qum_pp);
	if (skmflag & SKMEM_NOSLEEP) {
		err = kern_pbufpool_alloc_nosleep(pool, bufs_cnt_alloc, &ph2);
		m_how = M_NOWAIT;
	} else {
		err = kern_pbufpool_alloc(pool, bufs_cnt_alloc, &ph2);
		ASSERT(err != ENOMEM);
		m_how = M_WAIT;
	}
	if (__improbable(err != 0)) {
		/* See comments above related to KPKT_COPY_{HEAVY,LIGHT} */
		goto error;
	}
	p2 = SK_PTR_ADDR_KPKT(p2);

	/* Copy packet metadata */
	_QUM_COPY(&(p1)->pkt_qum, &(p2)->pkt_qum);
	_PKT_COPY(p1, p2);
	ASSERT(p2->pkt_mbuf == NULL);

	/* Copy AQM metadata */
	p2->pkt_flowsrc_type = p1->pkt_flowsrc_type;
	p2->pkt_flowsrc_fidx = p1->pkt_flowsrc_fidx;
	_CASSERT((offsetof(struct __flow, flow_src_id) % 8) == 0);
	_UUID_COPY(p2->pkt_flowsrc_id, p1->pkt_flowsrc_id);
	_UUID_COPY(p2->pkt_policy_euuid, p1->pkt_policy_euuid);
	p2->pkt_policy_id = p1->pkt_policy_id;

	ASSERT(p1->pkt_bufs_cnt == 1);

	/* clear finalized and classified bits from clone */
	p2->pkt_qum.qum_qflags &= ~(QUM_F_FINALIZED | QUM_F_FLOW_CLASSIFIED);

	if (METADATA_TYPE(p2) == NEXUS_META_TYPE_PACKET) {
		PKT_GET_FIRST_BUFLET(p1, p1->pkt_bufs_cnt, p1_buf);
		PKT_GET_FIRST_BUFLET(p2, p2->pkt_bufs_cnt, p2_buf);
	} else {
	    ASSERT(METADATA_TYPE(p2) == NEXUS_META_TYPE_QUANTUM);
		p1_buf = &p1->pkt_qum_buf;
		p2_buf = &p2->pkt_qum_buf;
	}

	saddr = (void *)p1_buf->buf_addr;
	daddr = (void *)p2_buf->buf_addr;
	copy_len = MIN(p1_buf->buf_dlen, p1_buf->buf_dlim);
	*__DECONST(uint16_t *, &p2_buf->buf_dlim) = p1_buf->buf_dlim;
	p2_buf->buf_dlen = p1_buf->buf_dlen;
	p2_buf->buf_doff = p1_buf->buf_doff;

	p2->pkt_pflags = p1->pkt_pflags;
	if (p1->pkt_pflags & PKT_F_MBUF_DATA) {
		ASSERT(p1->pkt_mbuf != NULL);
		p2->pkt_mbuf = m_dup(p1->pkt_mbuf, m_how);
		if (p2->pkt_mbuf == NULL) {
			KPKT_CLEAR_MBUF_DATA(p2);
			err = ENOBUFS;
			goto error;
		}
	}

	/* SAMUEL ZORMEISTER: lovely. */
	err = __packet_finalize(ph2);

   error:
	if (err != 0 && p2 != NULL) {
		ASSERT(p2->pkt_mbuf == NULL);
		kern_pbufpool_free(pool, ph2);
		ph2 = 0;
	}

	ASSERT(err == 0);

	return ph2;
}

errno_t
kern_buflet_set_data_offset(const kern_buflet_t buf, const uint16_t doff)
{
	return __buflet_set_data_offset(buf, doff);
}

uint16_t
kern_buflet_get_data_offset(const kern_buflet_t buf)
{
	return __buflet_get_data_offset(buf);
}

errno_t
kern_buflet_set_data_length(const kern_buflet_t buf, const uint16_t dlen)
{
	return __buflet_set_data_length(buf, dlen);
}

uint16_t
kern_buflet_get_data_length(const kern_buflet_t buf)
{
	return __buflet_get_data_length(buf);
}

void *
kern_buflet_get_object_address(const kern_buflet_t buf)
{
	return __buflet_get_object_address(buf);
}

kern_segment_t
kern_buflet_get_object_segment(const kern_buflet_t buf,
    kern_obj_idx_seg_t *idx)
{
	return __buflet_get_object_segment(buf, idx);
}

mach_vm_offset_t
kern_buflet_get_object_offset(const kern_buflet_t buf)
{
    return __buflet_get_object_offset(buf);
}

uint16_t
kern_buflet_get_data_limit(const kern_buflet_t buf)
{
	return __buflet_get_data_limit(buf);
}

errno_t kern_buflet_attach_buffer(const kern_buflet_t buflet, mach_vm_address_t baddr)
{
    errno_t err;
    struct skmem_obj_info oib;

    ASSERT(buflet != NULL && baddr != 0 && buflet->buf_qoff);
}

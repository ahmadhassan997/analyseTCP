#include "Connection.h"
#include "ByteRange.h"
#include "util.h"
#include "color_print.h"


/**
 * This function generates the relative sequence number of packets read from pcap files.
 *
 * seq:                The sequence number of the packet
 * firstSeq:           The first sequence number in the stream
 * largestSeq:         The largest relative sequence number that has been read for this stream
 * largestSeqAbsolute: The largest absolute (raw) sequence number that has been read for this stream
 *
 * Returns the relative sequence number or std::numeric_limits<ulong>::max() if it failed.
 **/
seq64_t getRelativeSequenceNumber(seq32_t seq, seq32_t firstSeq, seq64_t largestSeq, seq32_t largestSeqAbsolute, Connection *conn) {

#ifdef DEBUG
	bool debug = false;

	if (debug)
		fprintf(stderr, "getRelativeSequenceNumber!\n");
#endif

	ullint_t wrap_index;
	seq64_t seq_relative;
	wrap_index = firstSeq + largestSeq;
	wrap_index += 1;

#ifdef DEBUG
	if (debug)
		printf("getRelativeSequenceNumber: seq: %u, firstSeq: %u, largestSeq: %llu, largestSeqAbsolute: %u, wrap_index: %llu\n", seq, firstSeq, largestSeq, largestSeqAbsolute, wrap_index);
#endif

	// Either seq has wrapped, or a retrans (or maybe reorder if netem is run on sender machine)
	if (seq < largestSeqAbsolute) {
		// This is an earlier sequence number
		if (before(seq, largestSeqAbsolute)) {
			if (before(seq, firstSeq)) {
				return std::numeric_limits<ulong>::max();
				//printf("Before first!\n");
			}
			wrap_index -= (largestSeqAbsolute - seq);
		}
		// Sequence number has wrapped
		else {
			wrap_index += (0 - largestSeqAbsolute) + seq;
		}
	}
	// When seq is greater, it is either newer data, or it is older data because
	// largestSeqAbsolute just wrapped. E.g. largestSeqAbsolute == 10, and seq = 4294967250
	else {
#ifdef DEBUG
		if (debug) {
			printf("JAU 3\n");
			printf("largestSeqAbsolute: %u\n", largestSeqAbsolute);
			printf("seq: %u\n", seq);
		}
#endif
		//printf("wrap_index: %lu\n", wrap_index);
		// This is newer seq
		if (after_or_equal(largestSeqAbsolute, seq)) {
			//printf("after_or_equal\n");
			wrap_index += (seq - largestSeqAbsolute);
			//printf("new wrap_index: %lu\n", wrap_index);
		}
		// Acks older data than largestAckSeqAbsolute, largestAckSeqAbsolute has wrapped.
		else {
#ifdef DEBUG
			if (debug) {
				printf("JAU 5\n");
				printf("seq: %u\n", (seq));
				printf("0 - seq: %u\n", (0 - seq));
				printf("largestSeqAbsolute: %u\n", largestSeqAbsolute);
				printf("wrap_index: %llu\n", wrap_index);
				printf("wrap_index -= %u\n", ((0 - seq) + largestSeqAbsolute));
				printf("wrap_index -= %u\n", (largestSeqAbsolute - seq));
				printf("wrap_index <- %llu\n", wrap_index - ((0 - seq) + largestSeqAbsolute));
			}
#endif
			wrap_index -= ((0 - seq) + largestSeqAbsolute);
		}
	}
	// ullint_t wrap_index_tmp = wrap_index;

	// 4294967295
	wrap_index /= 4294967296L;
	// When seq has wrapped, wrap_index will make sure the relative sequence number continues to grow
	seq_relative = seq + (wrap_index * 4294967296L) - firstSeq;
	if (seq_relative > 9999999999) {// TODO: Do a better check than this, e.g. checking for distance of largestSeq and seq_relative > a large number
		// use stderr for error messages for crying out loud!!!!!
		//fprintf(stderr, "wrap_index_tmp: %llu\n", wrap_index_tmp);
		//fprintf(stderr, "wrap_index: %llu\n", wrap_index);
		//fprintf(stderr, "\ngetRelativeSequenceNumber: seq: %u, firstSeq: %u, largestSeq: %llu, largestSeqAbsolute: %u\n", seq, firstSeq, largestSeq, largestSeqAbsolute);
		//fprintf(stderr, "seq_relative: %llu\n", seq_relative);
		//fprintf(stderr, "Conn: %s\n", conn->getConnKey().c_str());

#if !defined(NDEBUG)
		fprintf(stderr, "Encountered invalid sequence number for connection %s: seq_relative: %lld, seq: %u (firstSeq=%u, largestSeq=%llu, largestSeqAbsolute=%u\n",
				conn->getConnKey().c_str(),
				seq_relative,
				seq,
				firstSeq,
				largestSeq,
				largestSeqAbsolute);
#endif

		assert(0 && "Incorrect sequence number calculation!\n");
		return std::numeric_limits<ulong>::max();
	}
	//printf("RETURN seq_relative: %llu\n", seq_relative);
	return seq_relative;
}

seq64_t Connection::getRelativeSequenceNumber(seq32_t seq, relative_seq_type type) {
	switch (type) {
	case RELSEQ_SEND_OUT: // sender outgoing seq
		return ::getRelativeSequenceNumber(seq, rm->firstSeq, lastLargestEndSeq, lastLargestSeqAbsolute, this);
	case RELSEQ_SEND_ACK: // sender incomming (ack) seq
		return ::getRelativeSequenceNumber(seq, rm->firstSeq, lastLargestAckSeq, lastLargestAckSeqAbsolute, this);
	case RELSEQ_RECV_INN: // receiver incomming seq
		return ::getRelativeSequenceNumber(seq, rm->firstSeq, lastLargestRecvEndSeq, lastLargestRecvSeqAbsolute, this);
	case RELSEQ_SOJ_SEQ: // sojourn time seq
		return ::getRelativeSequenceNumber(seq, rm->firstSeq, lastLargestSojournEndSeq, lastLargestSojournSeqAbsolute, this);
	case RELSEQ_NONE: // sender outgoing seq
		break;
	}
	return std::numeric_limits<ulong>::max();
}


uint32_t Connection::getDuration(bool analyse_range_duration) {
	double d;
	if (analyse_range_duration) {
		d = getTimeInterval(rm->analyse_range_start->second, rm->analyse_range_last->second);
	}
	else {
		d = rm->getDuration();
	}
	return static_cast<uint32_t>(d);
}

/* Count bundled and retransmitted packets from sent data */
bool Connection::registerSent(PcapPacket *pkt) {
	DataSeg *seg = &pkt->seg;
	totPacketSize += pkt->totalSize;
	nrPacketsSent++;
#ifdef DEBUG
	int debug = 0;
#endif

	if (seg->endSeq > lastLargestEndSeq && lastLargestEndSeq +1 != seg->seq) {
#ifdef DEBUG
		if (debug) {
			colored_fprintf(stderr, COLOR_ERROR, "CONN: %s\nSending unexpected sequence number: %llu, lastlargest: %llu\n",
							getConnKey().c_str(), seg->seq, lastLargestEndSeq);
		}
#endif
		if (closed) {
			ignored_count++;
			return false;
		}

		// For some reason, seq can increase even if no data was sent, some issue with multiple SYN packets.
		// 2014-08-12: This is expected for SYN retries after timeout (aka new connections)
		if (seg->flags & TH_SYN) {

			if (std::labs((int64_t)seg->seq_absolute - (int64_t)rm->firstSeq) > 10) {
				colored_fprintf(stderr, COLOR_NOTICE, "New SYN changes sequence number by more than 10 (%ld) on connection with %ld ranges already registered.\n"
								"This is presumably due to TCP port number reused for a new connection. "
								"Marking this connection as closed and ignore any packets in the new connection.\n",
								std::labs((int64_t)seg->seq_absolute - (int64_t)rm->firstSeq), rm->ranges.size());
				closed = true;
			}
			else {
				rm->firstSeq = seg->seq_absolute;
				//printf("Changing SD seq from (%llu - %llu) to (%d - %d)\n", seg->seq, seg->endSeq, 0, 0);
				seg->seq = 0;
				seg->endSeq = 0;
			}
		}
	}

	// This is ack
	if (seg->payloadSize == 0) {
		if (seg->flags & TH_RST) {
			//printf("CONN: %s\n", getConnKey().c_str());
			//assert("RST!" && 0);
			//return false;
			return true;
		}
		else
			return true;
	}

#ifdef DEBUG
	if (debug) {
		printf("\nRegisterSent (%llu): %s\n", (seg->endSeq - seg->seq),
		rm->absolute_seq_pair_str(seg->seq, END_SEQ(seg->endSeq)).c_str());
	}
#endif

	if (seg->endSeq > lastLargestEndSeq) { /* New data */
		if (DEBUGL_SENDER(6)) {
			printf("New Data - pkt->endSeq: %llu > lastLargestEndSeq: %llu, pkt->seq: %llu, lastLargestStartSeq: %llu, len: %u\n",
				   rm->get_print_seq(END_SEQ(seg->endSeq)),
				   rm->get_print_seq(END_SEQ(lastLargestEndSeq)),
				   rm->get_print_seq(seg->seq),
				   rm->get_print_seq(lastLargestStartSeq),
				   seg->payloadSize);
		}
#ifdef DEBUG
		if (debug) {
			printf("seg->seq:    %llu\n", rm->get_print_seq(seg->seq));
			printf(" lastLargestStartSeq:    %llu\n", rm->get_print_seq(lastLargestStartSeq));
			printf("seg->endSeq: %llu\n", rm->get_print_seq(END_SEQ(seg->endSeq)));
			printf("lastLargestEndSeq: %llu\n", rm->get_print_seq(END_SEQ(lastLargestEndSeq)));

			printf("(seg->seq == lastLargestStartSeq): %d\n", (seg->seq == lastLargestStartSeq));
			printf("(seg->endSeq > lastLargestEndSeq): %d\n", (seg->endSeq > lastLargestEndSeq));
			printf("(seg->seq > lastLargestStartSeq): %d\n", (seg->seq > lastLargestStartSeq));
			printf("(seg->seq < lastLargestEndSeq): %d\n", (seg->seq < lastLargestEndSeq));
			printf("(seg->endSeq > lastLargestEndSeq): %d\n", (seg->endSeq > lastLargestEndSeq));
		}
#endif
		// Same seq as previous packet, if (lastLargestStartSeq + lastLargestEndSeq) == 0, it's the first packet of a stream with no SYN
		if ((seg->seq == lastLargestStartSeq) && (seg->endSeq > lastLargestEndSeq)
			&& (lastLargestStartSeq + lastLargestEndSeq) != 0) {
			bundleCount++;
			totRDBBytesSent += (lastLargestEndSeq - seg->seq);
			totNewDataSent += (seg->endSeq - lastLargestEndSeq);
			seg->is_rdb = true;
			seg->rdb_end_seq = lastLargestEndSeq;
		} else if ((seg->seq > lastLargestStartSeq) && (seg->seq < lastLargestEndSeq)
				   && (seg->endSeq > lastLargestEndSeq)) {
			totRDBBytesSent += (lastLargestEndSeq - seg->seq);
			totNewDataSent += (seg->endSeq - lastLargestEndSeq);
			bundleCount++;
			seg->is_rdb = true;
			seg->rdb_end_seq = lastLargestEndSeq;
		} else if ((seg->seq < lastLargestEndSeq) && (seg->endSeq > lastLargestEndSeq)) {
			totRDBBytesSent += (lastLargestEndSeq - seg->seq);
			totNewDataSent += (seg->endSeq - lastLargestEndSeq);
			bundleCount++;
			seg->is_rdb = true;
			seg->rdb_end_seq = lastLargestEndSeq;
		}
		else {
			// New data that is not RDB
			totNewDataSent += seg->payloadSize;
		}
		lastLargestEndSeq = seg->endSeq;
		lastLargestSeqAbsolute = seg->seq_absolute + seg->payloadSize;
	} else if (lastLargestStartSeq > 0 && seg->endSeq <= lastLargestStartSeq) { /* All seen before */
		if (DEBUGL_SENDER(6)) {
			printf("\nRetrans - lastLargestStartSeq: %llu > 0 && seg->seq: %llu <= lastLargestStartSeq: %llu\n",
				   rm->get_print_seq(lastLargestStartSeq), rm->get_print_seq(seg->seq),
				   rm->get_print_seq(lastLargestStartSeq));
		}
		nrPacketRetrans++;
		totRetransBytesSent += seg->payloadSize;
		seg->retrans = true;
	}
	else {
		nrPacketRetrans++;
		totRetransBytesSent += seg->payloadSize;
		seg->retrans = true;
		if (DEBUGL_SENDER(6)) {
			printf("Retrans - lastLargestStartSeq: %llu > 0 && seg->seq: %llu <= lastLargestStartSeq: %llu\n",
				   rm->get_print_seq(lastLargestStartSeq), rm->get_print_seq(seg->seq), rm->get_print_seq(lastLargestStartSeq));
			printf("New Data - seg->endSeq: %llu > lastLargestEndSeq: %llu\n",
				   rm->get_print_seq(END_SEQ(seg->endSeq)), rm->get_print_seq(END_SEQ(lastLargestEndSeq)));
		}
	}

	if (seg->payloadSize) {
		nrDataPacketsSent++;
		lastLargestStartSeq = seg->seq;
	}
	totBytesSent += seg->payloadSize;
	return true;
}

/* Process range for outgoing packet */
void Connection::registerRange(DataSeg *seg) {
	if (DEBUGL_SENDER(4)) {
		if (firstSendTime.tv_sec == 0 && firstSendTime.tv_usec == 0) {
			firstSendTime = seg->tstamp_pcap;
		}

		timeval offset;
		timersub(&(seg->tstamp_pcap), &firstSendTime, &offset);
		cerr << "\nRegistering new outgoing. Seq: " << rm->get_print_seq(seg->seq) << " - "
		     << rm->get_print_seq(END_SEQ(seg->endSeq)) << " Absolute seq: " << seg->seq << " - "
			 << END_SEQ(seg->endSeq) << " Payload: " << seg->payloadSize << endl;
		cerr << "Time offset: Secs: " << offset.tv_sec << "." << offset.tv_usec << endl;
	}

	rm->insertSentRange(seg);

	if (DEBUGL_SENDER(4)) {
		cerr << "Last range: seq: " << rm->get_print_seq(rm->getLastRange()->getStartSeq())
		     << " - " << rm->get_print_seq(END_SEQ(rm->getLastRange()->getEndSeq())) << " - size: "
		     << rm->getLastRange()->getEndSeq() - rm->getLastRange()->getStartSeq() << endl;
	}
}

/* Register times for first ACK of each byte */
bool Connection::registerAck(DataSeg *seg) {
	static bool ret;
	if (DEBUGL_SENDER(4)) {
		timeval offset;
		timersub(&seg->tstamp_pcap, &firstSendTime, &offset);
		cerr << endl << "Registering new ACK. Conn: " << getConnKey() << " Ack: " << rm->get_print_seq(seg->ack) << endl;
		cerr << "Time offset: Secs: " << offset.tv_sec << " uSecs: " << offset.tv_usec << endl;
	}

	ret = rm->processAck(seg);
	if (ret) {
		lastLargestAckSeq = seg->endSeq;
		lastLargestAckSeqAbsolute = seg->seq_absolute + seg->payloadSize;
	}

	if (DEBUGL_SENDER(4)) {
		if (rm->getHighestAcked() != NULL) {
			cerr << "highestAcked: startSeq: " << rm->get_print_seq(rm->getHighestAcked()->getStartSeq()) << " - endSeq: "
			     << rm->get_print_seq(rm->getHighestAcked()->getEndSeq()) << " - size: "
			     << rm->getHighestAcked()->getEndSeq() - rm->getHighestAcked()->getStartSeq() << endl;
		}
	}
	return ret;
}

void Connection::calculateRetransAndRDBStats() {
	setAnalyseRangeInterval();
	rm->calculateRetransAndRDBStats();
}

// Set which ranges to analyse
void Connection::setAnalyseRangeInterval() {
	ulong start_index = 0;
	rm->analyse_range_start = rm->ranges.begin();
	rm->analyse_range_end = rm->ranges.end();
	rm->analyse_range_last = rm->analyse_range_end;
	rm->analyse_range_last--;
	rm->analyse_time_sec_start = GlobOpts::analyse_start;

	timeval tv;
	timersub(&(rm->ranges.rbegin()->second->sent_tstamp_pcap[0].first),
			 &rm->analyse_range_start->second->sent_tstamp_pcap[0].first, &tv);
	rm->analyse_time_sec_end = tv.tv_sec;

	if (GlobOpts::analyse_start) {
		map<seq64_t, ByteRange*>::iterator it, it_end;
		it = rm->ranges.begin();
		timeval first_pcap_tstamp = it->second->sent_tstamp_pcap[0].first;
		it_end = rm->ranges.end();
		for (; it != it_end; it++) {
			timersub(&(it->second->sent_tstamp_pcap[0].first), &first_pcap_tstamp, &tv);
			if (tv.tv_sec >= GlobOpts::analyse_start) {
				rm->analyse_range_start = it;
				rm->analyse_time_sec_start = tv.tv_sec;
				break;
			}
			start_index++;
		}
	}

	if (GlobOpts::analyse_end) {
		multimap<seq64_t, ByteRange*>::reverse_iterator rit, rit_end = rm->ranges.rend();
		rit = rm->ranges.rbegin();
		timeval last_pcap_tstamp = rit->second->sent_tstamp_pcap[0].first;
		rit_end = rm->ranges.rend();
		for (; rit != rit_end; rit++) {
			timersub(&last_pcap_tstamp, &(rit->second->sent_tstamp_pcap[0].first), &tv);
			if (tv.tv_sec >= GlobOpts::analyse_end) {
				rm->analyse_range_last = rm->analyse_range_end = rit.base();
				rm->analyse_range_end++;
				timersub(&(rit->second->sent_tstamp_pcap[0].first),
						 &rm->ranges.begin()->second->sent_tstamp_pcap[0].first, &tv);
				rm->analyse_time_sec_end = tv.tv_sec;
				break;
			}
		}
	}
	else if (GlobOpts::analyse_duration) {
		ulong end_index = rm->ranges.size();
		timeval begin_tv = rm->analyse_range_start->second->sent_tstamp_pcap[0].first;
		map<seq64_t, ByteRange*>::iterator begin_it, end_it, tmp_it;
		begin_it = rm->analyse_range_start;
		end_it = rm->ranges.end();
		while (true) {
			ulong advance = (end_index - start_index) / 2;
			// We have found the transition point
			if (!advance) {
				rm->analyse_range_end = rm->analyse_range_last = begin_it;
				rm->analyse_range_end++;
				timersub(&(begin_it->second->sent_tstamp_pcap[0].first), &begin_tv, &tv);
				rm->analyse_time_sec_end = rm->analyse_time_sec_start + GlobOpts::analyse_duration;
				break;
			}

			tmp_it = begin_it;
			std::advance(tmp_it, (long)advance);

			timersub(&(tmp_it->second->sent_tstamp_pcap[0].first), &begin_tv, &tv);
			// Compares seconds, does not take into account milliseconds
			// Shorter than the requested length
			if (tv.tv_sec <= GlobOpts::analyse_duration) {
				begin_it = tmp_it;
				start_index += advance;
			}
			// Longer than the requested length
			else if (tv.tv_sec > GlobOpts::analyse_duration) {
				end_index -= advance;
			}
		}
	}
}

/* Generate statistics for each connection.
   update aggregate stats if requested */
void Connection::addConnStats(ConnStats* cs) {
	cs->duration += getDuration(true);
	cs->analysed_duration_sec += rm->analyse_time_sec_end - rm->analyse_time_sec_start;
	cs->analysed_start_sec += rm->analyse_time_sec_start;
	cs->analysed_end_sec += rm->analyse_time_sec_end;
	cs->totBytesSent += rm->analysed_bytes_sent;
	cs->totRetransBytesSent += rm->analysed_bytes_retransmitted;
	cs->nrPacketsSent += rm->analysed_packet_sent_count;
	cs->nrPacketsSentFoundInDump += rm->analysed_packet_sent_count_in_dump;
	cs->nrPacketsReceivedFoundInDump += rm->analysed_packet_received_count;
	cs->nrDataPacketsSent += rm->analysed_data_packet_count;
	cs->nrPacketRetrans += rm->analysed_retr_packet_count;
	cs->nrPacketRetransNoPayload += rm->analysed_retr_no_payload_packet_count;
	cs->bundleCount += rm->analysed_rdb_packet_count;
	cs->totUniqueBytes += getNumUniqueBytes();
	cs->totUniqueBytesSent += rm->analysed_bytes_sent_unique;
	cs->redundantBytes += rm->getRedundantBytes();
	cs->rdb_bytes_sent += rm->rdb_byte_miss + rm->rdb_byte_hits;
	cs->ackCount += rm->analysed_ack_count;
	cs->synCount += rm->analysed_syn_count;
	cs->finCount += rm->analysed_fin_count;
	cs->rstCount += rm->analysed_rst_count;
	cs->pureAcksCount += rm->analysed_pure_acks_count;

	cs->ranges_sent += rm->getByteRangesSent();
	cs->ranges_lost += rm->getByteRangesLost();
	cs->bytes_lost += rm->getLostBytes();
	cs->totPacketSize += totPacketSize;

	if ((rm->analysed_bytes_sent - getNumUniqueBytes()) != rm->analysed_redundant_bytes) {
		if (rm->analysed_bytes_sent >= getNumUniqueBytes()) {
			if (DEBUGL_SENDER(1)) {
				colored_fprintf(stderr, COLOR_WARN, "Mismatch between redundant bytes and (bytes_sent - UniqueBytes) which should be equal\n");
				fprintf(stderr, "CONNKEY: %s\n", getConnKey().c_str());
				fprintf(stderr, "rm->analysed_bytes_sent - getNumUniqueBytes (%llu) != rm->analysed_redundant_bytes (%llu)\n",
					   rm->analysed_bytes_sent - getNumUniqueBytes(), rm->analysed_redundant_bytes);
				fprintf(stderr, "rm->analysed_bytes_sent: %llu\n", rm->analysed_bytes_sent);
				fprintf(stderr, "getNumUniqueBytes(): %llu\n", getNumUniqueBytes());
				fprintf(stderr, "rm->analysed_redundant_bytes: %llu\n", rm->analysed_redundant_bytes);
				fprintf(stderr, "cs->totBytesSent: %llu\n", cs->totBytesSent);
				fprintf(stderr, "cs->totUniqueBytes: %llu\n", cs->totUniqueBytes);
				fprintf(stderr, "cs->totBytesSent - cs->totUniqueBytes: %llu\n", cs->totBytesSent - cs->totUniqueBytes);
			}
		}
	}
	cs->rdb_packet_misses += rm->rdb_packet_misses;
	cs->rdb_packet_hits += rm->rdb_packet_hits;
	cs->rdb_byte_misses += rm->rdb_byte_miss;
	cs->rdb_byte_hits += rm->rdb_byte_hits;
}

/* Generate statistics for bytewise latency */
PacketsStats* Connection::getBytesLatencyStats() {
	if (!packetsStats.has_stats()) {
		packetsStats.init();
		rm->genStats(&packetsStats);
	}
	return &packetsStats;
}

/* Check validity of connection range and time data */
void Connection::validateRanges() {
	if (DEBUGL(2)) {
		cerr << "###### Validation of range data ######" << endl;
		cerr << "Connection: " << getConnKey() << endl;
	}
	rm->validateContent();
}

void Connection::registerRecvd(DataSeg *seg) {
	/* Insert range into datastructure */
	if (seg->seq <= lastLargestRecvEndSeq &&
		seg->endSeq > lastLargestRecvEndSeq) {
		seg->in_sequence = 1;
	}
	rm->insertReceivedRange(seg);
	lastLargestRecvEndSeq = seg->endSeq;
	lastLargestRecvSeqAbsolute = seg->seq_absolute + seg->payloadSize;
}

void Connection::writeByteLatencyVariationCDF(ofstream *stream) {
	*stream << endl;
	*stream << "#------CDF - Conn: " << getConnKey() << " --------" << endl;
	rm->writeByteLatencyVariationCDF(stream);
}

ullint_t Connection::getNumUniqueBytes() {
	multimap<seq64_t, ByteRange*>::iterator it, it_end;
	it = rm->analyse_range_start;
	it_end = rm->analyse_range_end;
	seq64_t first_data_seq = 0, last_data_seq = 0;
	for (; it != it_end; it++) {
		if (it->second->getNumBytes()) {
			first_data_seq = it->second->getStartSeq();
			break;
		}
	}

	it_end = rm->analyse_range_start;
	it = rm->analyse_range_last;
	for (; it != it_end; it--) {
		if (it->second->getNumBytes()) {
			last_data_seq = it->second->getEndSeq();
			break;
		}
	}
	ulong unique_data_bytes = last_data_seq - first_data_seq;
	return unique_data_bytes;
}

void Connection::registerPacketSize(const timeval& first, const timeval& ts, const uint32_t ps,
									const uint16_t payloadSize, bool retrans) {
	const uint64_t relative_ts = static_cast<uint64_t>(TV_TO_MS(ts) - TV_TO_MS(first));
	const uint64_t sent_time_bucket_idx = relative_ts / GlobOpts::throughputAggrMs;

	while (sent_time_bucket_idx >= packetSizes.size()) {
		vector< PacketSize> empty;
		packetSizes.push_back(empty);
		PacketSizeGroup empty2;
		packetSizeGroups.push_back(empty2);
	}

	PacketSize pSize(ts, static_cast<uint16_t>(ps), payloadSize, retrans);
	packetSizes[sent_time_bucket_idx].push_back(pSize);
	packetSizeGroups[sent_time_bucket_idx].add(pSize);
}

void Connection::writePacketByteCountAndITT(vector<csv::ofstream*> streams) {
	size_t i, j;

	uint64_t k = 0;
	while (packetSizes[k].empty()) k++;

	int64_t prev = TV_TO_MICSEC(packetSizes[k][0].time);
	int64_t itt, tmp;
	for (i = 0; i < packetSizes.size(); ++i) {
		for (j = 0; j < packetSizes[i].size(); ++j) {
			tmp = TV_TO_MICSEC(packetSizes[i][j].time);
			itt = (tmp - prev) / 1000L;
			prev = tmp;

			for (csv::ofstream* stream : streams) {
				*stream << tmp << itt << packetSizes[i][j].payload_size << packetSizes[i][j].packet_size << NEWLINE;
            }
		}
	}
}

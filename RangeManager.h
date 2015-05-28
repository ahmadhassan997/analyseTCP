#ifndef RANGEMANAGER_H
#define RANGEMANAGER_H

#include "common.h"
#include "time_util.h"
#include "statistics_common.h"

using namespace std;

enum received_type {DEF, DATA, RDB, RETR};

enum insert_type {INSERT_SENT, INSERT_RECV, INSERT_SOJOURN};

extern const char *received_type_str[4];

/* Forward declarations */
class ByteRange;
class Connection;

/* Has responsibility for managing ranges, creating,
   inserting and resizing ranges as sent packets and ACKs
   are received */
class RangeManager {
private:
	struct timeval highestRecvd;
	int redundantBytes;
	long lowestRecvDiff; /* Lowest pcap packet diff */
	double drift; /* Clock drift (ms/s) */

	int minimum_segment_size;
	int maximum_segment_size;

	map<ulong, ByteRange*>::iterator highestAckedByteRangeIt;
	map<const long, int> byteLatencyVariationCDFValues;

public:
	map<ulong, ByteRange*> ranges;
	uint32_t firstSeq; /* The absolute start sequence number */
	uint64_t lastSeq;  /* Global relative end sequence number (Equals the number of unique bytes) */

	map<ulong, ByteRange*>::iterator first_to_analyze, last_to_analyze;

	// The number of RDB bytes that were redundant and not
	int rdb_packet_misses;
	int rdb_packet_hits;
	uint rdb_byte_miss;
	uint rdb_byte_hits;
	int analysed_lost_ranges_count;
	int analysed_sent_ranges_count;
	uint analysed_lost_bytes;
	int ack_count;
	uint64_t analysed_bytes_sent, analysed_bytes_sent_unique, analysed_bytes_retransmitted, analysed_redundant_bytes;
	int analysed_packet_sent_count, analysed_retr_packet_count, analysed_retr_packet_count_in_dump,
		analysed_retr_no_payload_packet_count, analysed_rdb_packet_count, analysed_ack_count,
		analysed_packet_sent_count_in_dump, analysed_packet_received_count, analysed_ranges_count, analysed_sent_pure_ack_count;

	int analysed_data_packet_count;
	int analysed_syn_count, analysed_fin_count, analysed_rst_count, analysed_pure_acks_count;
	uint16_t analysed_max_range_payload;

	map<ulong, ByteRange*>::iterator analyse_range_start, analyse_range_last, analyse_range_end;
	uint64_t analyse_time_sec_start, analyse_time_sec_end;

	Connection *conn;
public:
	RangeManager(Connection *c, uint32_t first_seq) :
		redundantBytes(0), lastSeq(0),
		rdb_packet_misses(0), rdb_packet_hits(0), rdb_byte_miss(0),
		rdb_byte_hits(0), analysed_lost_ranges_count(0),
		analysed_sent_ranges_count(0), analysed_lost_bytes(0),
		ack_count(0), analysed_bytes_sent(0), analysed_bytes_sent_unique(0), analysed_bytes_retransmitted(0),
		analysed_redundant_bytes(0), analysed_packet_sent_count(0),
		analysed_retr_packet_count(0), analysed_retr_packet_count_in_dump(0),
		analysed_retr_no_payload_packet_count(0),
		analysed_rdb_packet_count(0), analysed_ack_count(0),
		analysed_packet_sent_count_in_dump(0), analysed_packet_received_count(0),
		analysed_ranges_count(0), analysed_sent_pure_ack_count(0), analysed_data_packet_count(0),
		analysed_syn_count(0), analysed_fin_count(0), analysed_rst_count(0), analysed_pure_acks_count(0),
		analysed_max_range_payload(0)
	{
        conn = c;
        firstSeq = first_seq;
        lowestRecvDiff = std::numeric_limits<long>::max();
		highestAckedByteRangeIt = ranges.end();
		memset(&highestRecvd, 0, sizeof(highestRecvd));
	};

	~RangeManager();

	void insertSentRange(struct sendData *sd);
	void insertReceivedRange(struct sendData *sd);
	bool processAck(struct DataSeg *seg);
	void genStats(PacketsStats* bs);
	ByteRange* getLastRange() {	return ranges.rbegin()->second;	}
	ByteRange* getHighestAcked();
	uint32_t getDuration();
	double getDuration(ByteRange *brLast);
	void validateContent();
	void calculateLatencyVariation();
	void registerRecvDiffs();
	void makeByteLatencyVariationCDF();
	void writeByteLatencyVariationCDF(ofstream *stream);
	void writeSentTimesAndQueueingDelayVariance(const uint64_t first_tstamp, vector<ofstream*> streams);
	int calculateClockDrift();
	void doDriftCompensation();
	bool insert_byte_range(uint64_t start_seq, uint64_t end_seq, insert_type type, struct DataSeg *data_seq, int level);
	void genAckLatencyFiles(long first_tstamp, const string& connKey);
	void genAckLatencyData(long first_tstamp, vector<SPNS::shared_ptr<vector <LatencyItem> > > &diff_times, const string& connKey);

	int getNumBytes() { return lastSeq; } // lastSeq is the last relative seq number
	long int getByteRangesCount() { return ranges.size(); }
	long int getAnalysedByteRangesCount() { return ranges.size(); }
	long int getByteRangesLost() { return analysed_lost_ranges_count; }
	long int getByteRangesSent() { return analysed_sent_ranges_count; }
	int getRedundantBytes(){ return analysed_redundant_bytes; }
	int getLostBytes() { return analysed_lost_bytes; }
	ulong get_print_seq(uint64_t seq);
	string get_print_relative_seq_pair(uint64_t start, uint64_t end);
	ulong relative_seq(uint64_t seq);
	string relative_seq_pair_str(uint64_t start, uint64_t end);
	void calculateRealLoss(map<ulong, ByteRange*>::iterator brIt, map<ulong, ByteRange*>::iterator brIt_end);
	void analyseReceiverSideData();
	void calculateRetransAndRDBStats();
	void calculateLossGroupedByInterval(const uint64_t first_ts, vector<LossInterval>& aggr_loss, vector<LossInterval>& loss);
	void printPacketDetails();
};

int seq_with_print_range(uint64_t start, uint64_t end, size_t &print_packet_ranges_index);

#endif /* RANGEMANAGER_H */

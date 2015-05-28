#include "util.h"

string get_TCP_flags_str(u_char flags) {
	stringstream out;
	if (flags & TH_SYN)
		out << ",SYN";
	if (flags & TH_ACK)
		out << ",ACK";
	if (flags & TH_FIN)
		out << ",FIN";
	if (flags & TH_RST)
		out << ",RST";
	string result = out.str();
	if (!result.empty())
		result.erase(0, 1);
	return "[" + result + "]";;
}
/*
  Checks if a char buf is a string
*/
bool isNumeric(const char* pszInput, int nNumberBase) {
	string base = "0123456789ABCDEF";
	string input = pszInput;
	return (input.find_first_not_of(base.substr(0, nNumberBase)) == string::npos);
}


string getIp(const struct in_addr &ip) {
	char ip_buf[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &ip, ip_buf, INET_ADDRSTRLEN);
	return string(ip_buf);
}

string makeHostKey(const struct in_addr &ip, const uint16_t *port) {
	return getIp(ip) + ":" + to_string(ntohs(*port));
}

string makeConnKey(const struct in_addr &srcIp, const struct in_addr &dstIp, const uint16_t *srcPort, const uint16_t *dstPort) {
	stringstream connKeyTmp;
	connKeyTmp << makeHostKey(srcIp, srcPort) << "-" << makeHostKey(dstIp, dstPort);
	return connKeyTmp.str();
}


void parse_print_packets(char* optarg)
{
	std::istringstream ss(optarg);
	std::string token;
	uint64_t last_range_seq = 0;
	uint64_t num;
	size_t range_i;
	while (std::getline(ss, token, ',')) {
		range_i = token.find("-");
		if (range_i == string::npos) {
			// Add just this seqnum
			istringstream(token) >> num;
			GlobOpts::print_packets_pairs.push_back(pair<uint64_t, uint64_t>(num, num));
		}
		else {
			// e.g. '-1000'
			if (range_i == 0) {
				istringstream(token.substr(1, string::npos)) >> num;
				GlobOpts::print_packets_pairs.push_back(make_pair(last_range_seq, num));
				last_range_seq = num + 1;
			}
			else if (range_i == token.size() - 1) {
				string f1 = token.substr(0, range_i);
				GlobOpts::print_packets_pairs.push_back(make_pair(std::stoul(f1), (numeric_limits<uint64_t>::max)()));
			}
			else {
				// We have a range with two values
				string f1 = token.substr(0, token.find("-"));
				string f2 = token.substr(token.find("-")+1, string::npos);
				istringstream(token.substr(1, string::npos)) >> num;
				GlobOpts::print_packets_pairs.push_back(make_pair(std::stoul(f1), std::stoul(f2)));
				last_range_seq = std::stoul(f2) + 1;
			}
		}
		istringstream(token) >> num;
	}
}

// Requires #include <execinfo.h>
//void print_stack() {
//	void *array[10];
//	size_t size;
//
//	// get void*'s for all entries on the stack
//	size = backtrace(array, 10);
//
//	// print out all the frames to stderr
//	//fprintf(stderr, "Error: signal %d:\n", sig);
//	backtrace_symbols_fd(array, size, STDERR_FILENO);
//	//exit(1);
//}


/**
 * Parse the long_options data structure to ensure an optstring that
 * is always up to date.
 * Return pair<string,string>(OPTSTRING, usage)
 */
pair <string,string> make_optstring(struct option long_options[])
{
	stringstream usage_tmp, opts;
	int i = 0;
	for (; long_options[i].name != 0; i++) {
		if (i)
			usage_tmp << "|";
		if (isprint(long_options[i].val))
			usage_tmp << "-" << ((char) long_options[i].val);
		else
			usage_tmp << "--" << (long_options[i].name);

		opts << (char) long_options[i].val;
		if (long_options[i].has_arg == no_argument)
			continue;
		opts << ':';
		if (long_options[i].has_arg == optional_argument)
			opts << ':';
	}
	return make_pair(opts.str(), usage_tmp.str());
}

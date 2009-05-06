/*************************************************************************************
**************************************************************************************
**                                                                                  **
**  analyseRdb - Tool for analysing sender side tcpdump                             **
**               files with regard to latency.                                      **
**                                                                                  **
**  Copyright (C) 2007     Andreas Petlund  - andreas@petlund.no                    **
**                     and Kristian Evensen - kristrev@ifi.uio.no                   **
**                                                                                  **
**     This program is free software; you can redistribute it and/or modify         **
**     it under the terms of the GNU General Public License as published by         **
**     the Free Software Foundation; either version 2 of the License, or            **
**     (at your option) any later version.                                          **
**                                                                                  **
**     This program is distributed in the hope that it will be useful,              **
**     but WITHOUT ANY WARRANTY; without even the implied warranty of               **
**     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the                **
**     GNU General Public License for more details.                                 ** 
**                                                                                  **
**     You should have received a copy of the GNU General Public License along      **
**     with this program; if not, write to the Free Software Foundation, Inc.,      **
**     51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.                  **
**                                                                                  **
**************************************************************************************
*************************************************************************************/

#include "analyseTCP.h"

/* Initialize global options */
bool GlobOpts::aggregate      = false;
bool GlobOpts::aggOnly        = false;
bool GlobOpts::withRecv       = false;
bool GlobOpts::transport      = false;
bool GlobOpts::genRFiles      = false;
string GlobOpts::prefix       = "";
int GlobOpts::debugLevel      = 0;
string GlobOpts::sendNatIP    = "";
string GlobOpts::recvNatIP    = "";


void usage (char* argv){
  printf("Usage: %s [-s] [-r] [-p] [-f] [-v|a|b|d]\n", argv);
  printf("Required options:\n");
  printf(" -s <sender ip>     : Sender ip.\n");
  printf(" -r <receiver ip>   : Receiver ip.\n");
  printf(" -p <receiver port> : Receiver port.\n");
  printf(" -f <pcap-file>     : Sender-side dumpfile.\n");
  printf("Other options:\n");
  printf(" -g                 : Receiver-side dumpfile\n");
  printf(" -t                 : Calculate transport-layer delays\n");
  printf("                    : (if not set, application-layer delay is calculated)\n");
  printf(" -u <prefix>        : Write statistics to comma-separated files (for use with R)\n");
  printf("                      <prefix> assigns an output filename prefix.\n");
  printf(" -m <IP>            : Sender side external NAT address (as seen on recv dump)\n");
  printf(" -n <IP>            : Receiver side local address (as seen on recv dump\n");
  printf(" -a                 : Get aggregated statistics (off by default, optional)\n");
  printf(" -A                 : Only print aggregated statistics (off by default, optional)\n");
  printf(" -d                 : Indicate debug level\n");
  printf("                      1 = Only output on reading sender side dump first pass.\n");
  printf("                      2 = Only output on reading sender side second pass.\n");
  printf("                      3 = Only output on reading receiver side.\n");
  printf("                      4 = Only output when comparing sender and receiver.\n");
  printf("                      5 = Print all debug messages.\n");
  exit(0);
}

int main(int argc, char *argv[]){
  char *src_ip;
  char *dst_ip;
  int dst_port;
  char *sendfn; /* Sender dump file name */
  char *recvfn; /* Receiver dump filename */

  int c;
  Dump *senderDump;

  while(1){
    c = getopt( argc, argv, "s:r:p:f:m:n:o:g:d:u:aAt");
    if(c == -1) break;

    switch(c){
    case 's':
      src_ip = optarg;
      break;
    case 'r':
      dst_ip = optarg;
      break;
    case 'p':
      dst_port = atoi(optarg);
      break;
    case 'm':
      GlobOpts::sendNatIP = optarg;
      break;
    case 'n':
      GlobOpts::recvNatIP = optarg;
      break;
    case 'f':
      sendfn = optarg;
      break;
    case 'g':
      recvfn = optarg;
      GlobOpts::withRecv = true;
      break;
    case 't':
      GlobOpts::transport = true;
      break;
    case 'u':
      GlobOpts::prefix = optarg;
      GlobOpts::genRFiles = true;
      break;
    case 'a':
      GlobOpts::aggregate = true;
      break;
    case 'A':
      GlobOpts::aggOnly = true;
      GlobOpts::aggregate = true;
      break;
    case 'd':
      GlobOpts::debugLevel = atoi(optarg);
      break;
    case '?':
      if (optopt == 'c')
	fprintf(stderr, "Option -%c requires an argument\n", optopt);
      else if(isprint(optopt))
	fprintf(stderr,"Unknown option -%c\n", optopt);
      else
	fprintf(stderr, "Something is really wrong\n");

      return 1;
    default:
      break;
    }
  }

  if(argc < 6){
    usage(argv[0]);
  }

  if(GlobOpts::debugLevel < 0)
    cerr << "debugLevel = " << GlobOpts::debugLevel << endl;
  
  /* Create Dump - object */
  senderDump = new Dump(src_ip, dst_ip, dst_port, sendfn);
  senderDump->analyseSender();
  
  if(GlobOpts::genRFiles)
    senderDump->genRFiles();
  
  if (GlobOpts::withRecv)
    senderDump->processRecvd(recvfn);
  
  senderDump->printDumpStats();

  return 0;
}


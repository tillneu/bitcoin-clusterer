CPPFLAGS  = -O3 -Wall -Wno-unused-variable -g --std=c++11

all:
	g++ $(CPPFLAGS) parsebc.cc -o parsebc
	g++ $(CPPFLAGS) cattxgraph.cc -o cattxgraph
	g++ $(CPPFLAGS) txperaddr.cc -o txperaddr
	g++ $(CPPFLAGS) clusterize.cc -o clusterize
	g++ $(CPPFLAGS) histcluster.cc -o histcluster
	g++ $(CPPFLAGS) catcluster.cc -o catcluster
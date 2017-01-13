# Overview

Reads transactions from the bitcoin blockchain into a file representing the transaction graph and performs address clustering on the transaction graph.

The clustering is performed completely in-memory. It only takes about half an hour but requires around 64GB of memory (depending on the used heuristics).


# Requirements & Build
Requires C++11, no additional dependencies.

Run ``make`` to build all required executables.


# Usage

Detailed usage instructions for each executable are shown when run without any parameter. The general workflow is as follows:

0. Run bitcoind for a long time to get the current blockchain.
1. Run `./parsebc` on your blockchain data. This program creates the transaction graph binary file.
2. Check with `./cattxgraph`, whether the binary graph file looks good.
3. Run `./txperaddr` to extract the number of transactions per address. This information is required for some of the used heuristics.
4. Run `./clusterize` on the transaction graph file and on the transactions per address file to actually perform the clustering.
5. Check the results with `./catcluster` or crate a histogram using the cluster sizes (`./histcluster`)

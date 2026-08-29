#include <iostream>
#include <fstream>
#include <vector>
#include <mpi.h>
using namespace std;
#define ll long long

void master(int v, int wrkrcount, ifstream &in) {
    // optimal to precompute the data to send, than to send repeatedly

    // input adjlist, and store number of nbrs, and offset for each worker
    vector<int> adjlist; // 1d for easy sending
    vector<int> nbrcounts(v);
    for (int i=0; i<v; i++) {
        in >> nbrcounts[i];
        for (int j=0; j<nbrcounts[i]; j++) {
            int temp; in >> temp;
            adjlist.push_back(temp);
        }
    }
        
    // 1. send number of vertices
    int minvcount = v/wrkrcount;
    int remainder = v%wrkrcount;
    vector<int> vcounts(wrkrcount+1, minvcount);
    vcounts[0] = 0;
    // NOTE: remainder < wrkrcount
    for (int i=1; i<=remainder; i++) {
        vcounts[i]++;
    }
    cout << "vcounts: ";
    for (int i : vcounts) cout << i << ' ';
    cout << endl;

    MPI_Scatter(vcounts.data(), 1, MPI_INT, // send
                MPI_IN_PLACE, 0, MPI_INT,   // recv
                0, MPI_COMM_WORLD);

    vector<int> nbrcount_starts(wrkrcount+1, 0); // prefix sum of vcounts
    for (int i=1; i<=wrkrcount; i++) {
        nbrcount_starts[i] = nbrcount_starts[i-1] + vcounts[i-1];
    }

    // 2. send first vertex id for each
    MPI_Scatter(nbrcount_starts.data(), 1, MPI_INT,
                MPI_IN_PLACE, 0, MPI_INT,
                0, MPI_COMM_WORLD);

    // 3. send neighbor counts for each
    cout << "nbrcounts: ";
    for (int i : nbrcounts) cout << i << ' ';
    cout << endl;

    MPI_Scatterv(nbrcounts.data(), vcounts.data(), nbrcount_starts.data(), MPI_INT,
                 MPI_IN_PLACE, 0, MPI_INT,
                 0, MPI_COMM_WORLD);

    // 4. send neighbors for each
    // calculate total number of neighbors to send to each worker
    vector<int> allnbrcounts(wrkrcount+1, 0);
    int curwrkr = 0;
    int curwrkr_vcount = 0;
    for (int nbrcount : nbrcounts) {
        // nbrcount per vertex
        if (curwrkr_vcount == vcounts[curwrkr]) {
            // if current worker's nbrs added,
            // move to next worker
            curwrkr++;
            curwrkr_vcount = 0;
        }
        allnbrcounts[curwrkr] += nbrcount;
        curwrkr_vcount++;
    }
    cout << "allnbrcounts: ";
    for (int i : allnbrcounts) cout << i << ' ';
    cout << endl;

    vector<int> nbr_starts(wrkrcount+1, 0); // prefix of allnbrcounts
    for (int i=1; i<=wrkrcount; i++) {
        nbr_starts[i] = nbr_starts[i-1] + allnbrcounts[i-1];
    }

    MPI_Scatterv(adjlist.data(), allnbrcounts.data(), nbr_starts.data(), MPI_INT,
                 MPI_IN_PLACE, 0, MPI_INT,
                 0, MPI_COMM_WORLD);

    // initialize components
    vector<int> ids(v);
    for (int i=0; i<v; i++) ids[i] = i;

    // begin v-1 rounds of edge relaxing
    for (int r=0; r<v-1; r++) {
        // send component ids to all
        MPI_Bcast(ids.data(), v, MPI_INT, 0, MPI_COMM_WORLD);
    }
}

void worker(int v, int rank) {
    int v_i;
    MPI_Scatter(NULL, 0, MPI_INT,
                &v_i, 1, MPI_INT,
                0, MPI_COMM_WORLD);
    cout << "rank " << rank << " v " << v_i << endl;

    int first_v;
    MPI_Scatter(NULL, 0, MPI_INT,
                &first_v, 1, MPI_INT,
                0, MPI_COMM_WORLD);

    vector<int> nbrcounts_i(v_i);
    MPI_Scatterv(NULL, NULL, NULL, MPI_INT,
                 nbrcounts_i.data(), v_i, MPI_INT,
                 0, MPI_COMM_WORLD);

    int totalnbrcount = 0;
    cout << "nbrcounts: ";
    for (int i : nbrcounts_i) {
        cout << i << ' ';
        totalnbrcount += i;
    }
    cout << endl;

    vector<int> nbrs(totalnbrcount);
    MPI_Scatterv(NULL, NULL, NULL, MPI_INT,
                 nbrs.data(), totalnbrcount, MPI_INT,
                 0, MPI_COMM_WORLD);
    cout << "nbrs: ";
    for (int i : nbrs) cout << i << ' ';
    cout << endl;

    // construct partial adjlist
    vector<vector<int>> adjlist(v);
    int cur_nbr = 0;
    for (int i=0; i<v_i; i++) {
        for (int j=0; j<nbrcounts_i[i]; j++) {
            adjlist[i+first_v].push_back(nbrs[cur_nbr++]);
        }
    }

    cout << "rank: " << rank << endl;
    for (int i=0; i<v; i++) {
        cout << i << ':';
        for (int j : adjlist[i]) cout << j << ' ';
        cout << endl;
    }

    // begin v-1 rounds
    vector<int> ids(v);
    for (int r=0; r<v-1; r++) {
        // receive latest component ids
        MPI_Bcast(ids.data(), v, MPI_INT, 0, MPI_COMM_WORLD);
        for (int i : ids) cout << i << ' ';
        cout << endl;
    }
}

int main() {
    // n-1 rounds; each round involves sending your id to your neighbors
    // before each round, master sends current labels to all
    // method 1: set your neighbors' labels, then master reduces with MIN
    // method 2: set your label from neighbors', then send only yours to master
    ifstream in;

    MPI_Init(NULL, NULL);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int v;
    if (rank == 0) {
        in.open("in");
        in >> v;
    }
    MPI_Bcast(&v, 1, MPI_INT, 0, MPI_COMM_WORLD);
    cout << "rank: " << rank << ' ' << v << endl;

    if (rank == 0) {
        int size;
        MPI_Comm_size(MPI_COMM_WORLD, &size);
        master(v, size-1, in);
    } else {
        worker(v, rank);
    }

    MPI_Finalize();
}

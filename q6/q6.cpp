#include <iostream>
#include <vector>
#include <mpi.h>
using namespace std;
#define ll long long

void master(int v, int wrkrcount, istream &in) {
    // optimal to precompute the data to send, than to send repeatedly

    // input adjlist, and store number of nbrs, and offset for each worker
    vector<int> adjlist; // 1d for easy sending
    vector<int> nbrcounts(v+1, 0);
    for (int i=1; i<=v; i++) {
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
    MPI_Scatter(vcounts.data(), 1, MPI_INT, // send
                MPI_IN_PLACE, 0, MPI_INT,   // recv
                0, MPI_COMM_WORLD);

    // 2. send neighbor counts for each
    vector<int> nbrcount_starts(wrkrcount+1, 0);
    for (int i=1; i<=wrkrcount; i++) {
        nbrcount_starts[i] = nbrcount_starts[i-1] + nbrcounts[i-1];
    }
    MPI_Scatterv(nbrcounts.data(), vcounts.data(), nbrcount_starts.data(), MPI_INT,
                 MPI_IN_PLACE, 0, MPI_INT,
                 0, MPI_COMM_WORLD);

    /* 3. send neighbors for each
    vector<int> nbr_starts(wrkrcount+1, 0); // TODO calculate
    vector<int> nbrcounts_counts(wrkrcount+1, 0); // TODO
    MPI_Scatterv(adjlist.data(), nbrcounts_counts.data(), nbr_starts.data(), MPI_INT,
                 MPI_IN_PLACE, 0, MPI_INT,
                 0, MPI_COMM_WORLD);
                 */
}

void worker(int rank) {
    int v_i;
    MPI_Scatter(NULL, 0, MPI_INT,
                &v_i, 1, MPI_INT,
                0, MPI_COMM_WORLD);
    cout << "rank " << rank << " v " << v << endl;

    vector<int> nbrcounts_i(v_i);
    MPI_Scatterv(NULL, NULL, NULL, MPI_INT,
                 nbrcounts_i.data(), v_i, MPI_INT,
                 0, MPI_COMM_WORLD);
}

int main() {
    // n-1 rounds; each round involves sending your id to your neighbors
    // method 1: send id directly to process with neighbor
    // method 2: send (v, id) to master, and master reduces by grouping (v, id_i)
    ifstream in;

    MPI_Init(NULL, NULL);
    int rank;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    int v;
    if (rank == 0) {
        in.open("in");
        in >> v;
    }
    MPI_Bcast(&m, 1, MPI_INT, 0, MPI_COMM_WORLD);
    cout << "rank: " << rank << ' ' << v << endl;

    if (rank == 0) {
        int size;
        MPI_Comm_size(MPI_COMM_WORLD, &size);
        master(v, size-1, in);
    } else {
        worker(v);
    }

    MPI_Finalize();
}

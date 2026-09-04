#include <iostream>
#include <fstream>
#include <vector>
#include <queue>
#include <mpi.h>
using namespace std;
#define LOG(x) cout << x;

void bfs(int i, vector<vector<int>> &adjlist, vector<int> &ids) {
    queue<int> q;
    q.push(i);
    while (!q.empty()) {
        int u = q.front();
        q.pop();
        for (int v : adjlist[u]) {
            if (ids[v] > ids[u]) {
                ids[v] = ids[u];
                q.push(v);
            }
        }
    }
}

int main() {
    MPI_Init(NULL, NULL);
    ifstream in("in");
    int v; in >> v;
    vector<vector<int>> adjlist(v);
    for (int i=0; i<v; i++) {
        int n; in >> n;
        for (int j=0; j<n; j++) {
            int nbr; in >> nbr;
            adjlist[i].push_back(nbr);
        }
    }

    double start_time = MPI_Wtime();

    vector<int> ids(v);
    for (int i=0; i<v; i++) ids[i] = i;

    for (int i=0; i<v; i++) {
        bfs(i, adjlist, ids);
    }

    ofstream out("out");
    for (int i : ids) out << i << '\n';

    double end_time = MPI_Wtime();
    LOG("Time taken");
    cout << end_time - start_time << " seconds\n";
    MPI_Finalize();
}

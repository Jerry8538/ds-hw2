#include "q8_common.hpp"
#include <iostream>
using namespace std;

int main() {
    // io optimisation is needed when we are taking a lot of records in input
    ios_base::sync_with_stdio(false); // normally cin and cout are synced with c streams, but we disable that so less sync overhead and we have faster IO
    cin.tie(nullptr); // normally cin is tied to cout and so we are just breaking that and due to this everytime the cout doesn't get flushed

    int N, K, S;
    if(!(cin >> N >> K >> S)){
        return 0;
    }

    if (N == 0) {
        cout << "TOTAL_MEASUREMENTS 0\n";
        return 0;
    }

    vector<Measurement> data(N);
    for (int i = 0; i < N; i++) {
        cin >> data[i].timestamp >> data[i].station_id
            >> data[i].temperature >> data[i].humidity >> data[i].pressure
            >> data[i].rainfall >> data[i].wind_speed;
    }

    const int P = 9;  // 1 logical master + 8 logical workers
    Stats globalStats = runMasterWorkerSimulation(data, N, S, P);

    printResults(globalStats, K);

    return 0;
}

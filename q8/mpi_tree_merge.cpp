#include "q8_common.hpp"
#include <mpi.h>
#include <cstdio>
#include<iostream>
using namespace std;

// this thing is complex but this will help us to write clean code, cause the other two methods i found are : 1. flatten the struct to a buffer array basically serialize it, then while receiving deserialize, but there will be millions of records, so this just doesn't feel correct and the second method was to send each data value individually, which also didn't seem correct to me
MPI_Datatype createMeasurementType() {
    Measurement sample;
    const int fieldCount = 7;

    int blockLengths[fieldCount] = {1, 1, 1, 1, 1, 1, 1}; // this is count for each field and it is 1 for all
    MPI_Datatype fieldTypes[fieldCount] = { // field type of all the struct members, in declaration order
        MPI_LONG_LONG, MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE, MPI_DOUBLE, MPI_INT
    };

    // this concept is really beautiful
    // ok so in simple c++ we just write struct, but for MPI we need to know offset of each the struct member and due to padding the offset will not be continuous so the trick that we use is that, get the starting address of the member and subtract the base address from that and that gives us the offset
    // MPI_Aint is MPI datatype to hold address
    MPI_Aint base, addr;
    MPI_Aint displacements[fieldCount];

    MPI_Get_address(&sample, &base);
    MPI_Get_address(&sample.timestamp, &addr);   displacements[0] = addr - base;
    MPI_Get_address(&sample.temperature, &addr); displacements[1] = addr - base;
    MPI_Get_address(&sample.humidity, &addr);    displacements[2] = addr - base;
    MPI_Get_address(&sample.pressure, &addr);    displacements[3] = addr - base;
    MPI_Get_address(&sample.rainfall, &addr);    displacements[4] = addr - base;
    MPI_Get_address(&sample.wind_speed, &addr);  displacements[5] = addr - base;
    MPI_Get_address(&sample.station_id, &addr);  displacements[6] = addr - base;

    MPI_Datatype structType;
    MPI_Type_create_struct(fieldCount, blockLengths, displacements, fieldTypes, &structType);

    // station_id (the last field) ends at offset 52, but sizeof(Measurement)
    // is 56 because the struct is padded to its 8-byte alignment - without
    // resizing, MPI would think each element is only 52 bytes apart and
    // step through a vector<Measurement> array with the wrong stride,
    // silently reading garbage past the first element. resizing the type's
    // extent to sizeof(Measurement) makes it match the real array layout.
    MPI_Datatype measurementType;
    MPI_Type_create_resized(structType, 0, sizeof(Measurement), &measurementType);
    MPI_Type_free(&structType);

    MPI_Type_commit(&measurementType);
    return measurementType;
}

const int tag_long_long_values = 10;
const int tag_int_values       = 11;
const int tag_double_values    = 12;

const int tag_station_counts    = 13;
const int tag_station_temp_sums = 14;
const int tag_station_rain_sums = 15;

const int tag_interval_count  = 16;
const int tag_interval_ids    = 17;
const int tag_interval_counts = 18;

// wire format for sending one Stats object to another rank - same arrays-
// by-type approach mpi.cpp uses for worker-to-master (see that file's
// comment for why: at most a handful of aggregated results ever cross
// this wire, so message count doesn't matter next to the cost of the
// measurements themselves). factored into functions here because the
// binomial tree merge below needs the SAME send/recv logic to also run
// worker-to-worker, not just worker-to-master.
void sendStats(const Stats &stats, int destRank, int S) {
    long long long_long_values_array[5] = {
        stats.count,
        stats.extreme_count,
        stats.has_measurement ? 1LL : 0LL,
        stats.hottest.timestamp,
        stats.coldest.timestamp
    };
    MPI_Send(long_long_values_array, 5, MPI_LONG_LONG, destRank, tag_long_long_values, MPI_COMM_WORLD);

    int int_values_array[2] = { stats.hottest.station_id, stats.coldest.station_id };
    MPI_Send(int_values_array, 2, MPI_INT, destRank, tag_int_values, MPI_COMM_WORLD);

    double double_values_array[15] = {
        stats.temp_sum, stats.temp_min, stats.temp_max,
        stats.humidity_sum, stats.humidity_min, stats.humidity_max,
        stats.pressure_sum, stats.pressure_min, stats.pressure_max,
        stats.rainfall_sum, stats.rainfall_max,
        stats.wind_sum, stats.wind_max,
        stats.hottest.temperature, stats.coldest.temperature
    };
    MPI_Send(double_values_array, 15, MPI_DOUBLE, destRank, tag_double_values, MPI_COMM_WORLD);

    vector<long long> stationCounts(S);
    vector<double> stationTempSums(S);
    vector<double> stationRainSums(S);
    for (int i = 0; i < S; i++) {
        stationCounts[i] = stats.stations[i].count;
        stationTempSums[i] = stats.stations[i].temp_sum;
        stationRainSums[i] = stats.stations[i].rainfall_sum;
    }
    MPI_Send(stationCounts.data(), S, MPI_LONG_LONG, destRank, tag_station_counts, MPI_COMM_WORLD);
    MPI_Send(stationTempSums.data(), S, MPI_DOUBLE, destRank, tag_station_temp_sums, MPI_COMM_WORLD);
    MPI_Send(stationRainSums.data(), S, MPI_DOUBLE, destRank, tag_station_rain_sums, MPI_COMM_WORLD);

    int localIntervalCount = (int)stats.intervals.size();
    vector<long long> intervalIds;
    vector<long long> intervalCounts;
    intervalIds.reserve(localIntervalCount);
    intervalCounts.reserve(localIntervalCount);
    for (const auto &entry : stats.intervals) {
        intervalIds.push_back(entry.first);
        intervalCounts.push_back(entry.second);
    }
    MPI_Send(&localIntervalCount, 1, MPI_INT, destRank, tag_interval_count, MPI_COMM_WORLD);
    if (localIntervalCount > 0) {
        MPI_Send(intervalIds.data(), localIntervalCount, MPI_LONG_LONG, destRank, tag_interval_ids, MPI_COMM_WORLD);
        MPI_Send(intervalCounts.data(), localIntervalCount, MPI_LONG_LONG, destRank, tag_interval_counts, MPI_COMM_WORLD);
    }
}

Stats recvStats(int srcRank, int S) {
    long long long_long_values_array[5];
    MPI_Recv(long_long_values_array, 5, MPI_LONG_LONG, srcRank, tag_long_long_values, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    int int_values_array[2];
    MPI_Recv(int_values_array, 2, MPI_INT, srcRank, tag_int_values, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    double double_values_array[15];
    MPI_Recv(double_values_array, 15, MPI_DOUBLE, srcRank, tag_double_values, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    vector<long long> stationCounts(S);
    vector<double> stationTempSums(S);
    vector<double> stationRainSums(S);
    MPI_Recv(stationCounts.data(), S, MPI_LONG_LONG, srcRank, tag_station_counts, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Recv(stationTempSums.data(), S, MPI_DOUBLE, srcRank, tag_station_temp_sums, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    MPI_Recv(stationRainSums.data(), S, MPI_DOUBLE, srcRank, tag_station_rain_sums, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    int localIntervalCount = 0;
    MPI_Recv(&localIntervalCount, 1, MPI_INT, srcRank, tag_interval_count, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

    vector<long long> intervalIds(localIntervalCount);
    vector<long long> intervalCounts(localIntervalCount);
    if (localIntervalCount > 0) {
        MPI_Recv(intervalIds.data(), localIntervalCount, MPI_LONG_LONG, srcRank, tag_interval_ids, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        MPI_Recv(intervalCounts.data(), localIntervalCount, MPI_LONG_LONG, srcRank, tag_interval_counts, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    }

    Stats result(S);
    result.count = long_long_values_array[0];
    result.extreme_count = long_long_values_array[1];
    result.has_measurement = (long_long_values_array[2] != 0);
    result.hottest.timestamp = long_long_values_array[3];
    result.coldest.timestamp = long_long_values_array[4];

    result.hottest.station_id = int_values_array[0];
    result.coldest.station_id = int_values_array[1];

    result.temp_sum = double_values_array[0]; result.temp_min = double_values_array[1]; result.temp_max = double_values_array[2];
    result.humidity_sum = double_values_array[3]; result.humidity_min = double_values_array[4]; result.humidity_max = double_values_array[5];
    result.pressure_sum = double_values_array[6]; result.pressure_min = double_values_array[7]; result.pressure_max = double_values_array[8];
    result.rainfall_sum = double_values_array[9]; result.rainfall_max = double_values_array[10];
    result.wind_sum = double_values_array[11]; result.wind_max = double_values_array[12];
    result.hottest.temperature = double_values_array[13];
    result.coldest.temperature = double_values_array[14];

    for (int i = 0; i < S; i++) {
        result.stations[i].count = stationCounts[i];
        result.stations[i].temp_sum = stationTempSums[i];
        result.stations[i].rainfall_sum = stationRainSums[i];
    }

    for (int i = 0; i < localIntervalCount; i++) {
        result.intervals[intervalIds[i]] = intervalCounts[i];
    }

    return result;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    // same I/O optimization as sequential.cpp - only the master actually
    // reads input, but this was missing here, so cin>> was paying full
    // C-stream-sync overhead while parsing millions of records serially,
    // which dominated total runtime regardless of process count.
    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);

    int rank, P;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &P);

    double t_start = MPI_Wtime();
    double read_time = 0.0, scatter_time = 0.0, compute_time = 0.0, tree_merge_time = 0.0, gather_merge_time = 0.0;

    if(P < 2){
        // this is invalid as per our design decision
        fprintf(stderr, "Error: P must be at least 2\n");
        MPI_Finalize();
        return 1;
    }

    int numWorkers = P - 1;
    int N, K, S;
    bool isMaster = (rank == 0);
    int hasInput = 1;
    if(isMaster){
        if(!(cin >> N >> K >> S)){
            hasInput = 0;
        }
    }

    MPI_Bcast(&hasInput, 1, MPI_INT, 0, MPI_COMM_WORLD);
    if(!hasInput){
        MPI_Finalize();
        return 0;
    }

    // here i need to MPI_Bcast() to send the data of N, K and S to all the workers
    MPI_Bcast(&N, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&K, 1, MPI_INT, 0, MPI_COMM_WORLD);
    MPI_Bcast(&S, 1, MPI_INT, 0, MPI_COMM_WORLD);

    if(N == 0){
        if(isMaster){
            cout << "TOTAL_MEASUREMENTS 0" << endl;
        }
        MPI_Finalize();
        return 0;
    }
    MPI_Datatype measurementType = createMeasurementType();

    // preparing the sendcounts and displs array for the scatterv function
    vector<int> sendcounts(P, 0);
    vector<int> displs(P, 0);
    for (int worker = 0; worker < numWorkers; worker++) {
        auto [begin, end] = getPartition(worker, numWorkers, N);
        int workerRank = worker + 1;
        sendcounts[workerRank] = end - begin;
        displs[workerRank] = begin;
    }
    // this assignment beautifully maintains sendcounts[0] and displs[0] as 0, cause rank 0 is master process and will not process any data as per our design decision

    // now lets do data distribution
    // first lets get the input data using master
    vector<Measurement> inputData;
    if(isMaster){
        double t_read_start = MPI_Wtime();
        inputData.resize(N);
        for (int i = 0; i < N; i++) {
            cin >> inputData[i].timestamp
                >> inputData[i].station_id
                >> inputData[i].temperature
                >> inputData[i].humidity
                >> inputData[i].pressure
                >> inputData[i].rainfall
                >> inputData[i].wind_speed;
        }
        read_time = MPI_Wtime() - t_read_start;
    }

    int localDataCount = sendcounts[rank];
    vector<Measurement> localData(localDataCount);

    double t_scatter_start = MPI_Wtime();
    MPI_Scatterv(isMaster ? inputData.data() : nullptr, sendcounts.data(), displs.data(), measurementType, localData.data(), localDataCount, measurementType, 0, MPI_COMM_WORLD);
    scatter_time = MPI_Wtime() - t_scatter_start;

    MPI_Type_free(&measurementType);

    // now that wach worker has got its share of data, let them do their computation
    Stats localStats(S);
    if (!isMaster) {
        double t_compute_start = MPI_Wtime();
        localStats = computeRangeOnWorker(localData, 0, localDataCount, S);
        compute_time = MPI_Wtime() - t_compute_start;
    }

    // this is the global state of all the stations which we will keep updating using the localstats that we receive from all the distributed processes
    Stats globalStats(S);

    // worker part - binomial tree merge among the workers themselves before
    // anything reaches the master, instead of every worker sending straight
    // to the master one at a time (that's what mpi.cpp does). with
    // numWorkers workers this takes O(log numWorkers) sequential rounds
    // instead of O(numWorkers), and different pairs combine simultaneously
    // within a round instead of the master doing every combine itself.
    // widx is each worker's own 0-based index among workers (global rank -
    // 1); the tree always finishes with the merged result sitting on
    // widx==0 (global rank 1), which then makes the single hop to the
    // master, same send/recv wire format as before (see sendStats/recvStats
    // above), and mergingWorkerProcessStats is unchanged too, so an empty
    // worker (has_measurement=false) is still handled exactly as it always
    // was.
    if (!isMaster) {
        double t_tree_start = MPI_Wtime();
        int widx = rank - 1;
        for (int step = 1; step < numWorkers; step *= 2) {
            if (widx % (2 * step) == 0) {
                int partnerWidx = widx + step;
                if (partnerWidx < numWorkers) {
                    Stats received = recvStats(partnerWidx + 1, S);
                    mergingWorkerProcessStats(localStats, received);
                }
            } else if (widx % step == 0) {
                int partnerWidx = widx - step;
                sendStats(localStats, partnerWidx + 1, S);
                break;
            }
        }

        if (widx == 0) {
            sendStats(localStats, 0, S);
        }
        tree_merge_time = MPI_Wtime() - t_tree_start;
    }

    if (isMaster) {
        double t_gather_start = MPI_Wtime();
        Stats workerStats = recvStats(1, S);
        mergingWorkerProcessStats(globalStats, workerStats);
        gather_merge_time = MPI_Wtime() - t_gather_start;

        printResults(globalStats, K);
    }

    // timing breakdown, one single summary line (not one per rank), to
    // stderr only - never touches the required stdout output. read_input
    // and gather_merge only ever happen on the master, so its own value is
    // already the right number. scatter, compute, and tree_merge happen on
    // every worker rank, so we take the max across all ranks - that's the
    // one that actually sets the critical path/wall-clock time (a scatter
    // time dominated by workers idle-waiting for the master to finish
    // reading is exactly what we want visible here, not averaged away).
    double max_scatter_time, max_compute_time, max_tree_merge_time;
    MPI_Reduce(&scatter_time, &max_scatter_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&compute_time, &max_compute_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&tree_merge_time, &max_tree_merge_time, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);

    if (isMaster) {
        double total_time = MPI_Wtime() - t_start;
        fprintf(stderr, "[N=%d P=%d] total=%.3fs read_input=%.3fs scatter=%.3fs compute=%.3fs tree_merge=%.3fs gather_merge=%.3fs\n",
                N, P, total_time, read_time, max_scatter_time, max_compute_time, max_tree_merge_time, gather_merge_time);
    }

    MPI_Finalize();
    return 0;
}

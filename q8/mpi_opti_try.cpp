#include "q8_common.hpp"
#include <mpi.h>
#include <cstdio>
#include <iostream>
#include <fstream>
#include <string>
#include <limits>
using namespace std;

// EXPERIMENTAL variant of mpi.cpp. In mpi.cpp, the master alone reads and
// parses the *entire* input serially, then MPI_Scatterv's it out - and
// benchmarking showed that serial parse dominates wall-clock time at every
// P (P=2,3,5 all land around the same time at large N), capping how much
// speedup more workers can ever give (Amdahl's law: no amount of
// downstream parallelism fixes a serial bottleneck at the front).
//
// Here, instead, every worker independently opens the input file and reads
// only its own byte-range straight off disk, in parallel - no bulk data
// ever crosses MPI, only the small aggregated Stats results at the end
// (same send/recv protocol as mpi.cpp, unchanged).
//
// This needs a real file path, not stdin - MPI only reliably forwards
// stdin to rank 0, but every rank here needs independent seek access.
// usage: mpirun -np P ./mpi_opti_try <input-file>
//
// Known consequence: since byte-range partitioning doesn't land on the
// same record-count boundaries as sequential.cpp's fixed 8-worker
// simulation (records have variable byte length), this can diverge from
// sequential's output in the same way already documented in README.md
// (floating-point summation order) even at P=9, where mpi.cpp always
// matched exactly - that's expected here, not a new bug.
//
// STATUS: draft, untested. Not verified against verify_correctness.sh,
// not benchmarked. See README.md "Future improvements".

const int tag_long_long_values = 10;
const int tag_int_values       = 11;
const int tag_double_values    = 12;

const int tag_station_counts    = 13;
const int tag_station_temp_sums = 14;
const int tag_station_rain_sums = 15;

const int tag_interval_count  = 16;
const int tag_interval_ids    = 17;
const int tag_interval_counts = 18;

// returns the byte offset of the start of the next full record at or after
// `pos` (clamped to [dataStart, dataEnd]). this is a pure function of
// (path, pos) - so two neighboring workers, calling it with the exact same
// pos (one as its range end, the other as its range start), always land on
// the identical boundary. that's what keeps ranges non-overlapping and
// gap-free without any worker having to talk to another.
long long findRecordBoundary(const string &path, long long pos, long long dataStart, long long dataEnd) {
    if (pos <= dataStart) return dataStart;
    if (pos >= dataEnd) return dataEnd;

    ifstream fin(path, ios::binary);
    fin.seekg(pos);
    string discardedPartialLine;
    if (!getline(fin, discardedPartialLine)) {
        return dataEnd; // pos was already inside the last, unterminated line
    }
    long long result = (long long)fin.tellg();
    if (result < 0) {
        return dataEnd; // getline consumed straight through to EOF, no trailing newline
    }
    return result;
}

int main(int argc, char **argv) {
    MPI_Init(&argc, &argv);

    ios_base::sync_with_stdio(false);
    cin.tie(nullptr);

    int rank, P;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &P);
    bool isMaster = (rank == 0);

    if (P < 2) {
        fprintf(stderr, "Error: P must be at least 2\n");
        MPI_Finalize();
        return 1;
    }

    if (argc < 2) {
        if (isMaster) fprintf(stderr, "Usage: %s <input-file>\n", argv[0]);
        MPI_Finalize();
        return 1;
    }
    string path = argv[1];

    int numWorkers = P - 1;

    // every rank independently opens the same read-only file and reads the
    // header itself - deterministic given the file, so this needs no
    // MPI_Bcast at all (unlike mpi.cpp, which broadcasts N/K/S from master).
    int N, K, S;
    long long dataStart, dataEnd;
    {
        ifstream fin(path, ios::binary);
        if (!(fin >> N >> K >> S)) {
            // no header at all - mirrors mpi.cpp's hasInput=false path (no output)
            MPI_Finalize();
            return 0;
        }
        fin.ignore(numeric_limits<streamsize>::max(), '\n');
        dataStart = (long long)fin.tellg();
        fin.seekg(0, ios::end);
        dataEnd = (long long)fin.tellg();
    }

    if (N == 0) {
        if (isMaster) cout << "TOTAL_MEASUREMENTS 0" << endl;
        MPI_Finalize();
        return 0;
    }

    Stats localStats(S);
    if (!isMaster) {
        int r = rank; // 1..numWorkers
        long long dataLen = dataEnd - dataStart;
        long long splitBefore = dataStart + dataLen * (long long)(r - 1) / numWorkers;
        long long splitAfter  = dataStart + dataLen * (long long)r / numWorkers;

        long long actualStart = (r == 1) ? dataStart : findRecordBoundary(path, splitBefore, dataStart, dataEnd);
        long long actualEnd   = (r == numWorkers) ? dataEnd : findRecordBoundary(path, splitAfter, dataStart, dataEnd);

        vector<Measurement> localData;
        ifstream fin(path, ios::binary);
        fin.seekg(actualStart);
        while (true) {
            fin >> ws; // skip whitespace/newlines to land exactly at the next record's start
            long long curPos = (long long)fin.tellg();
            if (curPos < 0 || curPos >= actualEnd) break;

            Measurement m;
            if (!(fin >> m.timestamp >> m.station_id >> m.temperature
                      >> m.humidity >> m.pressure >> m.rainfall >> m.wind_speed)) {
                break;
            }
            localData.push_back(m);
        }

        localStats = computeRangeOnWorker(localData, 0, (int)localData.size(), S);
    }

    Stats globalStats(S);

    // worker part - identical send protocol to mpi.cpp, unchanged
    if (!isMaster) {
        long long long_long_values_array[5] = {
            localStats.count,
            localStats.extreme_count,
            localStats.has_measurement ? 1LL : 0LL,
            localStats.hottest.timestamp,
            localStats.coldest.timestamp
        };
        MPI_Send(long_long_values_array, 5, MPI_LONG_LONG, 0, tag_long_long_values, MPI_COMM_WORLD);

        int int_values_array[2] = { localStats.hottest.station_id, localStats.coldest.station_id };
        MPI_Send(int_values_array, 2, MPI_INT, 0, tag_int_values, MPI_COMM_WORLD);

        double double_values_array[15] = {
            localStats.temp_sum, localStats.temp_min, localStats.temp_max,
            localStats.humidity_sum, localStats.humidity_min, localStats.humidity_max,
            localStats.pressure_sum, localStats.pressure_min, localStats.pressure_max,
            localStats.rainfall_sum, localStats.rainfall_max,
            localStats.wind_sum, localStats.wind_max,
            localStats.hottest.temperature, localStats.coldest.temperature
        };
        MPI_Send(double_values_array, 15, MPI_DOUBLE, 0, tag_double_values, MPI_COMM_WORLD);

        vector<long long> stationCounts(S);
        vector<double> stationTempSums(S);
        vector<double> stationRainSums(S);
        for (int i = 0; i < S; i++) {
            stationCounts[i] = localStats.stations[i].count;
            stationTempSums[i] = localStats.stations[i].temp_sum;
            stationRainSums[i] = localStats.stations[i].rainfall_sum;
        }
        MPI_Send(stationCounts.data(), S, MPI_LONG_LONG, 0, tag_station_counts, MPI_COMM_WORLD);
        MPI_Send(stationTempSums.data(), S, MPI_DOUBLE, 0, tag_station_temp_sums, MPI_COMM_WORLD);
        MPI_Send(stationRainSums.data(), S, MPI_DOUBLE, 0, tag_station_rain_sums, MPI_COMM_WORLD);

        int localIntervalCount = (int)localStats.intervals.size();
        vector<long long> intervalIds;
        vector<long long> intervalCounts;
        intervalIds.reserve(localIntervalCount);
        intervalCounts.reserve(localIntervalCount);
        for (const auto &entry : localStats.intervals) {
            intervalIds.push_back(entry.first);
            intervalCounts.push_back(entry.second);
        }
        MPI_Send(&localIntervalCount, 1, MPI_INT, 0, tag_interval_count, MPI_COMM_WORLD);
        if (localIntervalCount > 0) {
            MPI_Send(intervalIds.data(), localIntervalCount, MPI_LONG_LONG, 0, tag_interval_ids, MPI_COMM_WORLD);
            MPI_Send(intervalCounts.data(), localIntervalCount, MPI_LONG_LONG, 0, tag_interval_counts, MPI_COMM_WORLD);
        }
    }

    if (isMaster) {
        for (int workerRank = 1; workerRank < P; workerRank++) {
            long long long_long_values_array[5];
            MPI_Recv(long_long_values_array, 5, MPI_LONG_LONG, workerRank, tag_long_long_values, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            int int_values_array[2];
            MPI_Recv(int_values_array, 2, MPI_INT, workerRank, tag_int_values, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            double double_values_array[15];
            MPI_Recv(double_values_array, 15, MPI_DOUBLE, workerRank, tag_double_values, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            vector<long long> stationCounts(S);
            vector<double> stationTempSums(S);
            vector<double> stationRainSums(S);
            MPI_Recv(stationCounts.data(), S, MPI_LONG_LONG, workerRank, tag_station_counts, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(stationTempSums.data(), S, MPI_DOUBLE, workerRank, tag_station_temp_sums, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            MPI_Recv(stationRainSums.data(), S, MPI_DOUBLE, workerRank, tag_station_rain_sums, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            int localIntervalCount = 0;
            MPI_Recv(&localIntervalCount, 1, MPI_INT, workerRank, tag_interval_count, MPI_COMM_WORLD, MPI_STATUS_IGNORE);

            vector<long long> intervalIds(localIntervalCount);
            vector<long long> intervalCounts(localIntervalCount);
            if (localIntervalCount > 0) {
                MPI_Recv(intervalIds.data(), localIntervalCount, MPI_LONG_LONG, workerRank, tag_interval_ids, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                MPI_Recv(intervalCounts.data(), localIntervalCount, MPI_LONG_LONG, workerRank, tag_interval_counts, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            }

            Stats workerStats(S);
            workerStats.count = long_long_values_array[0];
            workerStats.extreme_count = long_long_values_array[1];
            workerStats.has_measurement = (long_long_values_array[2] != 0);
            workerStats.hottest.timestamp = long_long_values_array[3];
            workerStats.coldest.timestamp = long_long_values_array[4];

            workerStats.hottest.station_id = int_values_array[0];
            workerStats.coldest.station_id = int_values_array[1];

            workerStats.temp_sum = double_values_array[0]; workerStats.temp_min = double_values_array[1]; workerStats.temp_max = double_values_array[2];
            workerStats.humidity_sum = double_values_array[3]; workerStats.humidity_min = double_values_array[4]; workerStats.humidity_max = double_values_array[5];
            workerStats.pressure_sum = double_values_array[6]; workerStats.pressure_min = double_values_array[7]; workerStats.pressure_max = double_values_array[8];
            workerStats.rainfall_sum = double_values_array[9]; workerStats.rainfall_max = double_values_array[10];
            workerStats.wind_sum = double_values_array[11]; workerStats.wind_max = double_values_array[12];
            workerStats.hottest.temperature = double_values_array[13];
            workerStats.coldest.temperature = double_values_array[14];

            for (int i = 0; i < S; i++) {
                workerStats.stations[i].count = stationCounts[i];
                workerStats.stations[i].temp_sum = stationTempSums[i];
                workerStats.stations[i].rainfall_sum = stationRainSums[i];
            }

            for (int i = 0; i < localIntervalCount; i++) {
                workerStats.intervals[intervalIds[i]] = intervalCounts[i];
            }

            mergingWorkerProcessStats(globalStats, workerStats);
        }

        printResults(globalStats, K);
    }

    MPI_Finalize();
    return 0;
}

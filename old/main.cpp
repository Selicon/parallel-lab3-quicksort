#include <random>
#include <mpi.h>
#include <tuple>
#include <iostream>

using namespace std;

vector<int> generateArray(int size) {
    std::default_random_engine generator(1224);
    std::uniform_int_distribution<int> distribution(0, 20);

    std::vector<int> values;
    values.reserve(size);

    for(int i = 0; i < size; i++) {
        int value = distribution(generator);
        values.push_back(value);
    }

    return values;
}

bool isSorted(const vector<int> &array) {
    for(size_t i = 0; i < array.size() - 1; i++) {
        if(array[i] > array[i+1]) {
            return false;
        }
    }

    return true;
}

void quickSort(vector<int> &arr, int low, int high) {
    int temp;
    int pivot = arr[(low + high) / 2];

    size_t i = low;
    size_t j = high;

    do {
        while (arr[i] < pivot) {
            i++;
        }

        while (arr[j] > pivot) {
            j--;
        }

        if (i <= j) {
            if (i < j) {
                temp = arr[i];
                arr[i] = arr[j];
                arr[j] = temp;
                i++;
                j--;
            } else {
                i++;
            }
        }

    } while (i <= j);

    if (low < j) {
        quickSort(arr, low, j);
    }
    if (i < high) {
        quickSort(arr, i , high);
    }
}

std::tuple<vector<int>, vector<int>> subdivide(const vector<int> &arr, int pivot) {
    vector<int> bigger = vector<int>();
    vector<int> less_than = vector<int>();
    for (size_t i = 0; i < arr.size(); i++) {
        if (arr[i] > pivot) {
            bigger.push_back(arr[i]);
        } else {
            less_than.push_back(arr[i]);
        }
    }

    return std::make_tuple(less_than, bigger);
}

void qsort_iteration(int rank, int size, vector<int> &arr, int iter_num, int max_iter_num) {

    MPI_Status status;
    int pivot;

    int start_bit = 1;
    start_bit <<= (int)(log2(size)) - iter_num;

    int process_index_to_connect = rank ^ start_bit;
    if (iter_num == 1) {

        if (rank == 0) {
            pivot = arr[arr.size() / 2];
        }

        MPI_Bcast(&pivot, 1, MPI_INT, 0, MPI_COMM_WORLD);

    } else {
        int group_count = (int)pow(2, iter_num - 1);
        int process_per_group = size / group_count;
        int group_number = rank / process_per_group;
        if ((rank % process_per_group) == 0) {
            pivot = arr[arr.size() / 2];
            for (int i = 1; i < process_per_group; i++) {
                MPI_Send(&pivot, 1, MPI_INT, group_number * process_per_group + i, 0, MPI_COMM_WORLD);
            }
        } else {
            MPI_Recv(&pivot, 1, MPI_INT, group_number * process_per_group, 0, MPI_COMM_WORLD, &status);
        }
    }

    std::tuple<vector<int>, vector<int>> subdivision = subdivide(arr, pivot);
    vector<int> less_than = std::get<0>(subdivision);
    vector<int> bigger = std::get<1>(subdivision);

    size_t buf_size;

    vector<int> iter_part_array;

    if (rank < process_index_to_connect ) {
        buf_size = bigger.size();

        MPI_Send(&buf_size, 1, MPI_INT, process_index_to_connect, rank, MPI_COMM_WORLD); //send size
        MPI_Send(bigger.data(), buf_size, MPI_INT, process_index_to_connect, rank, MPI_COMM_WORLD); //send bigger elements

        MPI_Recv(&buf_size, 1, MPI_INT, process_index_to_connect, process_index_to_connect, MPI_COMM_WORLD, &status); //accept size

        vector<int> another_lesser(buf_size);

        MPI_Recv(another_lesser.data(), buf_size, MPI_INT, process_index_to_connect, process_index_to_connect, MPI_COMM_WORLD, &status); //accept less elements

        iter_part_array = vector<int>(buf_size + less_than.size(), 0);

        size_t i = 0;
        size_t j = 0;

        while ((i <= less_than.size()) && (j <= buf_size)) {
            if (i == less_than.size() || (j == buf_size)) {
                if (i == less_than.size()) {
                    for (size_t k = j; k < buf_size; k++) {
                        iter_part_array[i + k] = another_lesser[k];
                    }
                } else if (j == buf_size) {
                    for (size_t k = i; k < less_than.size(); k++) {
                        iter_part_array[j + k] = less_than[k];
                    }
                }
                break;
            } else {
                if (less_than.at(i) < another_lesser[j]) {
                    iter_part_array[i + j] = less_than.at(i);
                    i++;
                } else {
                    iter_part_array[i + j] = another_lesser[j];
                    j++;
                }
            }

        }

        arr = iter_part_array;

    } else {
        buf_size = bigger.size();

        MPI_Recv(&buf_size, 1, MPI_INT, process_index_to_connect, process_index_to_connect, MPI_COMM_WORLD, &status); //wrong message index

        vector<int> another_bigger(buf_size);

        MPI_Recv(another_bigger.data(), buf_size, MPI_INT, process_index_to_connect, process_index_to_connect, MPI_COMM_WORLD, &status);

        iter_part_array = vector<int>(buf_size + bigger.size(), 0);

        size_t i = 0;
        size_t j = 0;
        while ((i <= bigger.size()) && (j <= buf_size)) {
            if (i == bigger.size() || (j == buf_size)) {
                if (i == bigger.size()) {
                    for (size_t k = j; k < buf_size; k++) {
                        iter_part_array[k + i] = another_bigger[k];
                    }
                } else if (j == buf_size) {
                    for (size_t k = i; k < bigger.size(); k++) {
                        iter_part_array[k + j] = bigger.at(k);
                    }
                }
                break;
            } else {
                if (bigger.at(i) < another_bigger[j]) {
                    iter_part_array[i + j] = bigger.at(i);
                    i++;
                } else {
                    iter_part_array[i + j] = another_bigger[j];
                    j++;
                }
            }

        }

        buf_size = less_than.size();

        MPI_Send(&buf_size, 1, MPI_INT, process_index_to_connect, rank, MPI_COMM_WORLD); //send size
        MPI_Send(less_than.data(), buf_size, MPI_INT, process_index_to_connect, rank, MPI_COMM_WORLD);

        arr = iter_part_array;
    }

    MPI_Barrier(MPI_COMM_WORLD);

    if (iter_num < max_iter_num) {
        qsort_iteration(rank, size, arr, iter_num + 1, max_iter_num);
    }

}

int main(int argc, char **argv) {
    MPI_Status status;

    int arr_size = 30'000'000;
    vector<int> array;

    int rank, size;
    MPI_Init (&argc, &argv);

    MPI_Comm_rank (MPI_COMM_WORLD, &rank);
    MPI_Comm_size (MPI_COMM_WORLD, &size);

    int chunkSize = ceil((double) arr_size / size);

    int first = rank * chunkSize;
    int last = min(arr_size - 1, first + chunkSize - 1);



    if (rank == 0) {
        array = generateArray(arr_size);

        for (int i = 1; i < size; i++) {
            MPI_Send(array.data() + chunkSize * i, chunkSize, MPI_INT, i, 0, MPI_COMM_WORLD);
        }
    } else {
        array = vector<int>(arr_size);

        MPI_Recv(array.data() + first, chunkSize, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
    }

    double start = MPI_Wtime();

    quickSort(array, first, last);

    vector<int> current_values = vector<int>(1 + last - first, 0);

    for (int i = first; i <= last; i++) {
        current_values[i - rank * chunkSize] = array[i];
    }

    int iter_count = (int)log2(size);

    if(size > 1) {
        qsort_iteration(rank, size, current_values, 1, iter_count);
    }

    vector<int> result;

    if(rank == 0) {
        cout << MPI_Wtime() - start << endl;
    }

/*
    for(int r = 0; r < size; r++) {
        if(rank == r) {
            cout << r << ": ";
            for (size_t i = 0; i < current_values.size(); i++) {
                cout << current_values[i] << " ";
            }
            cout << endl;
        }

        MPI_Barrier(MPI_COMM_WORLD);
    }
*/
	MPI_Finalize();

	return 0;
}

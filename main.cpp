#include <random>
#include <mpi.h>
#include <tuple>
#include <algorithm>
#include <iostream>

using namespace std;

vector<int> generateArray(size_t size)
{
    std::default_random_engine generator(1224);
    std::uniform_int_distribution<int> distribution(0, 20);

    std::vector<int> values;
    values.reserve(size);

    for (size_t i = 0; i < size; i++)
    {
        int value = distribution(generator);
        values.push_back(value);
    }

    return values;
}

bool isSorted(const vector<int> &array)
{
    for (size_t i = 0; i < array.size() - 1; i++)
    {
        if (array[i] > array[i + 1])
        {
            return false;
        }
    }

    return true;
}

void subdivide(vector<int> &arr, int pivot, int **secondPart)
{
    auto a = std::partition(arr.begin(), arr.end(), [pivot](int i) { return i < pivot; });
    *secondPart = &*a;
}

void qsort_iteration(MPI_Comm group, int rank, int size, vector<int> &arr, int iter_num, int max_iter_num)
{

    MPI_Status status;
    int pivot;
    int start_bit = 1;
    start_bit <<= (int)(log2(size)) - iter_num;

    int group_count = (int)pow(2, iter_num - 1);
    int process_per_group = size / group_count;
    int group_number = rank / process_per_group;
    int group_rank = rank ^ (group_number << start_bit);
    int process_index_to_connect = group_rank ^ start_bit;

    MPI_Comm row_comm;
    MPI_Comm_split(group, group_number, group_rank, &row_comm);

    if (group_rank == 0)
    {
        pivot = arr[arr.size() / 2];
    }

    MPI_Bcast(&pivot, 1, MPI_INT, 0, row_comm);

    int *borders[3];
    borders[0] = &*arr.begin();
    borders[2] = &*arr.end();

    subdivide(arr, pivot, borders + 1);

    auto to_send = group_rank > process_index_to_connect ? 0 : 1;
    auto to_recv = group_rank > process_index_to_connect ? 1 : 0;
    size_t buf_size = borders[to_send + 1] - borders[to_send];
    size_t recv_size = borders[to_recv + 1] - borders[to_recv];

    MPI_Request request;

    MPI_Isend(&buf_size, 1, MPI_INT, process_index_to_connect, 0, row_comm, &request);
    MPI_Isend(borders[to_send], buf_size, MPI_INT, process_index_to_connect, 1, row_comm, &request);

    MPI_Recv(&buf_size, 1, MPI_INT, process_index_to_connect, 0, row_comm, &status);

    vector<int> another(buf_size);

    MPI_Recv(another.data(), buf_size, MPI_INT, process_index_to_connect, 1, row_comm, &status);

    auto tmp = vector<int>();

    tmp.insert(tmp.end(), borders[to_recv], borders[to_recv + 1]);
    tmp.insert(tmp.end(), another.begin(), another.end());

    arr = tmp;

    if (iter_num < max_iter_num)
    {
        qsort_iteration(row_comm, rank, size, arr, iter_num + 1, max_iter_num);
    }
}

int main(int argc, char **argv)
{
    MPI_Status status;

    int arr_size = 30'000'000;
    vector<int> array;

    int rank, size;
    MPI_Init(&argc, &argv);

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    int chunkSize = ceil((double)arr_size / size);

    int first = rank * chunkSize;
    int last = min(arr_size - 1, first + chunkSize - 1);

    if (rank == 0)
    {
        array = generateArray(arr_size);

        for (int i = 1; i < size; i++)
        {
            MPI_Send(array.data() + chunkSize * i, chunkSize, MPI_INT, i, 0, MPI_COMM_WORLD);
        }
    }
    else
    {
        array = vector<int>(arr_size);

        MPI_Recv(array.data() + first, chunkSize, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
    }

    double start = MPI_Wtime();

    vector<int> current_values = vector<int>(1 + last - first, 0);

    for (int i = first; i <= last; i++)
    {
        current_values[i - rank * chunkSize] = array[i];
    }

    int iter_count = (int)log2(size);

    if (size > 1)
    {
        qsort_iteration(MPI_COMM_WORLD, rank, size, current_values, 1, iter_count);
    }

    vector<int> result;

    if (rank == 0)
    {
        cout << MPI_Wtime() - start << endl;
    }

    std::sort(current_values.begin(), current_values.end());

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
    }//*/

    MPI_Finalize();

    return 0;
}

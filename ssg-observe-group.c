/*
 * Copyright (c) 2016 UChicago Argonne, LLC
 *
 * See COPYRIGHT in top-level directory.
 */

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <mpi.h>

#include <margo.h>
#include <ssg.h>

#define DIE_IF(cond_expr, err_fmt, ...) \
    do { \
        if (cond_expr) { \
            fprintf(stderr, "ERROR at %s:%d (" #cond_expr "): " \
                    err_fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
            exit(1); \
        } \
    } while(0)


#include <time.h>
double my_wtime() {
    struct timeval time;
    gettimeofday(&time, NULL);
    return time.tv_sec + time.tv_usec/1000000.0;
}
DECLARE_MARGO_RPC_HANDLER(group_id_forward_recv_ult)



static void usage()
{
    fprintf(stderr,
        "Usage: "
        "ssg-observe-group <addr> <GID>\n"
        "Observe group given by GID using Mercury address ADDR.\n");
}

static void parse_args(int argc, char *argv[], const char **addr_str, const char **gid_file)
{
    int ndx = 1;

    if (argc < 2)
    {
        usage();
        exit(1);
    }

    *addr_str = argv[ndx++];
    *gid_file = argv[ndx++];

    return;   
}

void dump_histogram(double *values, int count)
{
#define NR_BINS 5
    int i;
    double max=values[0];
    double min=values[0];
    int bins[NR_BINS];

    for (i=0; i< NR_BINS; i++)
        bins[i] = 0;

    for (i=1; i<count; i++) {
        if (values[i] > max)
            max = values[i];
        if (values[i] < min) {
            min = values[i];
        }
    }

    double step = (max-min)/(double)NR_BINS;

    for (i=0; i< count; i++) {
        int bin = (values[i]-min)/step;
        if (bin == NR_BINS) bin--;
        bins[bin]++;
    }
    for(i=0; i< NR_BINS; i++)
        printf("%f-%f : %d\n", min+step*i, min+step*i+step, bins[i]);
}
int main(int argc, char *argv[])
{
    margo_instance_id mid = MARGO_INSTANCE_NULL;
    const char *addr_str;
    const char *gid_file;
    ssg_group_id_t g_id;
    int num_addrs;
    int sret;
    double init_time, load_time, observe_time;
    int nprocs, rank;
    double min_load, max_load, sum_load;
    double min_observe, max_observe, sum_observe;
    double min_init, max_init, sum_init;

    parse_args(argc, argv, &addr_str, &gid_file);

    init_time = my_wtime();
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &nprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    /* init margo */
    /* use the main xstream to drive progress & run handlers */
    mid = margo_init(addr_str, MARGO_CLIENT_MODE, 0, -1);
    DIE_IF(mid == MARGO_INSTANCE_NULL, "margo_init");

    /* initialize SSG */
    sret = ssg_init();
    DIE_IF(sret != SSG_SUCCESS, "ssg_init");
    init_time = my_wtime() - init_time;

    num_addrs = SSG_ALL_MEMBERS;
    load_time = my_wtime();
    sret = ssg_group_id_load(gid_file, &num_addrs, &g_id);
    load_time = my_wtime() - load_time;
    DIE_IF(sret != SSG_SUCCESS, "ssg_group_id_load");
    DIE_IF(num_addrs < 1, "ssg_group_id_load");

    observe_time = my_wtime();
    /* start observging the SSG server group */
    sret = ssg_group_refresh(mid, g_id);
    observe_time = my_wtime() - observe_time;

    DIE_IF(sret != SSG_SUCCESS, "ssg_group_observe");

    /* have everyone dump their group state */
    if (rank == 0) ssg_group_dump(g_id);

    /* clean up */
    ssg_finalize();
    margo_finalize(mid);

    MPI_Reduce(&load_time, &max_load, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&load_time, &min_load, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&load_time, &sum_load, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    MPI_Reduce(&observe_time, &max_observe, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&observe_time, &min_observe, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&observe_time, &sum_observe, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    MPI_Reduce(&init_time, &max_init, 1, MPI_DOUBLE, MPI_MAX, 0, MPI_COMM_WORLD);
    MPI_Reduce(&init_time, &min_init, 1, MPI_DOUBLE, MPI_MIN, 0, MPI_COMM_WORLD);
    MPI_Reduce(&init_time, &sum_init, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD);

    /* histogram of results */
    double *all_observes;
    all_observes=malloc(nprocs*sizeof(*all_observes));
    MPI_Gather(&observe_time, 1, MPI_DOUBLE,
            all_observes, 1, MPI_DOUBLE,
            0, MPI_COMM_WORLD);

   if (rank == 0) {

        dump_histogram(all_observes, nprocs);

        printf(" %d : init average (min max): %f ( %f %f )\n", nprocs, sum_init/nprocs, min_init, max_init);
        printf(" %d : load average (min max): %f ( %f %f )\n", nprocs, sum_load/nprocs, min_load, max_load);
        printf(" %d : observe average (min max): %f ( %f %f )\n", nprocs, sum_observe/nprocs, min_observe, max_observe);
    }

    MPI_Finalize();

    return 0;
}

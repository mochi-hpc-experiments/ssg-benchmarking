/*
 * Copyright (c) 2016 UChicago Argonne, LLC
 *
 * See COPYRIGHT in top-level directory.
 */

#define SSG_HAVE_MPI

#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#ifdef SSG_HAVE_MPI
#include <mpi.h>
#endif
#ifdef SSG_HAVE_PMIX
#include <pmix.h>
#endif

#include <margo.h>
#include <ssg.h>
#ifdef SSG_HAVE_MPI
#include <ssg-mpi.h>
#endif
#ifdef SSG_HAVE_PMIX
#include <ssg-pmix.h>
#endif

#define DIE_IF(cond_expr, err_fmt, ...) \
    do { \
        if (cond_expr) { \
            fprintf(stderr, "ERROR at %s:%d (" #cond_expr "): " \
                    err_fmt "\n", __FILE__, __LINE__, ##__VA_ARGS__); \
            exit(EXIT_FAILURE); \
        } \
    } while(0)

struct group_launch_opts
{
    char *addr_str;
    char *group_mode;
    int shutdown_time;
    char *gid_file;
    char *group_name;
};

static void usage()
{
    fprintf(stderr,
        "Usage: "
        "ssg-launch-group [OPTIONS] <ADDR> <MODE>\n"
        "Create and launch group using given Mercury ADDR string and group create MODE (\"mpi\" or \"pmix\").\n"
        "\n"
        "OPTIONS:\n"
        "\t-s <TIME>\t\tTime duration (in seconds) to run the group before shutting down\n"
        "\t-f <FILE>\t\tFile path to store group ID in\n"
        "\t-n <NAME>\t\tName of the group to launch\n");
}

static void parse_args(int argc, char *argv[], struct group_launch_opts *opts)
{
    int c;
    const char *options = "s:f:n:";
    char *check = NULL;

    while ((c = getopt(argc, argv, options)) != -1)
    {
        switch (c)
        {
            case 's':
                opts->shutdown_time = (int)strtol(optarg, &check, 0);
                if (opts->shutdown_time < 0 || (check && *check != '\0'))
                {
                    usage();
                    exit(EXIT_FAILURE);
                }
                break;
            case 'f':
                opts->gid_file = optarg;
                break;
            case 'n':
                opts->group_name = optarg;
                break;
            default:
                usage();
                exit(EXIT_FAILURE);
        }
    }

    if ((argc - optind) < 2)
    {
        usage();
        exit(EXIT_FAILURE);
    }

    opts->addr_str = argv[optind++];
    opts->group_mode = argv[optind++];
    if (strcmp(opts->group_mode, "mpi") == 0)
    {
#ifdef SSG_HAVE_MPI
        if (optind != argc)
        {
            usage();
            exit(EXIT_FAILURE);
        }
#else
        fprintf(stderr, "Error: MPI support not built in\n");
        exit(EXIT_FAILURE);
#endif
    }
    else if (strcmp(opts->group_mode, "pmix") == 0)
    {
#ifdef SSG_HAVE_PMIX
        if (optind != argc)
        {
            usage();
            exit(EXIT_FAILURE);
        }
#else
        fprintf(stderr, "Error: PMIx support not built in\n");
        exit(EXIT_FAILURE);
#endif
    }
    else
    {
        usage();
        exit(EXIT_FAILURE);
    }

    return;
}

int main(int argc, char *argv[])
{
    struct group_launch_opts opts;
    margo_instance_id mid = MARGO_INSTANCE_NULL;
    ssg_group_id_t g_id = SSG_GROUP_ID_INVALID;
    ssg_member_id_t my_id;
    ssg_group_config_t g_conf = SSG_GROUP_CONFIG_INITIALIZER;
    int group_size;
    int ret, sret;
    int self_ssg_rank = 0;

    /* set any default options (that may be overwritten by cmd args) */
    opts.shutdown_time = 10; /* default to running group for 10 seconds */
    opts.group_name = "simple_group";
    opts.gid_file = NULL;

    /* parse cmdline arguments */
    parse_args(argc, argv, &opts);

#ifdef SSG_HAVE_MPI
    int mpi_rank, mpi_size;
    if (strcmp(opts.group_mode, "mpi") == 0)
    {
        MPI_Init(&argc, &argv);
        MPI_Comm_rank(MPI_COMM_WORLD, &mpi_rank);
        MPI_Comm_size(MPI_COMM_WORLD, &mpi_size);
    }
#endif
#ifdef SSG_HAVE_PMIX
    pmix_status_t ret;
    pmix_proc_t proc;
    if (strcmp(opts.group_mode, "pmix") == 0)
    {
        ret = PMIx_Init(&proc, NULL, 0);
        DIE_IF(ret != PMIX_SUCCESS, "PMIx_Init");
    }
#endif

    /* init margo */
    /* use the main xstream to drive progress & run handlers */
    mid = margo_init(opts.addr_str, MARGO_SERVER_MODE, 0, -1);
    DIE_IF(mid == MARGO_INSTANCE_NULL, "margo_init");

    /* initialize SSG */
    sret = ssg_init();
    DIE_IF(sret != SSG_SUCCESS, "ssg_init");

    /* set non-default group config parameters */
    g_conf.swim_period_length_ms = 1000; /* 1-second period length */
    g_conf.swim_suspect_timeout_periods = 4; /* 4-period suspicion length */
    g_conf.swim_subgroup_member_count = 3; /* 3-member subgroups for SWIM */
    //g_conf.swim_disabled = 1;              /* temporarily disable swim for improved stability */

    /* XXX do we want to use callback for testing anything about group??? */
#ifdef SSG_HAVE_MPI
    if(strcmp(opts.group_mode, "mpi") == 0)
        ret = ssg_group_create_mpi(mid, opts.group_name, MPI_COMM_WORLD, &g_conf,
            NULL, NULL, &g_id);
#endif
#ifdef SSG_HAVE_PMIX
    if(strcmp(opts.group_mode, "pmix") == 0)
        ret = ssg_group_create_pmix(mid, opts.group_name, proc, &g_conf,
            NULL, NULL, &g_id);
#endif
    DIE_IF(g_id == SSG_GROUP_ID_INVALID, "ssg_group_create");

    /* store the gid if requested */
    if (opts.gid_file)
        ssg_group_id_store(opts.gid_file, g_id, SSG_ALL_MEMBERS);

    /* sleep a bit for group to stabilize */
    margo_thread_sleep(mid, 5 * 1000.0);

    /* get my group id and the size of the group */
    ret = ssg_get_self_id(mid, &my_id);
    DIE_IF(my_id == SSG_MEMBER_ID_INVALID, "ssg_get_group_self_id");
    ret = ssg_get_group_size(g_id, &group_size);
    DIE_IF(ret != 0, "ssg_get_group_size");

    /* have one process dump group information */
    ret = ssg_get_group_self_rank(g_id, &self_ssg_rank);
    DIE_IF(ret != 0, "ssg_get_group_self_rank");
    if(self_ssg_rank == 0)
        ssg_group_dump(g_id);

    /* sleep for given duration to allow group time to run */
    if (opts.shutdown_time > 0)
        margo_thread_sleep(mid, opts.shutdown_time * 1000.0);

    ssg_group_destroy(g_id);

    /** cleanup **/
    ssg_finalize();
    margo_finalize(mid);
#ifdef SSG_HAVE_MPI
    if (strcmp(opts.group_mode, "mpi") == 0)
        MPI_Finalize();
#endif
#ifdef SSG_HAVE_PMIX
    if (strcmp(opts.group_mode, "pmix") == 0)
        PMIx_Finalize(NULL, 0);
#endif

    return 0;
}

/* File that contains the variable declarations */
#include <stdio.h>
#include "poseidon.h"

/* First function called. It initiallizes all the functions and variables used by AURORA */
void lib_init(int metric, int start_search)
{
        int i;
        int numCores = sysconf(_SC_NPROCESSORS_ONLN);
        /*Initialization of RAPL */
        lib_detect_cpu();
        lib_detect_packages();
        /*End initialization of RAPL */

        int startThreads = numCores;
        /* Initialization of the variables necessary to perform the search algorithm */
        for (i = 0; i < MAX_KERNEL; i++)
        {
                libKernels[i].numThreads = numCores;
                libKernels[i].startThreads = startThreads;
                libKernels[i].numCores = numCores;
                libKernels[i].initResult = 0.0;
                libKernels[i].state = REPEAT;
                libKernels[i].metric = metric;
                libKernels[i].hasSequentialBase = SEQUENTIAL_BASE_NOT_TESTED;
                idKernels[i] = 0;
        }

        /* Start the counters for energy and time for all the application execution */
        id_actual_region = MAX_KERNEL - 1;
        lib_start_rapl_sysfs();
        initGlobalTime = omp_get_wtime();
}

/* It defines the number of threads that will execute the actual parallel region based on the current state of the search algorithm */
int lib_resolve_num_threads(uintptr_t ptr_region)
{
        int i;
        id_actual_region = -1;

        /* Find the actual parallel region */
        for (i = 0; i < totalKernels; i++)
        {
                if (idKernels[i] == ptr_region)
                {
                        id_actual_region = i;
                        break;
                }
        }

        /* If a new parallel region is discovered */
        if (id_actual_region == -1)
        {
                idKernels[totalKernels] = ptr_region;
                id_actual_region = totalKernels;
                totalKernels++;
        }

        /* Check the state of the search algorithm. */
        switch (libKernels[id_actual_region].state)
        {
        case END:
                return libKernels[id_actual_region].bestThread;
        default:
                lib_start_rapl_sysfs();
                libKernels[id_actual_region].initResult = omp_get_wtime();
                return libKernels[id_actual_region].numThreads;
        }
        
}

/* It is responsible for performing the search algorithm */
void lib_end_parallel_region()
{
        double time, energy, result = 0;
        if (libKernels[id_actual_region].state != END)
        {
                /* Check the metric that is being evaluated and collect the results */
                switch (libKernels[id_actual_region].metric)
                {
                case PERFORMANCE:
                        time = omp_get_wtime();
                        result = time - libKernels[id_actual_region].initResult;
                        break;
                case EDP:
                        time = omp_get_wtime() - libKernels[id_actual_region].initResult;
                        energy = lib_end_rapl_sysfs();
                        result = time * energy;
                        /* If the result is negative, it means some problem while reading of the hardware counter. Then, the metric changes to performance */
                        if (result == 0.00000 || result < 0)
                        {
                                libKernels[id_actual_region].state = REPEAT;
                                libKernels[id_actual_region].metric = PERFORMANCE;
                        }
                        break;
                }
                switch (libKernels[id_actual_region].state)
                {
                case REPEAT:
                        libKernels[id_actual_region].state = S0;
                        libKernels[id_actual_region].numThreads = libKernels[id_actual_region].startThreads;
                        libKernels[id_actual_region].lastThread = libKernels[id_actual_region].numThreads;
                        //printf("REPEAT - Região %d, Num Thread %d, Best Thread %d\n", id_actual_region, libKernels[id_actual_region].numThreads, libKernels[id_actual_region].bestThread);
                        break;
                case S0:
                        libKernels[id_actual_region].bestResult = result;
                        libKernels[id_actual_region].bestThread = libKernels[id_actual_region].numThreads;
                        libKernels[id_actual_region].numThreads = libKernels[id_actual_region].bestThread / 2;
                        libKernels[id_actual_region].state = S1;
                        //printf("S0 - Região %d, Num Thread %d, Best Thread %d\n", id_actual_region, libKernels[id_actual_region].numThreads, libKernels[id_actual_region].bestThread);
                        break;
                case S1:
                        if (result < libKernels[id_actual_region].bestResult)
                        { //comparing S0 to REPEAT
                                libKernels[id_actual_region].bestResult = result;
                                libKernels[id_actual_region].bestThread = libKernels[id_actual_region].numThreads;
                                /* if there are opportunities for improvements, then double the number of threads */
                                if (libKernels[id_actual_region].numThreads / 2 > MIN_NUM_THREADS)
                                {
                                        libKernels[id_actual_region].lastThread = libKernels[id_actual_region].numThreads;
                                        libKernels[id_actual_region].numThreads = libKernels[id_actual_region].bestThread / 2;
                                        libKernels[id_actual_region].state = S1;
                                }
                                else
                                {
                                        libKernels[id_actual_region].pass = libKernels[id_actual_region].lastThread / 2;
                                        libKernels[id_actual_region].numThreads = libKernels[id_actual_region].numThreads + libKernels[id_actual_region].pass;
                                        if (libKernels[id_actual_region].pass <= 1)
                                        {
                                                libKernels[id_actual_region].state = S3;
                                        }
                                        else
                                        {
                                                libKernels[id_actual_region].state = S2;
                                        }
                                }
                               
                        }
                        else
                        {
                                if (libKernels[id_actual_region].bestThread == libKernels[id_actual_region].numCores / 2)
                                {
                                        libKernels[id_actual_region].pass = libKernels[id_actual_region].numThreads / 2; 
                                        libKernels[id_actual_region].numThreads = libKernels[id_actual_region].numThreads - libKernels[id_actual_region].pass;  
                                        if (libKernels[id_actual_region].pass <= 1)
                                        {
                                                libKernels[id_actual_region].state = S3;
                                        }
                                        else
                                        {
                                                libKernels[id_actual_region].state = S2;
                                        }
                                }
                                else
                                {
                                        libKernels[id_actual_region].pass = libKernels[id_actual_region].numThreads / 2; 
                                        libKernels[id_actual_region].numThreads = libKernels[id_actual_region].numThreads + libKernels[id_actual_region].pass; 
                                        if (libKernels[id_actual_region].pass == 1)
                                        {
                                                libKernels[id_actual_region].state = S3;
                                        }
                                        else
                                        {
                                                libKernels[id_actual_region].state = S2;
                                        }
                                }
                        }
                        //printf("S1 - Região %d, Num Thread %d, Best Thread %d, Pass %d\n", id_actual_region, libKernels[id_actual_region].numThreads, libKernels[id_actual_region].bestThread, libKernels[id_actual_region].pass);
                        break;
                case S2:
                        if (libKernels[id_actual_region].bestResult < result)
                        {
                                libKernels[id_actual_region].pass = libKernels[id_actual_region].pass / 2;
                                if (libKernels[id_actual_region].numThreads - libKernels[id_actual_region].pass >= MIN_NUM_THREADS)
                                {
                                        libKernels[id_actual_region].numThreads = libKernels[id_actual_region].numThreads - libKernels[id_actual_region].pass;
                                        if (libKernels[id_actual_region].pass <= 1)
                                        {
                                                libKernels[id_actual_region].state = S3;
                                        }
                                        else
                                        {
                                                libKernels[id_actual_region].state = S2;
                                        }
                                } else 
                                {
                                        libKernels[id_actual_region].state = END;
                                }
                        }
                        else
                        {
                                libKernels[id_actual_region].bestThread = libKernels[id_actual_region].numThreads;
                                libKernels[id_actual_region].bestResult = result;
                                libKernels[id_actual_region].pass = libKernels[id_actual_region].pass / 2;
                                libKernels[id_actual_region].numThreads = libKernels[id_actual_region].numThreads + libKernels[id_actual_region].pass;
                                if (libKernels[id_actual_region].pass <= 1)
                                {
                                        libKernels[id_actual_region].state = S3;
                                }
                                else
                                {
                                        libKernels[id_actual_region].state = S2;
                                }
                        }
                        //printf("S2 - Região %d, Num Thread %d, Best Thread %d, Pass %d\n", id_actual_region, libKernels[id_actual_region].numThreads, libKernels[id_actual_region].bestThread, libKernels[id_actual_region].pass);
                        break;
                case S3: /*The last comparison to define the best number of threads*/
                        libKernels[id_actual_region].state = END;
                        if (result < libKernels[id_actual_region].bestResult)
                        {
                                libKernels[id_actual_region].bestThread = libKernels[id_actual_region].numThreads;
                        }
                        //printf("S3 - Região %d, Num Thread %d, Best Thread %d, Pass %d\n", id_actual_region, libKernels[id_actual_region].numThreads, libKernels[id_actual_region].bestThread, libKernels[id_actual_region].pass);
                        break;
                }
        }
}

/* It finalizes the environment of Aurora */
void lib_destructor()
{
        float time = omp_get_wtime() - initGlobalTime;
        id_actual_region = MAX_KERNEL - 1;
        float energy = lib_end_rapl_sysfs();
        float edp = time * energy;
        system("PID_BOOST_TEMP=$(ps -afx | grep boost.sh | awk '{print $1}'); PID_BOOST=$(echo $PID_BOOST_TEMP | awk '{print $4}'); kill -9 $PID_BOOST");
        printf("POSEIDON - Execution Time: %.5f seconds\n", time);
        printf("POSEIDON - Energy: %.5f joules\n", energy);
        printf("POSEIDON - EDP: %.5f\n", edp);
}

/* Function used by the Intel RAPL to detect the CPU Architecture*/
void lib_detect_cpu()
{
        FILE *fff;
        int family, model = -1;
        char buffer[BUFSIZ], *result;
        char vendor[BUFSIZ];
        fff = fopen("/proc/cpuinfo", "r");
        while (1)
        {
                result = fgets(buffer, BUFSIZ, fff);
                if (result == NULL)
                        break;
                if (!strncmp(result, "vendor_id", 8))
                {
                        sscanf(result, "%*s%*s%s", vendor);
                        if (strncmp(vendor, "GenuineIntel", 12))
                        {
                                printf("%s not an Intel chip\n", vendor);
                        }
                }
                if (!strncmp(result, "cpu family", 10))
                {
                        sscanf(result, "%*s%*s%*s%d", &family);
                        if (family != 6)
                        {
                                printf("Wrong CPU family %d\n", family);
                        }
                }
                if (!strncmp(result, "model", 5))
                {
                        sscanf(result, "%*s%*s%d", &model);
                }
        }
        fclose(fff);
}

/* Function used by the Intel RAPL to detect the number of cores and CPU sockets*/
void lib_detect_packages()
{
        char filename[BUFSIZ];
        FILE *fff;
        int package;
        int i;
        for (i = 0; i < MAX_PACKAGES; i++)
                package_map[i] = -1;
        for (i = 0; i < MAX_CPUS; i++)
        {
                sprintf(filename, "/sys/devices/system/cpu/cpu%d/topology/physical_package_id", i);
                fff = fopen(filename, "r");
                if (fff == NULL)
                        break;
                fscanf(fff, "%d", &package);
                fclose(fff);
                if (package_map[package] == -1)
                {
                        total_packages++;
                        package_map[package] = i;
                }
        }
        total_cores = i;
}

/* Function used by the Intel RAPL to store the actual value of the hardware counter*/
void lib_start_rapl_sysfs()
{
        int i, j;
        FILE *fff;
        for (j = 0; j < total_packages; j++)
        {
                i = 0;
                sprintf(packname[j], "/sys/class/powercap/intel-rapl/intel-rapl:%d", j);
                sprintf(tempfile, "%s/name", packname[j]);
                fff = fopen(tempfile, "r");
                if (fff == NULL)
                {
                        fprintf(stderr, "\tCould not open %s\n", tempfile);
                        exit(0);
                }
                fscanf(fff, "%s", event_names[j][i]);
                valid[j][i] = 1;
                fclose(fff);
                sprintf(filenames[j][i], "%s/energy_uj", packname[j]);

                /* Handle subdomains */
                for (i = 1; i < NUM_RAPL_DOMAINS; i++)
                {
                        sprintf(tempfile, "%s/intel-rapl:%d:%d/name", packname[j], j, i - 1);
                        fff = fopen(tempfile, "r");
                        if (fff == NULL)
                        {
                                //fprintf(stderr,"\tCould not open %s\n",tempfile);
                                valid[j][i] = 0;
                                continue;
                        }
                        valid[j][i] = 1;
                        fscanf(fff, "%s", event_names[j][i]);
                        fclose(fff);
                        sprintf(filenames[j][i], "%s/intel-rapl:%d:%d/energy_uj", packname[j], j, i - 1);
                }
        }
        /* Gather before values */
        for (j = 0; j < total_packages; j++)
        {
                for (i = 0; i < NUM_RAPL_DOMAINS; i++)
                {
                        if (valid[j][i])
                        {
                                fff = fopen(filenames[j][i], "r");
                                if (fff == NULL)
                                {
                                        fprintf(stderr, "\tError opening %s!\n", filenames[j][i]);
                                }
                                else
                                {
                                        fscanf(fff, "%lld", &libKernels[id_actual_region].kernelBefore[j][i]);
                                        fclose(fff);
                                }
                        }
                }
        }
}

/* Function used by the Intel RAPL to load the value of the hardware counter and returns the energy consumption*/
double lib_end_rapl_sysfs()
{
        int i, j;
        FILE *fff;
        double total = 0;
        for (j = 0; j < total_packages; j++)
        {
                for (i = 0; i < NUM_RAPL_DOMAINS; i++)
                {
                        if (valid[j][i])
                        {
                                fff = fopen(filenames[j][i], "r");
                                if (fff == NULL)
                                {
                                        fprintf(stderr, "\tError opening %s!\n", filenames[j][i]);
                                }
                                else
                                {
                                        fscanf(fff, "%lld", &libKernels[id_actual_region].kernelAfter[j][i]);
                                        fclose(fff);
                                }
                        }
                }
        }
        for (j = 0; j < total_packages; j++)
        {
                for (i = 0; i < NUM_RAPL_DOMAINS; i++)
                {
                        if (valid[j][i])
                        {
                                if (strcmp(event_names[j][i], "core") != 0 && strcmp(event_names[j][i], "uncore") != 0)
                                {
                                        total += (((double)libKernels[id_actual_region].kernelAfter[j][i] - (double)libKernels[id_actual_region].kernelBefore[j][i]) / 1000000.0);
                                }
                        }
                }
        }
        return total;
}
